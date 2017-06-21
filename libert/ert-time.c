/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "ert-time.h"
#include "ert-log.h"

inline bool ert_timespec_is_zero(struct timespec *ts)
{
  return ts->tv_sec == 0 && ts->tv_nsec == 0;
}

inline bool ert_timespec_is_nonzero(struct timespec *ts)
{
  return !ert_timespec_is_zero(ts);
}

int32_t ert_timespec_diff_milliseconds(struct timespec *start, struct timespec *stop)
{
  return (int32_t) (((((int64_t) stop->tv_sec) * 1000LL) + (((int64_t) stop->tv_nsec) / 1000000LL))
                    - ((((int64_t) start->tv_sec) * 1000LL) + (((int64_t) start->tv_nsec) / 1000000LL)));
}

int32_t ert_timespec_diff_milliseconds_from_current(struct timespec *from)
{
  struct timespec current;

  int result = ert_get_current_timestamp(&current);
  if (result != 0) {
    return 0;
  }

  return ert_timespec_diff_milliseconds(from, &current);
}

int ert_get_current_timestamp(struct timespec *ts)
{
  int result = clock_gettime(CLOCK_REALTIME, ts);
  if (result != 0) {
    ert_log_error("Error getting current time: %s", strerror(errno));
  }

  return result;
}

int ert_get_current_timestamp_offset(struct timespec *ts, uint64_t offset_milliseconds)
{
  int result = ert_get_current_timestamp(ts);
  if (result != 0) {
    return result;
  }

  ts->tv_nsec += (offset_milliseconds % 1000LL) * 1000000LL;
  ts->tv_sec += (offset_milliseconds / 1000LL) + ((ts->tv_nsec >= 1000000000LL) ? 1 : 0);
  ts->tv_nsec = ts->tv_nsec % 1000000000LL;

  return 0;
}

int ert_format_timestamp(struct timespec *ts, const char *format, size_t length, uint8_t *buffer)
{
  struct tm timestamp_tm;

  gmtime_r(&ts->tv_sec, &timestamp_tm);

  strftime((char *) buffer, length, format, &timestamp_tm);

  return 0;
}

int ert_format_iso8601_timestamp(struct timespec *ts, size_t length, uint8_t *buffer)
{
  ert_format_timestamp(ts, "%Y-%m-%dT%H:%M:%S", length, buffer);

  size_t strftime_length = strlen((char *) buffer);
  snprintf((char *) buffer + strftime_length, length - strftime_length, ".%03dZ", (int) ((int64_t) ts->tv_nsec / (int64_t) 1000000));

  return 0;
}

int ert_format_timestamp_current(const char *format, size_t length, uint8_t *buffer)
{
  struct timespec ts;

  int result = clock_gettime(CLOCK_REALTIME, &ts);
  if (result < 0) {
    ert_log_error("Error getting current time: %s", strerror(errno));
    return -EIO;
  }

  return ert_format_timestamp(&ts, format, length, buffer);
}
