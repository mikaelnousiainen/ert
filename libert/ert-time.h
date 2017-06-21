/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_TIME_H
#define __ERT_TIME_H

#include "ert-common.h"

bool ert_timespec_is_zero(struct timespec *ts);
bool ert_timespec_is_nonzero(struct timespec *ts);
int32_t ert_timespec_diff_milliseconds(struct timespec *start, struct timespec *stop);
int32_t ert_timespec_diff_milliseconds_from_current(struct timespec *from);
int ert_get_current_timestamp(struct timespec *ts);
int ert_get_current_timestamp_offset(struct timespec *ts, uint64_t offset_milliseconds);
int ert_format_timestamp(struct timespec *ts, const char *format, size_t length, uint8_t *buffer);
int ert_format_iso8601_timestamp(struct timespec *ts, size_t length, uint8_t *buffer);
int ert_format_timestamp_current(const char *format, size_t length, uint8_t *buffer);

#endif
