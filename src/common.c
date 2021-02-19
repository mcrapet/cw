/*
 * cURL wrapper - commom routines
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "common.h"        /* HAVE_* defines */

#ifdef HAVE_CW_PSELECT
#include <sys/select.h>
#endif
#ifdef HAVE_CW_PPOLL
#include <poll.h>
#endif
#ifdef HAVE_CW_EPOLL
#include <sys/epoll.h>
#endif

#define LINE_BUFFER_SIZE 128 /* cURL seems to have fixed it to 79, but let's be tolerant */
#define WAIT_TIME_SECS    50 /* pselect/ppoll (in seconds) */

typedef struct {
  int in_fd;
  int out_fd;
  int (*cw_parsing_func)(int fd, const char *str, size_t len);
} cw_context_t;

volatile sig_atomic_t exit_request = 0;

/* Signal handler. */
static void signal_handler (int sig)
{
  //CW_WARNING("signal %d received", sig);
  exit_request = (sig == SIGCHLD) ? 1 : -1;
}

/**
 * Parse cURL progress meter.
 *
 * It looks like this:
 *   % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
 *                                  Dload  Upload   Total   Spent    Left  Speed
 *  28 20.0M   28 5936k    0     0  2970k      0  0:00:06  0:00:01  0:00:05 2969k
 *
 * \param[in] fd write output to fd stream descriptor
 * \param[in] str '\0' terminated string
 * \param[in] len string length (strlen, ending '\0' not counted)
 * \return parsed number (percent)
 */
static int parse_curl_progress_meter (int fd, const char *str, size_t len)
{
  char tmp[8];
  int percent, i;

  /* First number is integer (from 0 to 100) */
  strncpy(&tmp[0], str, 4);
  percent = atoi(tmp);

  if (percent > 0) {
    /* Grab last number (speed) */
    i = 1;
    while (i < (int)sizeof(tmp) && str[len - i] != ' ')
      i++;
    memcpy(&tmp[0], &str[len - i + 1], i - 1);
    if (i > 2 && tmp[i-2] == ' ')
      i--;
    tmp[i-1] = '\0';

    dprintf(fd, "%d\n# %d%% (%s/s)\n", percent, percent, tmp);
  }

  return 0;
}

/**
 * Parse cURL progress bar.
 *
 * It looks like this:
 * ################                                                          23,3%
 * ############################################                              61,1%
 * #############################################################             85,9%
 *
 * \param[in] write output (parsed results) to fd stream descriptor
 * \param[in] str '\0' terminated string
 * \param[in] len string length (strlen, ending '\0' not counted)
 * \return parsed (rounded) number (percent)
 */
static int parse_curl_progress_bar (int fd, const char *str, size_t len)
{
  char tmp[8] = {61};
  int percent;

  memcpy(&tmp[0], str + len - 6, 6);
  if (tmp[3] == ',' && tmp[5] == '%') {
    tmp[3] = '\0';
    percent = atoi(tmp);
    dprintf(fd, "%d\n# %d%%\n", percent, percent);
  }

  return 0;
}

/**
 * Read line (seek until delim character).
 *
 * \param[in] buffer input data
 * \param[in,out] length number of bytes of buffer. Decreased by eaten value.
 * \param[out] eaten number of bytes treated (between 1 to length, delim character is included)
 * \param[in] delim Character to search in buffer
 * \return true is delim has been found in buffer
 */
static bool scan_character (char *buffer, size_t *length, size_t *eaten, const char delim)
{
  char *p = buffer;
  size_t sz = *length;

  while (sz > 0 && *p != delim)
    sz--, p++;

  if (sz > 0) {
    *eaten = *length - sz + 1;
    *length = sz - 1;
  } else {
    *eaten = *length;
    *length = 0;
  }

  return (sz > 0);
}

static inline int process_read (cw_context_t *ctx)
{
  ssize_t orig_sz;
  size_t sz, eaten, tmp;
  char *p;

  static char buffer[LINE_BUFFER_SIZE * 2];
  static bool sync = false;                    /* synchronisation character is \r */
  static char stats_buffer[LINE_BUFFER_SIZE];  /* cURL line to analyse */
  static size_t stats_offset = 0;

  orig_sz = read(ctx->in_fd, &buffer[0], sizeof(buffer));
  if (orig_sz == 0)
    return -1;

  sz = (size_t)orig_sz;
  p = &buffer[0];

  while (sz > 0) {
    if (scan_character(p, &sz, &eaten, '\r')) {
      sync = true;
      tmp = eaten;

      if (stats_offset > 0) {

        if ((stats_offset + tmp) > sizeof(stats_buffer)) {
          CW_ERROR("%s: stats_buffer overflow, bytes will be lost", __func__);
          tmp = sizeof(stats_buffer) - stats_offset;
          sync = false;
        }

        if (tmp > 0) {
          memcpy(&stats_buffer[stats_offset], p, tmp - 1); /* don't copy ending \r */
          stats_offset += tmp;
          stats_buffer[stats_offset - 1] = '\0';
          ctx->cw_parsing_func(ctx->out_fd, &stats_buffer[0], stats_offset - 1);
        }

        stats_offset = 0;
      }

    } else if (sync) {
      /* Check for overflow */
      if ((stats_offset + eaten) > sizeof(stats_buffer)) {
        sync = false;
        stats_offset = 0;
      } else {
        memcpy(&stats_buffer[stats_offset], p, eaten);
        stats_offset += eaten;
      }
    }

    p += eaten;
  }

  return 0;
}

#ifdef HAVE_CW_EPOLL
/**
 * Read, parse data and write results.
 * This is a blocking function using epoll (Linux) syscall.
 *
 * \param[in] in_fd input fd to read (raw statistics data) from
 * \param[in] out_fd output fd to write (parsed results) to
 * \param[in] mode use any non zero number when using curl's progress bar (-#)
 * \return <0: for any error
 *          0: success (there's nothing left to read)
 *         >0: SIGCHLD signal received
 */
int cw_filter (int in_fd, int out_fd, int mode)
{
  struct epoll_event ev, events[1];
  int epollfd, n, i, timeout;
  sigset_t mask, orig_mask;
  struct sigaction sa;
  cw_context_t ctx;

  if (in_fd < 0 || out_fd < 0)
    return -1;

  epollfd = epoll_create(1);
  if (epollfd == -1) {
    CW_ERROR_ERRNO(errno, "epoll_create");
    return -2;
  }

  /* Block the 3 signals until ppoll call */
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
    CW_ERROR_ERRNO(errno, "sigprocmask");
    return -3;
  }

  ctx.in_fd = in_fd;
  ctx.out_fd = out_fd;
  ctx.cw_parsing_func = (mode) ? parse_curl_progress_bar :
      parse_curl_progress_meter;

  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask); // signals to be blocked while the handler runs

  if (sigaction(SIGINT, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -4;
  }

  if (sigaction(SIGTERM, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -5;
  }

  if (sigaction(SIGCHLD, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -6;
  }

  timeout = WAIT_TIME_SECS * 1000; /* milliseconds */

  ev.events = EPOLLIN;
  ev.data.fd = ctx.in_fd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctx.in_fd, &ev) == -1) {
    CW_ERROR_ERRNO(errno, "epoll_ctl");
    close(epollfd);
    return -7;
  }

  while (!exit_request) {
    n = epoll_pwait(epollfd, &events[0], sizeof(events)/sizeof(struct epoll_event),
        timeout, &orig_mask);
    if (n == -1 || n > 1) {
      printf("n=%d", n);
      CW_ERROR_ERRNO(errno, "epoll_pwait");
      return -8;
    }
    i = 0;
    if ((events[i].events & EPOLLIN) && (ctx.in_fd == events[i].data.fd) && \
      process_read(&ctx) < 0) {
      break;
    } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
      break;
    }
  }

  close(epollfd);
  return exit_request;
}
#else
#ifdef HAVE_CW_PPOLL
/**
 * Read, parse data and write results.
 * This is a blocking function using ppoll (Linux) syscall.
 *
 * \param[in] in_fd input fd to read (raw statistics data) from
 * \param[in] out_fd output fd to write (parsed results) to
 * \param[in] mode use any non zero number when using curl's progress bar (-#)
 * \return <0: for any error
 *          0: success (there's nothing left to read)
 *         >0: SIGCHLD signal received
 */
int cw_filter (int in_fd, int out_fd, int mode)
{
  struct pollfd readfds[1];
  struct timespec timeout = {0};
  int retval;
  sigset_t mask, orig_mask;
  struct sigaction sa;
  cw_context_t ctx;

  if (in_fd < 0 || out_fd < 0)
    return -1;

  /* Block the 3 signals until ppoll call */
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
    CW_ERROR_ERRNO(errno, "sigprocmask");
    return -2;
  }

  ctx.in_fd = in_fd;
  ctx.out_fd = out_fd;
  ctx.cw_parsing_func = (mode) ? parse_curl_progress_bar :
      parse_curl_progress_meter;

  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask); // signals to be blocked while the handler runs

  if (sigaction(SIGINT, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -3;
  }

  if (sigaction(SIGTERM, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -4;
  }

  if (sigaction(SIGCHLD, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -5;
  }

  timeout.tv_sec = WAIT_TIME_SECS;

  readfds[0].fd = ctx.in_fd;
  readfds[0].events = POLLIN;

  while (!exit_request) {
    retval = ppoll(&readfds[0], sizeof(readfds)/sizeof(struct pollfd),
        &timeout, &orig_mask);
    if (retval < 0) {
      if (errno != EINTR) {
        CW_ERROR_ERRNO(errno, "ppoll");
        return -6;
      }
      break;

    } else if (retval == 0) { /* timeout */
      continue;

    } else if (retval == 1 && readfds[0].revents & POLLHUP) {
      break;

    } else if ((retval == 1 && readfds[0].revents & POLLIN) && \
        process_read(&ctx) < 0) {
      break;
    }
  }

  return exit_request;
}
#else
#ifdef HAVE_CW_PSELECT
/**
 * Read, parse data and write results.
 * This is a blocking function using pselect syscall.
 *
 * \param[in] in_fd input fd to read (raw statistics data) from
 * \param[in] out_fd output fd to write (parsed results) to
 * \param[in] mode use any non zero number when using curl's progress bar (-#)
 * \return <0: for any error
 *          0: success (there's nothing left to read)
 *         >0: SIGCHLD signal received
 */
int cw_filter (int in_fd, int out_fd, int mode)
{
  fd_set readfds;
  struct timespec timeout = {0};
  int maxfd, retval;
  sigset_t mask, orig_mask;
  struct sigaction sa;
  cw_context_t ctx;

  if (in_fd < 0 || out_fd < 0)
    return -1;

  /* Block the 3 signals until pselect call */
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
    CW_ERROR_ERRNO(errno, "sigprocmask");
    return -2;
  }

  ctx.in_fd = in_fd;
  ctx.out_fd = out_fd;
  ctx.cw_parsing_func = (mode) ? parse_curl_progress_bar :
      parse_curl_progress_meter;

  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGINT, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -3;
  }

  if (sigaction(SIGTERM, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -4;
  }

  if (sigaction(SIGCHLD, &sa, NULL)) {
    CW_ERROR_ERRNO(errno, "sigaction");
    return -5;
  }

  timeout.tv_sec = WAIT_TIME_SECS;

  while (!exit_request) {
    FD_ZERO(&readfds);
    FD_SET(ctx.in_fd, &readfds);
    maxfd = ctx.in_fd;

    retval = pselect(maxfd + 1, &readfds, NULL, NULL, &timeout, &orig_mask);
    if (retval < 0) {
      if (errno != EINTR) {
        CW_ERROR_ERRNO(errno, "pselect");
        return -6;
      }
      break;

    } else if (retval == 0) { /* timeout */
      continue;

    } else if (FD_ISSET(ctx.in_fd, &readfds) && \
        process_read(&ctx) < 0) {
      break;
    }
  }

  return exit_request;
}
#endif /* HAVE_CW_PSELECT */
#endif /* HAVE_CW_PPOLL */
#endif /* HAVE_CW_EPOLL */

/* vim: set et sw=2 ts=4: */
