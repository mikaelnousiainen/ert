/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ertgateway.h"
#include "ertgateway-handler-telemetry-node.h"

#define TELEMETRY_DATA_BUFFER_SIZE (16 * 1024)

int ert_gateway_telemetry_parse(ert_gateway *gateway, uint32_t data_length, uint8_t *data)
{
  ert_data_logger_serializer *msgpack_serializer = gateway->msgpack_serializer;
  int result;

  ert_data_logger_entry *entry;
  result = ert_data_logger_serializer_msgpack_deserialize(msgpack_serializer, data_length, data, &entry);
  if (result < 0) {
    ert_log_error("ert_data_logger_serializer_msgpack_deserialize failed with result: %d", result);
    return result;
  }

  ert_data_logger_log_entry(gateway->display_logger, entry);

  int log_result = ert_data_logger_log(gateway->data_logger_node, entry);

  ert_event_emitter_emit(gateway->event_emitter, ERT_EVENT_NODE_TELEMETRY_RECEIVED, entry);

  pthread_mutex_lock(&gateway->related_entry_mutex);
  if (gateway->related_entry != NULL) {
    ert_data_logger_destroy_entry(gateway->related_entry);
    gateway->related_entry = NULL;
  }

  result = ert_data_logger_clone_entry(entry, &gateway->related_entry);
  if (result < 0) {
    ert_log_error("ert_data_logger_clone_entry failed with result: %d", result);
  }
  pthread_mutex_unlock(&gateway->related_entry_mutex);

  ert_data_logger_serializer_msgpack_destroy_entry(gateway->msgpack_serializer, entry);
  if (log_result < 0) {
    ert_log_error("ert_data_logger_log failed with result: %d", result);
    return result;
  }

  return 0;
}

void *ert_gateway_handler_telemetry_node(void *context)
{
  uint32_t buffer_size = TELEMETRY_DATA_BUFFER_SIZE;
  ert_gateway *gateway = (ert_gateway *) context;
  uint8_t buffer[buffer_size];
  int result;

  ert_log_info("Node telemetry handler thread running");

  while (gateway->running) {
    ert_comm_protocol_stream *stream;
    size_t count = ert_pipe_pop(gateway->telemetry_stream_queue, &stream, 1);
    if (count == 0) {
      break;
    }

    uint32_t total_bytes_read = 0;
    result = ert_comm_protocol_receive_buffer(gateway->comm_protocol, stream, buffer_size, buffer, &total_bytes_read, &gateway->running);
    if (result < 0) {
      continue;
    }

    if (total_bytes_read > 0) {
      result = ert_gateway_telemetry_parse(gateway, total_bytes_read, buffer);
      if (result < 0) {
        continue;
      }
    }
  }

  pthread_mutex_lock(&gateway->related_entry_mutex);
  if (gateway->related_entry != NULL) {
    ert_data_logger_destroy_entry(gateway->related_entry);
    gateway->related_entry = NULL;
  }
  pthread_mutex_unlock(&gateway->related_entry_mutex);

  ert_log_info("Node telemetry handler thread stopping");

  return NULL;
}
