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

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef FORCE_IOWAIT
#  if defined(HAVE_PPOLL) && (FORCE_IOWAIT == 0x504F4C4C)
#  define HAVE_CW_PPOLL 1
#  elif defined(HAVE_PSELECT) && (FORCE_IOWAIT == 0x53454C45)
#  define HAVE_CW_PSELECT 1
#  else
#  error "Unavailable implementation, FORCE_IOWAIT has unexpected value"
#  endif
#else
#  if defined(HAVE_PPOLL)
#  define HAVE_CW_PPOLL 1
#  elif defined(HAVE_PSELECT)
#  define HAVE_CW_PSELECT 1
#  endif
#endif

#define CW_WARNING(fmt, ...) \
    fprintf(stderr, "warning: " fmt "\n", ## __VA_ARGS__)
#define CW_ERROR(fmt, ...) \
    fprintf(stderr, "error: " fmt "\n", ## __VA_ARGS__)
#define CW_ERROR_ERRNO(errno, fmt, ...) \
    fprintf(stderr, "error: " fmt " (%s)\n", ## __VA_ARGS__, strerror(errno))

/* Exported prototype */
int cw_filter (int in_fd, int out_fd, int mode);

#endif /* COMMON_H */
