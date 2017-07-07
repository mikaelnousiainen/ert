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

#include "ertnode.h"
#include "ertnode-image.h"
#include "ert-exif.h"

int ert_node_capture_image(const char *command, const char *image_filename,
    int16_t quality, bool hflip, bool vflip, ert_gps_data *gps_data)
{
  size_t args_length = 128;
  char *args_alloc[args_length];
  size_t args_alloc_index = 0;
  const char *args[args_length];
  size_t argument_index = 0;
  size_t exif_argument_length = ERT_EXIF_ENTRY_TAG_LENGTH + ERT_EXIF_ENTRY_VALUE_LENGTH + 32;
  char exif_argument[exif_argument_length];

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

  if (gps_data != NULL) {
    ert_exif_entry *exif_entries;
    int result = ert_exif_gps_create(gps_data, &exif_entries);
    if (result == 0) {
      for (size_t index = 0; strlen(exif_entries[index].tag) > 0; index++) {
        ert_exif_entry *entry = &exif_entries[index];
        snprintf(exif_argument, exif_argument_length, "GPS.%s=%s", entry->tag, entry->value);

        args[argument_index++] = "--exif";

        char *argument_copy = strdup(exif_argument);
        args[argument_index++] = argument_copy;
        args_alloc[args_alloc_index++] = argument_copy;
      }

      free(exif_entries);
    } else {
      ert_log_warn("Error creating EXIF tags for image %s, result %d", image_filename, result);
    }
  }

  args[argument_index++] = "--output";
  args[argument_index++] = image_filename;

  args[argument_index] = NULL;

  int result = ert_process_run_command(command, args);

  for (size_t index = 0; index < args_alloc_index; index++) {
    free(args_alloc[index]);
  }

  return result;
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
