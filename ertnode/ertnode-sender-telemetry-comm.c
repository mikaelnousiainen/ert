/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <errno.h>

#include "ertnode.h"
#include "ert-data-logger-serializer-msgpack.h"
#include "ertnode-sender-telemetry-comm.h"

typedef struct _ert_node_telemetry_sender_comm_context {
  ert_node *node;

  pthread_mutex_t current_entry_mutex;
  volatile bool current_entry_valid;
  ert_data_logger_entry *current_entry;
} ert_node_telemetry_sender_comm_context;

static ert_data_logger_serializer_msgpack_settings *msgpack_telemetry_minimal =
    &msgpack_serializer_settings[ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_MINIMAL];
static ert_data_logger_serializer_msgpack_settings *msgpack_telemetry_full =
    &msgpack_serializer_settings[ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_DATA_WITH_TEXT];

static int ert_node_telemetry_send_comm(ert_node_telemetry_sender_comm_context *sender_comm_context,
    ert_data_logger_serializer_msgpack_settings *msgpack_settings)
{
  ert_node *node = sender_comm_context->node;
  uint32_t data_length;
  uint8_t *data;
  int result;

  pthread_mutex_lock(&sender_comm_context->current_entry_mutex);
  result = ert_data_logger_serializer_msgpack_serialize_with_settings(
      node->msgpack_serializer, msgpack_settings, sender_comm_context->current_entry, &data_length, &data);
  sender_comm_context->current_entry_valid = false;
  pthread_mutex_unlock(&sender_comm_context->current_entry_mutex);
  if (result != 0) {
    ert_log_error("ert_data_logger_serializer_msgpack_serialize failed with result: %d", result);
    return result;
  }

  ert_log_info("Transmitting telemetry data with size of %d bytes ...", data_length);

  result = ert_comm_protocol_transmit_buffer(node->comm_protocol, ERT_STREAM_PORT_TELEMETRY_MSGPACK,
      true, data_length, data);
  free(data);
  if (result < 0) {
    ert_log_error("ert_node_transmit_buffer failed with result: %d", result);
    return result;
  }

  ert_log_info("Transmitted %d bytes of telemetry data successfully", result);

  return 0;
}

static void ert_node_sender_telemetry_comm_telemetry_collected_listener(char *event, void *data, void *context)
{
  ert_node_telemetry_sender_comm_context *sender_comm_context = (ert_node_telemetry_sender_comm_context *) context;
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  pthread_mutex_lock(&sender_comm_context->current_entry_mutex);

  if (sender_comm_context->current_entry != NULL) {
    ert_data_logger_destroy_entry(sender_comm_context->current_entry);
    sender_comm_context->current_entry = NULL;
  }

  int result = ert_data_logger_clone_entry(entry, &sender_comm_context->current_entry);
  if (result == 0) {
    sender_comm_context->current_entry_valid = true;
  } else if (result < 0) {
    ert_log_error("ert_data_logger_clone_entry failed with result: %d", result);
  }

  pthread_mutex_unlock(&sender_comm_context->current_entry_mutex);
}

void *ert_node_telemetry_sender_comm(void *context)
{
  int result;
  ert_node *node = (ert_node *) context;

  ert_node_telemetry_sender_comm_context sender_comm_context = {0};
  sender_comm_context.node = node;
  sender_comm_context.current_entry = NULL;

  size_t telemetry_message_counter = 0;
  result = pthread_mutex_init(&sender_comm_context.current_entry_mutex, NULL);
  if (result != 0) {
    ert_log_fatal("Error initializing data logger current entry mutex, result %d", result);
    return NULL;
  }

  ert_event_emitter_add_listener(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED,
      ert_node_sender_telemetry_comm_telemetry_collected_listener, &sender_comm_context);

  ert_log_info("Telemetry sender thread for comm device running");

  while (node->running) {
    ert_process_sleep_with_interrupt(1, &node->running);

    if (!sender_comm_context.current_entry_valid) {
      continue;
    }

    ert_node_telemetry_send_comm(&sender_comm_context, msgpack_telemetry_full);

    bool transmit_minimal_telemetry =
        (telemetry_message_counter % node->config.sender_telemetry_config.minimal_telemetry_data_send_interval) == 0;
    if (transmit_minimal_telemetry) {
      ert_node_telemetry_send_comm(&sender_comm_context, msgpack_telemetry_minimal);
    }

    telemetry_message_counter++;
  }

  ert_event_emitter_remove_listener(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED,
      ert_node_sender_telemetry_comm_telemetry_collected_listener);

  pthread_mutex_lock(&sender_comm_context.current_entry_mutex);
  if (sender_comm_context.current_entry != NULL) {
    ert_data_logger_destroy_entry(sender_comm_context.current_entry);
    sender_comm_context.current_entry = NULL;
  }
  pthread_mutex_unlock(&sender_comm_context.current_entry_mutex);

  pthread_mutex_destroy(&sender_comm_context.current_entry_mutex);

  ert_log_info("Telemetry sender thread for comm device stopping");

  return NULL;
}
