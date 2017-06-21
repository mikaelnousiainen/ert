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

#include "ertnode.h"
#include "ertnode-image.h"

int ert_node_capture_image(const char *command, const char *image_filename, int16_t quality, bool hflip, bool vflip)
{
  const char *args[64];
  size_t argument_index = 0;

  args[argument_index++] = command;

  args[argument_index++] = "--mode";
  args[argument_index++] = "2";

  args[argument_index++] = "--drc";
  args[argument_index++] = "off";

  if (hflip) {
    args[argument_index++] = "--hflip";
  }
  if (vflip) {
    args[argument_index++] = "--vflip";
  }

  args[argument_index++] = "--awb";
  args[argument_index++] = "auto";

  args[argument_index++] = "--exposure";
  args[argument_index++] = "auto";

  args[argument_index++] = "--stats";

  args[argument_index++] = "--encoding";
  args[argument_index++] = "jpg";

  char quality_string[8];
  if (quality > 0) {
    snprintf(quality_string, 8, "%d", quality);

    args[argument_index++] = "--quality";
    args[argument_index++] = quality_string;
  }

  // TODO: Set EXIF data and GPS coordinates using --exif

  args[argument_index++] = "--output";
  args[argument_index++] = image_filename;

  args[argument_index] = NULL;

  return ert_process_run_command(command, args);
}

int ert_node_resize_image(const char *command, const char *output_image_filename,
    const char *input_image_filename, int16_t width, int16_t height, int16_t quality)
{
  const char *args[64];
  size_t argument_index = 0;

  args[argument_index++] = command;

  char resize_string[32];
  if (width > 0 || height > 0) {
    if (width > 0 && height > 0) {
      snprintf(resize_string, 32, "%dx%d^", width, height);
    } else if (width > 0) {
      snprintf(resize_string, 32, "%d", width);
    }

    args[argument_index++] = "-resize";
    args[argument_index++] = resize_string;
  }

  char quality_string[8];
  if (quality > 0) {
    snprintf(quality_string, 8, "%d", quality);

    args[argument_index++] = "-quality";
    args[argument_index++] = quality_string;
  }

  args[argument_index++] = input_image_filename;

  args[argument_index++] = output_image_filename;

  args[argument_index] = NULL;

  return ert_process_run_command(command, args);
}
