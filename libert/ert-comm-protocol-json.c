/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "ert-comm-protocol-json.h"
#include "ert-log.h"
#include "ert-time.h"

#define jansson_check_result(x) \
  if ((x) < 0) { \
    ert_log_error("JSON serialization failed"); \
    return -EIO; \
  }

static int ert_comm_protocol_stream_info_json_serialize_single(ert_comm_protocol_stream_info *info, json_t *info_obj)
{
  jansson_check_result(json_object_set_new(info_obj, "type",
      json_string(info->type == ERT_COMM_PROTOCOL_STREAM_TYPE_TRANSMIT ? "TRANSMIT" : "RECEIVE")));
  jansson_check_result(json_object_set_new(info_obj, "stream_id", json_integer(info->stream_id)));
  jansson_check_result(json_object_set_new(info_obj, "port", json_integer(info->port)));

  jansson_check_result(json_object_set_new(info_obj, "acks_enabled", json_boolean(info->acks_enabled)));
  jansson_check_result(json_object_set_new(info_obj, "acks", json_boolean(info->acks)));
  jansson_check_result(json_object_set_new(info_obj, "ack_request_pending", json_boolean(info->ack_request_pending)));

  jansson_check_result(json_object_set_new(info_obj, "start_of_stream", json_boolean(info->start_of_stream)));
  jansson_check_result(json_object_set_new(info_obj, "end_of_stream_pending", json_boolean(info->end_of_stream_pending)));
  jansson_check_result(json_object_set_new(info_obj, "end_of_stream", json_boolean(info->end_of_stream)));
  jansson_check_result(json_object_set_new(info_obj, "close_pending", json_boolean(info->close_pending)));
  jansson_check_result(json_object_set_new(info_obj, "failed", json_boolean(info->failed)));

  jansson_check_result(json_object_set_new(info_obj, "current_sequence_number", json_integer(info->current_sequence_number)));
  jansson_check_result(json_object_set_new(info_obj, "last_acknowledged_sequence_number", json_integer(info->last_acknowledged_sequence_number)));
  jansson_check_result(json_object_set_new(info_obj, "last_transferred_sequence_number", json_integer(info->last_transferred_sequence_number)));

  jansson_check_result(json_object_set_new(info_obj, "transferred_packet_count", json_real(info->transferred_packet_count)));
  jansson_check_result(json_object_set_new(info_obj, "transferred_data_bytes", json_real(info->transferred_data_bytes)));
  jansson_check_result(json_object_set_new(info_obj, "transferred_payload_data_bytes", json_real(info->transferred_payload_data_bytes)));
  jansson_check_result(json_object_set_new(info_obj, "duplicate_transferred_packet_count", json_real(info->duplicate_transferred_packet_count)));

  uint8_t timestamp_string[64];
  ert_format_iso8601_timestamp(&info->last_transferred_packet_timestamp, 64, timestamp_string);
  jansson_check_result(json_object_set_new(info_obj, "last_transferred_packet_timestamp", json_string(timestamp_string)));

  jansson_check_result(json_object_set_new(info_obj, "ack_rerequest_count", json_integer(info->ack_rerequest_count)));
  jansson_check_result(json_object_set_new(info_obj, "end_of_stream_ack_rerequest_count", json_integer(info->end_of_stream_ack_rerequest_count)));

  jansson_check_result(json_object_set_new(info_obj, "retransmitted_packet_count", json_real(info->retransmitted_packet_count)));
  jansson_check_result(json_object_set_new(info_obj, "retransmitted_data_bytes", json_real(info->retransmitted_data_bytes)));
  jansson_check_result(json_object_set_new(info_obj, "retransmitted_payload_data_bytes", json_real(info->retransmitted_payload_data_bytes)));

  jansson_check_result(json_object_set_new(info_obj, "received_packet_sequence_number_error_count", json_real(info->received_packet_sequence_number_error_count)));

  return 0;
}

int ert_comm_protocol_stream_info_json_serialize(size_t stream_info_count, ert_comm_protocol_stream_info *stream_info,
    uint32_t *length, uint8_t **data_rcv)
{
  int result;

  json_t *root_obj = json_object();
  if (root_obj == NULL) {
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  json_t *info_array = json_array();
  if (info_array == NULL) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON array");
    return -EIO;
  }

  result = json_object_set_new(root_obj, "streams", info_array);
  if (result < 0) {
    json_decref(info_array);
    json_decref(root_obj);
    ert_log_error("Error setting JSON array");
    return -EIO;
  }

  for (size_t i = 0; i < stream_info_count; i++) {
    ert_comm_protocol_stream_info *info = &stream_info[i];

    json_t *info_obj = json_object();

    result = ert_comm_protocol_stream_info_json_serialize_single(info, info_obj);
    if (result < 0) {
      json_decref(info_obj);
      json_decref(root_obj);
      ert_log_error("Error serializing stream info to JSON, result %d", result);
      return -EIO;
    }

    result = json_array_append_new(info_array, info_obj);
    if (result < 0) {
      json_decref(info_obj);
      json_decref(root_obj);
      ert_log_error("Error adding stream info JSON object to array, result %d", result);
      return -EIO;
    }
  }

  char *data = json_dumps(root_obj, JSON_COMPACT | JSON_PRESERVE_ORDER);
  json_decref(root_obj);

  if (data == NULL) {
    ert_log_error("Error serializing JSON to string");
    return -EIO;
  }

  *data_rcv = (uint8_t *) data;
  *length = (uint32_t) strlen(data);

  return 0;
}
