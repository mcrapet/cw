/*
 * Zenity cURL wrapper
 * Copyright (C) 2016  Matthieu Crapet <mcrapet@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"

//#define CW_KEEP_ZENITY_ERRORS

int main (int argc, char *argv[])
{
  int ret, status = 0;
  pid_t pid[2];
  int apipe[2];
  bool curl_hash_flag, zenity_fork;

  /* Check provided cURL command-line */
  const char *switches[5] = {
    [0] = "-s",
    [1] = "--silent",
    [2] = "-#",
    [3] = "-v",
    [4] = "--verbose"
  };

  if (argc <= 1) {
    fprintf(stderr, "Usage: c2z [curl_options...] URL...\n");
    return 0;
  }

  for (size_t i = 0; i < sizeof(switches)/sizeof(char *); i++)
    for (int j = 1; j < argc; j++)
      if (*argv[j] == '-' && strcmp(argv[j], switches[i]) == 0) {
        status |= 1 << i;
        break;
      }

  /* curl silent flag detected, don't perform 2nd fork */
  zenity_fork = !(status & 3);

  curl_hash_flag = status & 4;

  if (pipe(apipe) == -1) {
    CW_ERROR_ERRNO(errno, "pipe");
    exit(EXIT_FAILURE);
  }

  /* First fork (curl) */
  pid[0] = fork();
  if (pid[0] == (pid_t)-1) {
    CW_ERROR_ERRNO(errno, "fork");
    exit(EXIT_FAILURE);
  }

  if (pid[0] == 0) { /* child */
    int saved_stderr = dup(STDERR_FILENO);

    /* We want to catch statistics data from stderr */
    if (dup2(apipe[1], STDERR_FILENO) == -1) {
      CW_ERROR_ERRNO(errno, "dup2");
    } else {
      close(apipe[0]);
      if (execvp("curl", argv) == -1) {
        close(apipe[1]);
        dup2(saved_stderr, STDERR_FILENO); /* restore stderr */
        CW_ERROR_ERRNO(errno, "execvp curl");
      }
    }

    exit(EXIT_FAILURE);

  } else {
    int bpipe[2];

    if (zenity_fork) {
      if (pipe(bpipe) == -1) {
        CW_ERROR_ERRNO(errno, "pipe");
        exit(EXIT_FAILURE);
      }

      /* Second fork (zenity) */
      pid[1] = fork();
      if (pid[1] == (pid_t)-1) {
        CW_ERROR_ERRNO(errno, "fork");
        exit(EXIT_FAILURE);
      }
    } else {
      bpipe[1] = STDERR_FILENO;
      pid[1] = (pid_t)-1;
    }

    if (pid[1] == 0) { /* child */

      if (dup2(bpipe[0], STDIN_FILENO) == -1) {
        CW_ERROR_ERRNO(errno, "dup2");
      } else {
        close(bpipe[1]);
        close(STDOUT_FILENO);

        /* Silent zenity errors. For example:
         * Gtk-Message: GtkDialog mapped without a transient parent. This is discouraged.
         */
        #ifndef CW_KEEP_ZENITY_ERRORS
        close(STDERR_FILENO);
        #endif

        /* but don't close stderr in case of zenity errors */
        if (execlp("zenity", "zenity", "--progress", "--no-cancel",
              "--auto-close", "--title", "cURL wrapper", NULL) == -1) {
          close(bpipe[0]);
          CW_ERROR_ERRNO(errno, "execlp zenity");
        }
      }

      exit(EXIT_FAILURE);

    } else { /* parent process */
      pid_t w;

      /* Blocking loop inside */
      ret = cw_filter(apipe[0], bpipe[1], curl_hash_flag);

      if (ret > 0 && zenity_fork) { /* SIGCHLD */
        write(bpipe[1], "100\n", 4);
      }

      w = wait(&status);
      if (w == -1) {
        CW_ERROR_ERRNO(errno, "waitpid");
      } else if (WIFEXITED(status)) {
          ret = WEXITSTATUS(status);
          if (ret != 0)
            CW_ERROR("%s exited with status=%d", \
                (w == pid[0]) ? "curl" : "zenity", ret);
      }
    }
  }

  return 0;
}

/* vim: set et sw=2 ts=4: */
