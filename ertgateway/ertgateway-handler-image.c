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
#include <limits.h>
#include <errno.h>

#include "ert-fileutil.h"
#include "ert-time.h"
#include "ert-handler-image-helpers.h"

#include "ertgateway.h"
#include "ertgateway-handler-image.h"

static int ert_gateway_image_handler_read_last_bytes_of_file(const char *filename, uint32_t length, uint8_t *data, uint32_t *bytes_read_rcv) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    ert_log_error("Error opening file '%s' for reading: %s", filename, strerror(errno));
    return -ENOENT;
  }

  off_t file_size = fsize(file);
  if (file_size < 0) {
    ert_log_error("Error seeking to file end: %s", filename);
    return -EIO;
  }


  size_t bytes_read = fgetbr(file, length, (char *) data);
  if (bytes_read == 0) {
    fclose(file);
    ert_log_error("Error reading file: %s", filename);
    return -EIO;
  }

  *bytes_read_rcv = (uint32_t) bytes_read;

  fclose(file);

  return 0;
}

static int ert_gateway_image_handler_parse_image_metadata(const char *filename, ert_image_metadata *image_metadata)
{
  uint8_t image_metadata_search_buffer[ERT_IMAGE_METADATA_SEARCH_BYTE_RANGE];
  uint32_t image_metadata_search_buffer_data_length;

  int result = ert_gateway_image_handler_read_last_bytes_of_file(filename, ERT_IMAGE_METADATA_SEARCH_BYTE_RANGE,
      image_metadata_search_buffer, &image_metadata_search_buffer_data_length);
  if (result < 0) {
    ert_log_error("Error reading file '%s' for searching image metadata, result %d", filename, result);
    return -EINVAL;
  }

  result = ert_image_metadata_msgpack_deserialize_with_header(
      image_metadata_search_buffer_data_length, image_metadata_search_buffer, image_metadata);
  if (result < 0) {
    return result;
  }

  result = ert_image_format_filename(
      image_metadata->format, image_metadata->id, &image_metadata->timestamp, "", image_metadata->filename);
  if (result < 0) {
    ert_log_error("ert_image_format_filename failed with result %d", result);
    return -EIO;
  }

  return 0;
}

void *ert_gateway_image_handler(void *context)
{
  ert_gateway *gateway = (ert_gateway *) context;
  ert_comm_protocol *comm_protocol = gateway->comm_protocol;
  int result;

  ert_log_info("Image handler thread running");

  char image_path[PATH_MAX];
  if (realpath(gateway->config.handler_image_config.image_path, image_path) == NULL) {
    ert_log_error("Error resolving image path: %s", gateway->config.handler_image_config.image_path);
    return NULL;
  }

  char image_filename[PATH_MAX];
  char image_full_path_filename[PATH_MAX];

  while (gateway->running) {
    ert_comm_protocol_stream *stream;

    size_t count = ert_pipe_pop(gateway->image_stream_queue, &stream, 1);
    if (count == 0) {
      break;
    }

    struct timespec image_timestamp;

    result = ert_get_current_timestamp(&image_timestamp);
    if (result < 0) {
      ert_log_error("Error getting current timestamp, result %d", result);
      continue;
    }

    // TODO: add mutex
    uint32_t image_index = gateway->image_index;
    gateway->image_index++;

    result = ert_image_format_filename(
        gateway->config.handler_image_config.image_format, image_index, &image_timestamp, "-local", image_filename);
    if (result < 0) {
      ert_log_error("ert_image_format_filename failed with result %d", result);
      ert_comm_protocol_receive_stream_close(comm_protocol, stream);
      continue;
    }

    ert_image_add_path(image_path, image_filename, image_full_path_filename);

    uint32_t bytes_received = 0;
    result = ert_comm_protocol_receive_file(
        gateway->comm_protocol, stream, image_full_path_filename, true, &gateway->running, &bytes_received);
    if (result < 0) {
      continue;
    }

    if (bytes_received > 0) {
      ert_image_metadata image_metadata;

      result = ert_gateway_image_handler_parse_image_metadata(image_full_path_filename, &image_metadata);
      if (result < 0) {
        ert_log_info("Error parsing image metadata for file '%s', result %d, setting fallback values",
            image_full_path_filename, result);

        image_metadata.id = image_index;
        image_metadata.timestamp = image_timestamp;
        strncpy(image_metadata.filename, image_filename, PATH_MAX);
        strncpy(image_metadata.format, gateway->config.handler_image_config.image_format, 8);
      } else {
        ert_image_add_path(image_path, image_metadata.filename, image_metadata.full_path_filename);

        ssize_t fcopy_result = fcopyn(image_full_path_filename, image_metadata.full_path_filename);
        if (fcopy_result < 0) {
          ert_log_error("Error copying image file '%s' to '%s', result %d",
              image_full_path_filename, image_metadata.full_path_filename, fcopy_result);

          strncpy(image_metadata.filename, image_filename, PATH_MAX);
        } else {
          ert_log_info("Copied file '%s' to '%s' (%d bytes) based on image metadata",
              image_full_path_filename, image_metadata.full_path_filename, (uint32_t) fcopy_result);
        }
      }

      ert_event_emitter_emit(gateway->event_emitter, ERT_EVENT_NODE_IMAGE_RECEIVED, &image_metadata);
    }
  }

  ert_log_info("Image handler thread stopping");

  return NULL;
}
