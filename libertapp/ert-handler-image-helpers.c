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
#include <limits.h>

#include "ert-handler-image-helpers.h"
#include "ert-time.h"
#include "ert-log.h"

const char *image_filename_template = "image-%s-%05d%s.%s";
const char *timestamp_format = "%Y-%m-%dT%H-%M-%SZ";

int ert_image_format_filename(const char *image_format, uint32_t image_index,
    struct timespec *timestamp, char *specifier, char *image_filename_rcv)
{
  size_t timestamp_string_buffer_length = 64;
  uint8_t timestamp_string[timestamp_string_buffer_length];
  timestamp_string[0] = '\0';

  int result = ert_format_timestamp(timestamp, timestamp_format, timestamp_string_buffer_length, timestamp_string);
  if (result < 0) {
    ert_log_error("Error formatting timestamp, result %d", result);
    return result;
  }

  snprintf(image_filename_rcv, PATH_MAX, image_filename_template, timestamp_string, image_index, specifier, image_format);

  return 0;
}

void ert_image_add_path(const char *image_path, const char *image_filename, char *image_full_path_filename_rcv)
{
  snprintf(image_full_path_filename_rcv, PATH_MAX, "%s/%s", image_path, image_filename);
}
