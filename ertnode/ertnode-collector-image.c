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

#include "ert-handler-image-helpers.h"
#include "ertnode.h"
#include "ertnode-image.h"
#include "ertnode-collector-image.h"

typedef struct _ert_node_image_collector_context {
  ert_node *node;

  pthread_mutex_t current_data_mutex;
  volatile bool current_data_valid;
  ert_gps_data current_gps_data;
} ert_node_image_collector_context;

static void ert_node_image_collector_telemetry_collected_listener(char *event, void *data, void *context) {
  ert_node_image_collector_context *image_collector_context = (ert_node_image_collector_context *) context;
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  pthread_mutex_lock(&image_collector_context->current_data_mutex);
  memcpy(&image_collector_context->current_gps_data, &entry->params->gps_data, sizeof(ert_gps_data));
  image_collector_context->current_data_valid = true;
  pthread_mutex_unlock(&image_collector_context->current_data_mutex);
}

void *ert_node_image_collector(void *context)
{
  ert_node *node = (ert_node *) context;
  int result;

  ert_node_image_collector_context image_collector_context = {0};
  image_collector_context.node = node;

  result = pthread_mutex_init(&image_collector_context.current_data_mutex, NULL);
  if (result != 0) {
    ert_log_fatal("Error initializing current data mutex, result %d", result);
    return NULL;
  }

  ert_event_emitter_add_listener(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED,
      ert_node_image_collector_telemetry_collected_listener, &image_collector_context);

  ert_log_info("Image collector thread running");

  char image_path[PATH_MAX];
  if (realpath(node->config.sender_image_config.image_path, image_path) == NULL) {
    ert_log_fatal("Error resolving image path: %s", node->config.sender_image_config.image_path);
    return NULL;
  }

  uint32_t image_index = 0;

  char original_image_filename[PATH_MAX];
  char original_image_full_path_filename[PATH_MAX];
  char transmitted_image_filename[PATH_MAX];
  char transmitted_image_full_path_filename[PATH_MAX];

  ert_process_sleep_with_interrupt(
      node->config.sender_telemetry_config.telemetry_collect_interval_seconds * 2, &node->running);

  while (node->running) {
    struct timespec image_timestamp;

    result = ert_get_current_timestamp(&image_timestamp);
    if (result < 0) {
      ert_log_error("Error getting current timestamp, result %d", result);
      goto error;
    }

    image_index++;

    result = ert_image_format_filename("jpg", image_index, &image_timestamp, "", original_image_filename);
    if (result < 0) {
      ert_log_error("ert_image_format_filename failed with result %d", result);
      continue;
    }
    result = ert_image_format_filename(
        node->config.sender_image_config.transmitted_image_format, image_index, &image_timestamp,
        "-transmit", transmitted_image_filename);
    if (result < 0) {
      ert_log_error("ert_image_format_filename failed with result %d", result);
      continue;
    }

    ert_image_add_path(image_path, original_image_filename, original_image_full_path_filename);
    ert_image_add_path(image_path, transmitted_image_filename, transmitted_image_full_path_filename);

    ert_log_info("Capturing image %d ...", image_index);

    ert_gps_data gps_data;
    ert_gps_data *gps_data_pointer = NULL;

    pthread_mutex_lock(&image_collector_context.current_data_mutex);
    if (image_collector_context.current_data_valid) {
      memcpy(&gps_data, &image_collector_context.current_gps_data, sizeof(ert_gps_data));
      gps_data_pointer = &gps_data;
    }
    pthread_mutex_unlock(&image_collector_context.current_data_mutex);

    result = ert_node_capture_image(
        node->config.sender_image_config.raspistill_command,
        original_image_full_path_filename,
        node->config.sender_image_config.original_image_quality,
        node->config.sender_image_config.horizontal_flip,
        node->config.sender_image_config.vertical_flip,
        gps_data_pointer);
    if (result < 0) {
      ert_log_error("Error capturing image, result %d", result);
      goto error;
    }

    if (!node->running) {
      break;
    }

    ert_log_info("Resizing image %d ...", image_index);

    result = ert_node_resize_image(
        node->config.sender_image_config.convert_command,
        transmitted_image_full_path_filename, original_image_full_path_filename,
        node->config.sender_image_config.transmitted_image_width,
        node->config.sender_image_config.transmitted_image_height,
        node->config.sender_image_config.transmitted_image_quality);
    if (result < 0) {
      ert_log_error("Error resizing image, result %d", result);
      goto error;
    }

    if (!node->running) {
      break;
    }

    ert_image_metadata image_metadata;
    memset(&image_metadata, 0, sizeof(ert_image_metadata));

    image_metadata.id = image_index;
    memcpy(&image_metadata.timestamp, &image_timestamp, sizeof(struct timespec));
    strncpy(image_metadata.format, node->config.sender_image_config.transmitted_image_format, 8);
    strcpy(image_metadata.filename, transmitted_image_filename);
    strcpy(image_metadata.full_path_filename, transmitted_image_full_path_filename);

    bool send_image =
        ((image_index - 1) % node->config.sender_image_config.image_send_interval) == 0;
    if (send_image) {
      ert_event_emitter_emit(node->event_emitter, ERT_EVENT_NODE_IMAGE_CAPTURED, &image_metadata);
    }

    error:
    ert_process_sleep_with_interrupt(
        node->config.sender_image_config.image_capture_interval_seconds, &node->running);
  }

  ert_event_emitter_remove_listener(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED,
      ert_node_image_collector_telemetry_collected_listener);

  ert_log_info("Image collector thread stopping");

  pthread_mutex_destroy(&image_collector_context.current_data_mutex);

  return NULL;
}
