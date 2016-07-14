/*
 * Pipe cURL wrapper
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
#include <unistd.h>
#include <getopt.h>

#include "common.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CW_NAME    "cw"
#define CW_VERSION PACKAGE_VERSION

int main (int argc, char *argv[])
{
  int c, option_index;
  bool curl_hash_flag = false;

  const struct option switches[] = {
    {"version", no_argument, 0, 'v'},
    {"help",    no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  while ((c = getopt_long(argc, argv, "#hv", switches, &option_index)) != -1) {
    switch(c) {
      case 'h':
        fprintf(stdout, "Usage: %s [OPTIONS...]\n"
            "Parse curl's progress meter/bar (stdin => stdout).\n"
            "\nOptions:\n"
            "   -#                     progress bar input data\n"
            "   -h,  --help            display this help and exit\n"
            "        --version         display program version and exit\n",
            CW_NAME);
        return 0;
      case 'v':
        fputs(CW_VERSION "\n", stdout);
        return 0;
      case '#':
        curl_hash_flag = true;
        break;
      default:
        fprintf(stderr, "Try `%s --help' for more information.\n", CW_NAME);
        return -1;
    }
  }

  if (optind < argc)
    fprintf(stderr, "%s: unwanted argument(s), ignoring them\n", CW_NAME);

  /* Blocking loop inside */
  if (cw_filter(STDIN_FILENO, STDOUT_FILENO,
          curl_hash_flag) == 0)
    write(STDOUT_FILENO, "100\n", 4);

  return 0;
}
