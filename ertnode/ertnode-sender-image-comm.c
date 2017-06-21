/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <memory.h>

#include "ertnode.h"
#include "ertnode-sender-image-comm.h"

typedef struct _ert_node_image_sender_comm_context {
  ert_node *node;

  pthread_mutex_t current_image_metadata_mutex;
  ert_image_metadata current_image_metadata;
  volatile bool current_image_metadata_valid;
} ert_node_image_sender_comm_context;

static void ert_node_sender_image_comm_image_captured_listener(char *event, void *data, void *context)
{
  ert_node_image_sender_comm_context *sender_comm_context = (ert_node_image_sender_comm_context *) context;
  ert_image_metadata *image_metadata = (ert_image_metadata *) data;

  pthread_mutex_lock(&sender_comm_context->current_image_metadata_mutex);

  memcpy(&sender_comm_context->current_image_metadata, image_metadata, sizeof(ert_image_metadata));
  sender_comm_context->current_image_metadata_valid = true;

  pthread_mutex_unlock(&sender_comm_context->current_image_metadata_mutex);
}

void *ert_node_sender_image_comm(void *context)
{
  ert_node *node = (ert_node *) context;
  int result;

  ert_node_image_sender_comm_context sender_comm_context = {0};
  sender_comm_context.node = node;

  result = pthread_mutex_init(&sender_comm_context.current_image_metadata_mutex, NULL);
  if (result != 0) {
    ert_log_fatal("Error initializing current image metadata mutex, result %d", result);
    return NULL;
  }

  ert_event_emitter_add_listener(node->event_emitter, ERT_EVENT_NODE_IMAGE_CAPTURED,
      ert_node_sender_image_comm_image_captured_listener, &sender_comm_context);

  ert_log_info("Image sender thread for comm device running");

  while (node->running) {
    ert_process_sleep_with_interrupt(1, &node->running);

    if (!sender_comm_context.current_image_metadata_valid) {
      continue;
    }

    ert_image_metadata image_metadata;

    pthread_mutex_lock(&sender_comm_context.current_image_metadata_mutex);
    memcpy(&image_metadata, &sender_comm_context.current_image_metadata, sizeof(ert_image_metadata));
    sender_comm_context.current_image_metadata_valid = false;
    pthread_mutex_unlock(&sender_comm_context.current_image_metadata_mutex);

    uint32_t image_metadata_serialized_length;
    uint8_t *image_metadata_serialized_data;
    result = ert_image_metadata_msgpack_serialize_with_header(
        &image_metadata, &image_metadata_serialized_length, &image_metadata_serialized_data);
    if (result < 0) {
      ert_log_error("ert_image_metadata_msgpack_serialize_with_header failed with result %d", result);
      continue;
    }

    ert_log_info("Transmitting image %d: %s ...", image_metadata.id, image_metadata.full_path_filename);

    result = ert_comm_protocol_transmit_file_and_buffer(node->comm_protocol, ERT_STREAM_PORT_IMAGE,
        true, image_metadata.full_path_filename, image_metadata_serialized_length, image_metadata_serialized_data,
        &node->running);
    free(image_metadata_serialized_data);
    if (result < 0) {
      ert_log_error("ert_comm_protocol_transmit_file_and_buffer failed with result %d", result);
      continue;
    }

    ert_log_info("Image %d successfully transmitted: %s", image_metadata.id, image_metadata.full_path_filename);
  }

  ert_event_emitter_remove_listener(node->event_emitter, ERT_EVENT_NODE_IMAGE_CAPTURED,
      ert_node_sender_image_comm_image_captured_listener);

  pthread_mutex_destroy(&sender_comm_context.current_image_metadata_mutex);

  ert_log_info("Telemetry sender thread for comm device stopping");

  return NULL;
}
