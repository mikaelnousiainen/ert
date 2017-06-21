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
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <memory.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-comm-protocol.h"

#define ERT_COMM_PROTOCOL_PACKET_IDENTIFIER 0x95

#define ERT_COMM_PROTOCOL_STREAM_PORT_ACKNOWLEDGEMENTS 15

#define ert_comm_protocol_packet_get_port(port_stream_id) ((port_stream_id >> 4) & 0x0F)
#define ert_comm_protocol_packet_get_stream_id(port_stream_id) (port_stream_id & 0x0F)

#define ert_comm_protocol_packet_set_port(port) ((port & 0x0F) << 4)
#define ert_comm_protocol_packet_set_stream_id(stream_id) (stream_id & 0x0F)

#define ERT_COMM_PROTOCOL_SEQUENCE_NUMBER_COUNT 0x100
#define ert_comm_protocol_get_next_sequence_number(current_sequence_number) ((current_sequence_number + 1) % ERT_COMM_PROTOCOL_SEQUENCE_NUMBER_COUNT)

typedef struct _ert_comm_protocol_packet_header {
  uint8_t identifier;
  uint8_t port_stream_id;
  uint8_t sequence_number;
  uint8_t flags;
} __attribute__((packed, aligned(1))) ert_comm_protocol_packet_header;

typedef struct _ert_comm_protocol_packet_acknowledgement {
  uint8_t port_stream_id;
  uint8_t sequence_number;
} __attribute__((packed, aligned(1))) ert_comm_protocol_packet_acknowledgement;

typedef struct _ert_comm_protocol_packet_info {
  uint16_t stream_id;
  uint16_t port;
  uint32_t sequence_number;

  bool start_of_stream;
  bool end_of_stream;
  bool acks_enabled;
  bool request_acks;
  bool retransmit;
  bool acks;

  uint32_t raw_packet_length;
  uint8_t *raw_packet_data;

  uint32_t payload_length;
  uint8_t *payload;
} ert_comm_protocol_packet_info;

struct _ert_comm_protocol_stream {
  ert_comm_protocol_stream_info info;

  volatile bool used;

  pthread_mutex_t operation_mutex;

  pthread_mutex_t mutex;
  pthread_cond_t change_cond;

  ert_ring_buffer *ring_buffer;

  ert_buffer_pool *packet_history_buffer_pool;
  uint32_t acknowledgement_interval_packet_count;
  uint32_t *packet_history_data_length;
  uint8_t **packet_history_pointers;

  ert_comm_protocol_packet_acknowledgement *acknowledgements;
};

struct _ert_comm_protocol {
  ert_comm_protocol_config config;

  ert_comm_protocol_device *protocol_device;

  pthread_mutex_t status_mutex;
  ert_comm_protocol_status status;

  uint32_t max_packet_size;
  uint32_t receive_buffer_length_bytes;
  uint32_t stream_inactivity_timeout_check_millis;

  volatile uint16_t next_transmit_stream_id;
  ert_comm_protocol_stream *transmit_streams;
  pthread_mutex_t transmit_streams_mutex;

  volatile uint16_t next_receive_stream_id;
  ert_comm_protocol_stream *receive_streams;
  pthread_mutex_t receive_streams_mutex;

  ert_comm_protocol_stream_listener_callback stream_listener_callback;
  void *stream_listener_callback_context;

  timer_t acknowledgement_timeout_timer;
  timer_t stream_inactivity_check_timer;
};

typedef enum _ert_comm_protocol_counter_type {
  ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT = 1,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT_DUPLICATE,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_RETRANSMIT,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_DUPLICATE,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_SEQUENCE_NUMBER_ERROR,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_INVALID,
} ert_comm_protocol_counter_type;

static const uint8_t ert_comm_protocol_packet_header_length = sizeof(ert_comm_protocol_packet_header);

static void ert_comm_protocol_log_packet_info_do(ert_log_level level, ert_comm_protocol_packet_info *info, char *format, va_list argp)
{
  char formatted_message[1024];

  vsnprintf(formatted_message, 1024, format, argp);

  ert_log_with_level(level, "%s - packet: stream_id=%d port=%d sequence_number=%d start_of_stream=%d end_of_stream=%d "
      "acks_enabled=%d request_acks=%d retransmit=%d acks=%d raw_packet_length=%d payload_length=%d",
      formatted_message, info->stream_id, info->port, info->sequence_number, info->start_of_stream, info->end_of_stream,
      info->acks_enabled, info->request_acks, info->retransmit, info->acks, info->raw_packet_length,
      info->payload_length);
}

static void ert_comm_protocol_log_packet_info(ert_log_level level, ert_comm_protocol_packet_info *info, char *format, ...)
{
  if (level == ERT_LOG_LEVEL_DEBUG) {
#ifdef ERTLIB_ENABLE_LOG_DEBUG
    va_list argp;
    va_start(argp, format);
    ert_comm_protocol_log_packet_info_do(level, info, format, argp);
    va_end(argp);
#endif
  } else {
    va_list argp;
    va_start(argp, format);
    ert_comm_protocol_log_packet_info_do(level, info, format, argp);
    va_end(argp);
  }
}

static void ert_comm_protocol_log_stream_info_do(ert_log_level level, ert_comm_protocol_stream_info *info, char *format, va_list argp)
{
  char formatted_message[1024];

  vsnprintf(formatted_message, 1024, format, argp);

  ert_log_with_level(level, "%s - stream: stream_id=%d port=%d current_sequence_number=%d last_acknowledged_sequence_number=%d "
      "last_transferred_sequence_number=%d received_packet_sequence_number_error_count=%" PRIu64 " "
      "transferred_packet_count=%" PRIu64 " transferred_data_bytes=%" PRIu64 " "
      "transferred_payload_data_bytes=%" PRIu64 " duplicate_transferred_packet_count=%" PRIu64 " "
      "start_of_stream=%d end_of_stream_pending=%d end_of_stream=%d "
      "close_pending=%d failed=%d acks_enabled=%d acks=%d ack_request_pending=%d "
      "ack_rerequest_count=%d end_of_stream_ack_rerequest_count=%d",
      formatted_message, info->stream_id, info->port, info->current_sequence_number,
      info->last_acknowledged_sequence_number, info->last_transferred_sequence_number,
      info->received_packet_sequence_number_error_count,
      info->transferred_packet_count, info->transferred_data_bytes, info->transferred_payload_data_bytes,
      info->duplicate_transferred_packet_count,  info->start_of_stream, info->end_of_stream_pending,
      info->end_of_stream, info->close_pending, info->failed,
      info->acks_enabled, info->acks, info->ack_request_pending,
      info->ack_rerequest_count, info->end_of_stream_ack_rerequest_count);
}

static void ert_comm_protocol_log_stream_info(ert_log_level level, ert_comm_protocol_stream_info *info, char *format, ...)
{
  if (level == ERT_LOG_LEVEL_DEBUG) {
#ifdef ERTLIB_ENABLE_LOG_DEBUG
    va_list argp;
    va_start(argp, format);
    ert_comm_protocol_log_stream_info_do(level, info, format, argp);
    va_end(argp);
#endif
  } else {
    va_list argp;
    va_start(argp, format);
    ert_comm_protocol_log_stream_info_do(level, info, format, argp);
    va_end(argp);
  }
}

static inline int8_t ert_comm_protocol_stream_calculate_sequence_number_distance(uint32_t sn1, uint32_t sn2)
{
  uint8_t unsigned_distance = (uint8_t) sn1 - (uint8_t) sn2;
  return (int8_t) unsigned_distance;
}

static inline bool ert_comm_protocol_is_acknowledgement_packet(ert_comm_protocol_packet_info *info)
{
  return info->port == ERT_COMM_PROTOCOL_STREAM_PORT_ACKNOWLEDGEMENTS && info->acks;
}

static inline bool ert_comm_protocol_transmit_stream_is_request_acks(ert_comm_protocol_stream *stream)
{
  return stream->info.acks_enabled && !stream->info.start_of_stream
         && ((((stream->info.transferred_packet_count + 1) % stream->acknowledgement_interval_packet_count) == 0) ? true : false);
}

static int ert_comm_protocol_increment_counter(
    ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    ert_comm_protocol_counter_type type, uint64_t packet_data_bytes, uint64_t packet_payload_data_bytes)
{
  int result = 0;

  pthread_mutex_lock(&comm_protocol->status_mutex);

  ert_comm_protocol_status *status = &comm_protocol->status;

  switch (type) {
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT:
      ert_get_current_timestamp(&status->last_transmitted_packet_timestamp);
      status->transmitted_packet_count++;
      status->transmitted_data_bytes += packet_data_bytes;
      status->transmitted_payload_data_bytes += packet_payload_data_bytes;

      memcpy(&stream->info.last_transferred_packet_timestamp,
          &status->last_transmitted_packet_timestamp, sizeof(struct timespec));

      stream->info.transferred_packet_count++;
      stream->info.transferred_data_bytes += packet_data_bytes;
      stream->info.transferred_payload_data_bytes += packet_payload_data_bytes;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_RETRANSMIT:
      ert_get_current_timestamp(&status->last_retransmitted_packet_timestamp);
      status->retransmitted_packet_count++;
      status->retransmitted_data_bytes += packet_data_bytes;
      status->retransmitted_payload_data_bytes += packet_payload_data_bytes;

      memcpy(&stream->info.last_transferred_packet_timestamp,
          &status->last_retransmitted_packet_timestamp, sizeof(struct timespec));

      stream->info.retransmitted_packet_count++;
      stream->info.retransmitted_data_bytes += packet_data_bytes;
      stream->info.retransmitted_payload_data_bytes += packet_payload_data_bytes;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT_DUPLICATE:
      status->duplicate_transmitted_packet_count++;

      stream->info.duplicate_transferred_packet_count++;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE:
      ert_get_current_timestamp(&status->last_received_packet_timestamp);
      status->received_packet_count++;
      status->received_data_bytes += packet_data_bytes;
      status->received_payload_data_bytes += packet_payload_data_bytes;

      memcpy(&stream->info.last_transferred_packet_timestamp,
          &status->last_received_packet_timestamp, sizeof(struct timespec));

      stream->info.transferred_packet_count++;
      stream->info.transferred_data_bytes += packet_data_bytes;
      stream->info.transferred_payload_data_bytes += packet_payload_data_bytes;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_DUPLICATE:
      ert_get_current_timestamp(&status->last_received_packet_timestamp);
      memcpy(&stream->info.last_transferred_packet_timestamp,
          &status->last_received_packet_timestamp, sizeof(struct timespec));

      status->duplicate_received_packet_count++;

      stream->info.duplicate_transferred_packet_count++;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_SEQUENCE_NUMBER_ERROR:
      ert_get_current_timestamp(&status->last_received_packet_timestamp);
      memcpy(&stream->info.last_transferred_packet_timestamp,
          &status->last_received_packet_timestamp, sizeof(struct timespec));

      status->received_packet_sequence_number_error_count++;

      stream->info.received_packet_sequence_number_error_count++;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_INVALID:
      ert_get_current_timestamp(&status->last_invalid_received_packet_timestamp);
      status->invalid_received_packet_count++;
      break;
    default:
      ert_log_error("Invalid counter type: %d", type);
      result = -EINVAL;
      break;
  }

  pthread_mutex_unlock(&comm_protocol->status_mutex);

  return result;
}

static void ert_comm_protocol_log_packet_data(uint32_t length, uint8_t *data)
{
  size_t bytes_per_line = 32;
  char log_text_hex[bytes_per_line * 3 + 1];
  char log_text_raw[bytes_per_line + 1];

  ert_log_info("Logging %d bytes of packet data", length);

  for (size_t data_index = 0; data_index < length;) {
    size_t position = data_index;
    size_t log_text_index = 0;
    for (; (log_text_index < bytes_per_line) && (data_index < length); data_index++, log_text_index++) {
      uint8_t d = data[data_index];
      sprintf(log_text_hex + (log_text_index * 3), "%02X ", d);

      if (d >= 32) {
        log_text_raw[log_text_index] = d;
      } else {
        log_text_raw[log_text_index] = 32;
      }
    }
    log_text_hex[log_text_index * 3] = '\0';
    log_text_raw[log_text_index] = '\0';

    ert_log_info("Packet data at 0x%04X bytes: %s | %s", position, log_text_hex, log_text_raw);
  }
}

static int ert_comm_protocol_get_packet_info(uint32_t length, uint8_t *data, ert_comm_protocol_packet_info *info)
{
  if (length < ert_comm_protocol_packet_header_length) {
    ert_log_error("Invalid packet length, no room for packet header in %d bytes", length);
    ert_comm_protocol_log_packet_data(length, data);
    return -EINVAL;
  }

  ert_comm_protocol_packet_header *header = (ert_comm_protocol_packet_header *) data;

  if (header->identifier != ERT_COMM_PROTOCOL_PACKET_IDENTIFIER) {
    ert_log_error("Unknown comm protocol packet identifier: expected 0x%02X, received 0x%02X, packet length %d bytes",
        ERT_COMM_PROTOCOL_PACKET_IDENTIFIER, header->identifier, length);
    ert_comm_protocol_log_packet_data(length, data);
    return -EINVAL;
  }

  uint32_t payload_length = length - ert_comm_protocol_packet_header_length;

  info->stream_id = (uint16_t) ert_comm_protocol_packet_get_stream_id(header->port_stream_id);
  info->port = (uint16_t) ert_comm_protocol_packet_get_port(header->port_stream_id);
  info->sequence_number = header->sequence_number;
  info->payload_length = payload_length;
  info->payload = data + ert_comm_protocol_packet_header_length;
  info->raw_packet_length = length;
  info->raw_packet_data = data;

  info->start_of_stream = (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_START_OF_STREAM) ? true : false;
  info->end_of_stream = (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_END_OF_STREAM) ? true : false;
  info->acks_enabled = (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS_ENABLED) ? true : false;
  info->request_acks = (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_REQUEST_ACKS) ? true : false;
  info->retransmit = (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_RETRANSMIT) ? true : false;
  info->acks = (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS) ? true : false;

  return 0;
}

static int ert_comm_protocol_stream_packet_history_clear(ert_comm_protocol_stream *stream)
{
  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    stream->packet_history_data_length[i] = 0;
    stream->packet_history_pointers[i] = NULL;
  }

  ert_buffer_pool_clear(stream->packet_history_buffer_pool);

  return 0;
}

static int ert_comm_protocol_stream_packet_history_push(ert_comm_protocol_stream *stream,
    uint32_t length, uint8_t *data)
{
  if (!stream->used) {
    return -EINVAL;
  }

  uint8_t *buffer_pool_pointer;

  int result = ert_buffer_pool_acquire(stream->packet_history_buffer_pool, (void **) &buffer_pool_pointer);
  if (result < 0) {
    return -ENOBUFS;
  }

  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    ert_log_debug("Finding free slot in packet history: index=%d", i);

    if (stream->packet_history_pointers[i] == NULL) {
      ert_log_debug("Pushing to packet history: index=%d", i);
      memcpy(buffer_pool_pointer, data, length);
      stream->packet_history_pointers[i] = buffer_pool_pointer;
      stream->packet_history_data_length[i] = length;
      return 0;
    }
  }

  ert_buffer_pool_release(stream->packet_history_buffer_pool, (void *) buffer_pool_pointer);

  return -ENOBUFS;
}

static bool ert_comm_protocol_stream_packet_history_get(ert_comm_protocol_stream *stream,
    uint16_t port, uint16_t stream_id, uint32_t sequence_number, uint32_t *length_rcv, uint8_t **data_rcv)
{
  if (!stream->used) {
    return false;
  }

  for (ssize_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    if (stream->packet_history_pointers[i] != NULL) {
      ert_comm_protocol_packet_header *header = (ert_comm_protocol_packet_header *) stream->packet_history_pointers[i];

      if (ert_comm_protocol_packet_get_port(header->port_stream_id) == port
          && ert_comm_protocol_packet_get_stream_id(header->port_stream_id) == stream_id
          && header->sequence_number == sequence_number) {
        if (length_rcv != NULL) {
          *length_rcv = stream->packet_history_data_length[i];
        }
        if (data_rcv != NULL) {
          *data_rcv = stream->packet_history_pointers[i];
        }
        return true;
      }
    }
  }

  return false;
}

static size_t ert_comm_protocol_stream_packet_history_get_count(ert_comm_protocol_stream *stream)
{
  if (!stream->used) {
    return 0;
  }

  size_t count = 0;

  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    if (stream->packet_history_pointers[i] != NULL) {
      count++;
    }
  }

  return count;
}

static bool ert_comm_protocol_stream_packet_history_get_index(ert_comm_protocol_stream *stream,
    size_t index, uint32_t *length_rcv, uint8_t **data_rcv)
{
  if (!stream->used || index >= stream->acknowledgement_interval_packet_count) {
    return false;
  }

  if (stream->packet_history_pointers[index] != NULL) {
    if (length_rcv != NULL) {
      *length_rcv = stream->packet_history_data_length[index];
    }
    if (data_rcv != NULL) {
      *data_rcv = stream->packet_history_pointers[index];
    }
    return true;
  }

  return false;
}

static bool ert_comm_protocol_stream_packet_history_get_last(ert_comm_protocol_stream *stream,
    uint32_t *length_rcv, uint8_t **data_rcv)
{
  for (size_t i = stream->acknowledgement_interval_packet_count - 1; i >= 0; i--) {
    bool packet_found = ert_comm_protocol_stream_packet_history_get_index(stream, i, length_rcv, data_rcv);
    if (packet_found) {
      return true;
    }
  }

  return false;
}

static bool ert_comm_protocol_stream_packet_history_pop(ert_comm_protocol_stream *stream,
    uint16_t port, uint16_t stream_id, uint32_t sequence_number)
{
  if (!stream->used) {
    return false;
  }

  ssize_t index = -1;

  for (ssize_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    if (index < 0 && stream->packet_history_pointers[i] != NULL) {
      ert_comm_protocol_packet_header *header = (ert_comm_protocol_packet_header *) stream->packet_history_pointers[i];

      ert_log_debug("Packet in history: stream_id=%d, port=%d, sequence_number=%d",
          ert_comm_protocol_packet_get_stream_id(header->port_stream_id),
          ert_comm_protocol_packet_get_port(header->port_stream_id),
          header->sequence_number);

      if (ert_comm_protocol_packet_get_port(header->port_stream_id) == port
          && ert_comm_protocol_packet_get_stream_id(header->port_stream_id) == stream_id
          && header->sequence_number == sequence_number) {

        ert_log_debug("Popping packet in history: stream_id=%d, port=%d, sequence_number=%d",
            ert_comm_protocol_packet_get_stream_id(header->port_stream_id),
            ert_comm_protocol_packet_get_port(header->port_stream_id),
            header->sequence_number);
        index = i;
        ert_buffer_pool_release(stream->packet_history_buffer_pool, stream->packet_history_pointers[i]);
      }
    }

    if (index >= 0) {
      if (i < (stream->acknowledgement_interval_packet_count - 1)) {
        stream->packet_history_data_length[i] = stream->packet_history_data_length[i + 1];
        stream->packet_history_pointers[i] = stream->packet_history_pointers[i + 1];
      } else {
        stream->packet_history_data_length[i] = 0;
        stream->packet_history_pointers[i] = NULL;
      }
    }
  }

  return (index >= 0) ? true : false;
}

static int ert_comm_protocol_stream_acknowledgements_push(ert_comm_protocol_stream *stream,
    uint16_t port, uint16_t stream_id, uint32_t sequence_number)
{
  if (!stream->used) {
    return -EINVAL;
  }

  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    if (stream->acknowledgements[i].port_stream_id != 0) {
      continue;
    }

    stream->acknowledgements[i].port_stream_id = (uint8_t) (ert_comm_protocol_packet_set_port(port)
        | ert_comm_protocol_packet_set_stream_id(stream_id));
    stream->acknowledgements[i].sequence_number = (uint8_t) sequence_number;
    return 0;
  }

  return -ENOBUFS;
}

static int ert_comm_protocol_stream_acknowledgements_create_payload_and_clear(ert_comm_protocol_stream *stream,
    uint32_t *payload_length_rcv, uint8_t *payload)
{
  if (!stream->used) {
    return -EINVAL;
  }

  uint32_t index = 0;

  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    if (stream->acknowledgements[i].port_stream_id == 0) {
      continue;
    }

    uint32_t offset = index * sizeof(ert_comm_protocol_packet_acknowledgement);
    uint8_t *pointer = payload + offset;

    memcpy(pointer, &stream->acknowledgements[i], sizeof(ert_comm_protocol_packet_acknowledgement));

    index++;

    stream->acknowledgements[i].port_stream_id = 0;
    stream->acknowledgements[i].sequence_number = 0;
  }

  *payload_length_rcv = index * sizeof(ert_comm_protocol_packet_acknowledgement);

  return 0;
}

static int ert_comm_protocol_stream_acknowledgements_clear(ert_comm_protocol_stream *stream)
{
  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    stream->acknowledgements[i].port_stream_id = 0;
    stream->acknowledgements[i].sequence_number = 0;
  }

  return 0;
}

static inline bool ert_comm_protocol_receive_stream_is_end_of_stream(ert_comm_protocol_stream *stream)
{
  return stream->info.end_of_stream_pending
         && (stream->info.current_sequence_number == stream->info.last_acknowledged_sequence_number)
         && (ert_comm_protocol_stream_packet_history_get_count(stream) == 0);
}

static inline bool ert_comm_protocol_transmit_stream_is_end_of_stream(ert_comm_protocol_stream *stream)
{
  return stream->info.end_of_stream_pending
         && (stream->info.last_transferred_sequence_number == stream->info.last_acknowledged_sequence_number)
         && (ert_comm_protocol_stream_packet_history_get_count(stream) == 0);
}

static inline int ert_comm_protocol_stream_update_transferred_packet_timestamp(ert_comm_protocol_stream *stream)
{
  int result = ert_get_current_timestamp(&stream->info.last_transferred_packet_timestamp);
  if (result < 0) {
    ert_log_error("ert_get_current_timestamp failed with result %d: %s", result, strerror(errno));
    return -EIO;
  }

  return 0;
}

void ert_comm_protocol_create_default_config(ert_comm_protocol_config *config)
{
  config->passive_mode = false;
  config->transmit_all_data = false;
  config->ignore_errors = false;
  config->receive_buffer_length_packets = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT * 2;
  config->stream_inactivity_timeout_millis = ERT_COMM_PROTOCOL_STREAM_INACTIVITY_TIMEOUT_MILLIS_DEFAULT;

  config->stream_acknowledgement_interval_packet_count = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT;
  config->stream_acknowledgement_receive_timeout_millis = ERT_COMM_PROTOCOL_STREAM_ACK_RECEIVE_TIMEOUT_MILLIS_DEFAULT;
  config->stream_acknowledgement_guard_interval_millis = ERT_COMM_PROTOCOL_STREAM_ACK_GUARD_INTERVAL_MILLIS_DEFAULT;
  config->stream_acknowledgement_max_rerequest_count = ERT_COMM_PROTOCOL_STREAM_ACK_REREQUEST_COUNT_MAX_DEFAULT;
  config->stream_end_of_stream_acknowledgement_max_rerequest_count = ERT_COMM_PROTOCOL_STREAM_END_OF_STREAM_ACK_REREQUEST_COUNT_MAX_DEFAULT;

  config->transmit_stream_count = ERT_COMM_PROTOCOL_MAX_TRANSMIT_STREAM_COUNT_DEFAULT;
  config->receive_stream_count = ERT_COMM_PROTOCOL_MAX_RECEIVE_STREAM_COUNT_DEFAULT;
}

int ert_comm_protocol_stream_get_info(ert_comm_protocol_stream *stream, ert_comm_protocol_stream_info *stream_info)
{
  if (!stream->used) {
    return -EINVAL;
  }

  memcpy(stream_info, &stream->info, sizeof(ert_comm_protocol_stream_info));

  return 0;
}

static int ert_comm_protocol_stream_reset(ert_comm_protocol_stream *stream, uint16_t stream_id) {
  if (stream->used) {
    return -EINVAL;
  }

  stream->info.stream_id = stream_id;
  stream->used = false;
  stream->info.port = 0;
  stream->info.start_of_stream = false;
  stream->info.end_of_stream = false;
  stream->info.end_of_stream_pending = false;
  stream->info.close_pending = false;
  stream->info.acks_enabled = false;
  stream->info.acks = false;
  stream->info.ack_request_pending = false;
  stream->info.failed = false;
  stream->info.ack_rerequest_count = 0;
  stream->info.end_of_stream_ack_rerequest_count = 0;
  stream->info.current_sequence_number = 0;
  stream->info.last_transferred_sequence_number = 0;
  stream->info.last_acknowledged_sequence_number = 0;
  stream->info.transferred_packet_count = 0;
  stream->info.transferred_data_bytes = 0;
  stream->info.transferred_payload_data_bytes = 0;
  stream->info.duplicate_transferred_packet_count = 0;
  stream->info.retransmitted_packet_count = 0;
  stream->info.retransmitted_data_bytes = 0;
  stream->info.retransmitted_payload_data_bytes = 0;
  stream->info.received_packet_sequence_number_error_count = 0;

  int result = ert_ring_buffer_clear(stream->ring_buffer);
  if (result < 0) {
    return result;
  }
  result = ert_comm_protocol_stream_packet_history_clear(stream);
  if (result < 0) {
    return result;
  }
  result = ert_comm_protocol_stream_acknowledgements_clear(stream);
  if (result < 0) {
    return result;
  }

  if (ert_comm_protocol_stream_update_transferred_packet_timestamp(stream) < 0) {
    return -EIO;
  }

  return 0;
}

static int ert_comm_protocol_stream_retransmit_packet(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t packet_length, uint8_t *packet_data, bool use_acks, bool force_request_acks, bool force_request_acks_if_end_of_stream_pending)
{
  ert_comm_protocol_packet_header *header = (ert_comm_protocol_packet_header *) packet_data;
  uint16_t stream_id = (uint16_t) ert_comm_protocol_packet_get_stream_id(header->port_stream_id);
  uint16_t port = (uint16_t) ert_comm_protocol_packet_get_port(header->port_stream_id);
  uint32_t sequence_number = header->sequence_number;

  header->flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_RETRANSMIT;

  bool request_acks = false;

  if (use_acks) {
    if (force_request_acks) {
      request_acks = true;
    } else if (stream->info.end_of_stream_pending && force_request_acks_if_end_of_stream_pending) {
      request_acks = true;
    } else {
      request_acks = ert_comm_protocol_transmit_stream_is_request_acks(stream);
    }
  }

  if (request_acks) {
    header->flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_REQUEST_ACKS;
    stream->info.ack_request_pending = true;
  } else {
    header->flags &= ~ERT_COMM_PROTOCOL_PACKET_FLAG_REQUEST_ACKS;
  }

  uint32_t write_packet_flags =
      request_acks ? ERT_COMM_PROTOCOL_DEVICE_WRITE_PACKET_FLAG_SET_RECEIVE_ACTIVE : 0;

  ert_log_info("Retransmitting packet: stream_id=%d, port=%d, sequence_number=%d, header_flags=%02X, write_packet_flags=%02X",
      stream_id, port, sequence_number, header->flags, write_packet_flags);

  uint32_t bytes_written = 0;
  int result = comm_protocol->protocol_device->write_packet(comm_protocol->protocol_device,
      packet_length, packet_data, write_packet_flags, &bytes_written);
  if (result < 0) {
    ert_log_error("Protocol device write_packet failed with result %d", result);
    return -EIO;
  }

  result = ert_comm_protocol_stream_update_transferred_packet_timestamp(stream);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_update_transferred_packet_timestamp failed with result %d", result);
    return -EIO;
  }

  ert_log_debug("retransmit: Wrote %d bytes", bytes_written);

  uint32_t payload_bytes_written = bytes_written - ert_comm_protocol_packet_header_length;

  ert_comm_protocol_increment_counter(comm_protocol, stream,
      ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT, bytes_written, payload_bytes_written);
  ert_comm_protocol_increment_counter(comm_protocol, stream,
      ERT_COMM_PROTOCOL_COUNTER_TYPE_RETRANSMIT, bytes_written, payload_bytes_written);

  return request_acks ? 1 : 0;
}

static int ert_comm_protocol_stream_retransmit_packet_history(
    ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream, bool use_acks)
{
  size_t total_packet_count = ert_comm_protocol_stream_packet_history_get_count(stream);

  if (total_packet_count == 0) {
    return 0;
  }

  bool acks_requested = false;
  size_t remaining_packet_count = total_packet_count;
  for (size_t i = 0; i < stream->acknowledgement_interval_packet_count; i++) {
    uint32_t packet_length;
    uint8_t *packet_data;

    bool packet_found = ert_comm_protocol_stream_packet_history_get_index(stream, i, &packet_length, &packet_data);
    if (!packet_found) {
      continue;
    }

    bool request_acks_for_last_packet_if_end_of_stream_pending = (remaining_packet_count == 1);

    int result = ert_comm_protocol_stream_retransmit_packet(comm_protocol, stream, packet_length, packet_data,
        use_acks, false, request_acks_for_last_packet_if_end_of_stream_pending);
    if (result < 0) {
      return result;
    }

    remaining_packet_count--;

    // NOTE: Leave packet to history list until it is acknowledged

    if (result > 0) {
      acks_requested = true;
      // NOTE: Transmit routine starts waiting for acks immediately after transmitting the packet
      break;
    }
  }

  ert_log_info("Packet history retransmitted for stream: stream_id=%d, port=%d",
      stream->info.stream_id, stream->info.port);

  return acks_requested ? 1 : 0;
}

static int ert_comm_protocol_transmit_stream_do_close(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream)
{
  if (!stream->used) {
    return -EINVAL;
  }

  pthread_mutex_lock(&comm_protocol->transmit_streams_mutex);

  ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_INFO, &stream->info, "Closing transmit stream immediately");

  stream->used = false;

  int result = ert_comm_protocol_stream_reset(stream, 0);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_reset failed with result %d", result);
  }

  pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);

  return 0;
}

static int ert_comm_protocol_set_packet_acknowledgement_timeout(ert_comm_protocol *comm_protocol, uint32_t milliseconds)
{
  // Receive mode is requested using write_packet flag

  struct itimerspec ts = {0};

  ts.it_value.tv_sec = ((long) milliseconds / 1000L);
  ts.it_value.tv_nsec = ((long) milliseconds % 1000L) * 1000000L;

  int result = timer_settime(comm_protocol->acknowledgement_timeout_timer, 0, &ts, NULL);
  if (result < 0) {
    comm_protocol->protocol_device->set_receive_active(comm_protocol->protocol_device, false);
    ert_log_error("Error setting acknowledgement timer timeout: %s", strerror(errno));
    return -EIO;
  }

  ert_log_debug("Packet acknowledgement timeout timer set ...");

  return 0;
}

static int ert_comm_protocol_clear_packet_acknowledgement_timeout(ert_comm_protocol *comm_protocol)
{
  struct itimerspec ts = {0};

  int result = timer_settime(comm_protocol->acknowledgement_timeout_timer, 0, &ts, NULL);
  if (result < 0) {
    ert_log_error("Error clearing acknowledgement timer timeout: %s", strerror(errno));
    return -EIO;
  }

  ert_log_debug("Packet acknowledgement timeout timer cleared");

  return 0;
}

static void ert_comm_protocol_packet_acknowledgement_timeout_callback(union sigval sv)
{
  ert_comm_protocol *comm_protocol = (ert_comm_protocol *) sv.sival_ptr;
  int result;

  ert_log_info("Packet acknowledgement timeout reached, disabling reception for comm device");

  result = comm_protocol->protocol_device->set_receive_active(comm_protocol->protocol_device, false);
  if (result < 0) {
    ert_log_error("Error disabling reception for comm protocol device, result %d", result);
  }

  pthread_mutex_lock(&comm_protocol->transmit_streams_mutex);

  for (uint16_t i = 0; i < comm_protocol->config.transmit_stream_count; i++){
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[i];

    if (!stream->used || stream->info.end_of_stream || !stream->info.acks_enabled || !stream->info.ack_request_pending) {
      continue;
    }

    uint32_t packet_length;
    uint8_t *packet_data;

    pthread_mutex_lock(&stream->mutex);
    stream->info.ack_request_pending = false;
    bool packet_found = ert_comm_protocol_stream_packet_history_get_last(stream, &packet_length, &packet_data);
    pthread_mutex_unlock(&stream->mutex);
    if (!packet_found) {
      continue;
    }

    bool max_ack_rerequests_reached = (stream->info.ack_rerequest_count >= comm_protocol->config.stream_acknowledgement_max_rerequest_count)
        || (stream->info.end_of_stream_ack_rerequest_count >= comm_protocol->config.stream_end_of_stream_acknowledgement_max_rerequest_count);

    if (max_ack_rerequests_reached) {
      pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);
      ert_log_warn("Maximum acknowledgement re-request count reached for stream_id=%d port=%d", stream->info.stream_id, stream->info.port);

      if (stream->info.end_of_stream_pending) {
        if (comm_protocol->config.transmit_all_data) {
          ert_log_info("Maximum acknowledgement re-request count reached for stream_id=%d port=%d: closing stream (close pending), but first retransmitting packet history",
              stream->info.stream_id, stream->info.port);

          pthread_mutex_lock(&stream->mutex);

          result = ert_comm_protocol_stream_retransmit_packet_history(comm_protocol, stream, false);
          if (result < 0) {
            ert_log_error("Error retransmitting packet history for stream_id=%d port=%d",
                stream->info.stream_id, stream->info.port);
          }

          pthread_mutex_unlock(&stream->mutex);
        } else {
          ert_log_warn("Maximum acknowledgement re-request count reached for stream_id=%d port=%d: closing stream (close pending)", stream->info.stream_id, stream->info.port);
        }

        result = ert_comm_protocol_transmit_stream_do_close(comm_protocol, stream);
        if (result < 0) {
          ert_log_error("ert_comm_protocol_transmit_stream_do_close failed with result %d", result);
        }
      } else if (comm_protocol->config.transmit_all_data) {
        ert_log_info("Maximum acknowledgement re-request count reached for stream_id=%d port=%d: retransmitting packet history, clearing packet history and continuing",
          stream->info.stream_id, stream->info.port);

        pthread_mutex_lock(&stream->mutex);

        result = ert_comm_protocol_stream_retransmit_packet_history(comm_protocol, stream, false);
        if (result < 0) {
          ert_log_error("Error retransmitting packet history for stream_id=%d port=%d",
              stream->info.stream_id, stream->info.port);
        }

        ert_comm_protocol_stream_packet_history_clear(stream);
        stream->info.last_acknowledged_sequence_number = stream->info.last_transferred_sequence_number;

        stream->info.ack_rerequest_count = 0;
        stream->info.end_of_stream_ack_rerequest_count = 0;

        pthread_mutex_unlock(&stream->mutex);
      } else {
        ert_log_warn("Maximum acknowledgement re-request count reached for stream_id=%d port=%d: stream failed", stream->info.stream_id, stream->info.port);
        stream->info.failed = true;
      }

      return;
    }

    if (stream->info.end_of_stream_pending) {
      stream->info.end_of_stream_ack_rerequest_count++;
    } else {
      stream->info.ack_rerequest_count++;
    }

    pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);

    // Re-request acks for streams with pending end-of-stream so that we can have packet acknowledged and the stream closed
    ert_log_info("Re-requesting acknowledgements for stream_id=%d port=%d", stream->info.stream_id, stream->info.port);

    result = ert_comm_protocol_stream_retransmit_packet(comm_protocol, stream, packet_length, packet_data, true, true, false);
    if (result < 0) {
      ert_log_error("ert_comm_protocol_stream_retransmit_packet failed with result %d", result);
      return;
    }

    result = ert_comm_protocol_set_packet_acknowledgement_timeout(comm_protocol,
        comm_protocol->config.stream_acknowledgement_receive_timeout_millis);
    if (result < 0) {
      ert_log_error("Error setting packet acknowledgement timeout, result %d", result);
      return;
    }

    // TODO: Check if this works properly when multiple streams have packets to be retransmitted

    pthread_mutex_lock(&comm_protocol->transmit_streams_mutex);
  }

  pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);
}

static ert_comm_protocol_stream *ert_comm_protocol_transmit_stream_find(ert_comm_protocol *comm_protocol,
    uint16_t stream_id, uint16_t port)
{
  for (int stream_index = 0; stream_index < comm_protocol->config.transmit_stream_count; stream_index++){
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[stream_index];
    ert_log_debug("find: used=%d end_of_stream=%d stream_id=%d port=%d",
      stream->used, stream->info.end_of_stream, stream->info.stream_id, stream->info.port);
    if (stream->used && !stream->info.end_of_stream && stream->info.stream_id == stream_id && stream->info.port == port) {
      return stream;
    }
  }

  return NULL;
}

static ert_comm_protocol_stream *ert_comm_protocol_receive_stream_find(ert_comm_protocol *comm_protocol,
    uint16_t stream_id, uint16_t port)
{
  for (int stream_index = 0; stream_index < comm_protocol->config.receive_stream_count; stream_index++){
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[stream_index];
    if (stream->used && !stream->info.end_of_stream && stream->info.stream_id == stream_id && stream->info.port == port) {
      return stream;
    }
  }

  return NULL;
}

static ert_comm_protocol_stream *ert_comm_protocol_transmit_stream_find_unused(ert_comm_protocol *comm_protocol)
{
  uint16_t stream_id = comm_protocol->next_transmit_stream_id;

  ert_comm_protocol_stream *stream;
  for (uint16_t i = 0; i < comm_protocol->config.transmit_stream_count; i++) {
    stream = &comm_protocol->transmit_streams[stream_id];
    if (!stream->used) {
      stream->info.stream_id = (uint16_t) stream_id;
      comm_protocol->next_transmit_stream_id = (uint16_t) ((stream_id + 1) % comm_protocol->config.transmit_stream_count);
      return stream;
    }

    stream_id = (uint16_t) ((stream_id + 1) % comm_protocol->config.transmit_stream_count);
  }

  return NULL;
}

static ert_comm_protocol_stream *ert_comm_protocol_receive_stream_find_unused(ert_comm_protocol *comm_protocol)
{
  for (int stream_index = 0; stream_index < comm_protocol->config.receive_stream_count; stream_index++){
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[stream_index];
    if (!stream->used) {
      return stream;
    }
  }

  return NULL;
}

static int ert_comm_protocol_receive_stream_find_or_create(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_packet_info *info,
    ert_comm_protocol_stream **stream_rcv)
{
  pthread_mutex_lock(&comm_protocol->receive_streams_mutex);

  ert_comm_protocol_stream *stream = ert_comm_protocol_receive_stream_find(comm_protocol, info->stream_id, info->port);

  bool new_stream = false;

  if (stream == NULL) {
    if (!comm_protocol->config.passive_mode && !comm_protocol->config.ignore_errors && !info->acks_enabled) {
      pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);

      // Start of stream flag must be set for new streams
      ert_comm_protocol_log_packet_info(ERT_LOG_LEVEL_WARN, info,
          "Invalid packet for new stream: got sequence number %d, but acks are not enabled", info->sequence_number);
      return -EPROTO;
    }

    if (!comm_protocol->config.ignore_errors && info->retransmit) {
      pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);

      // Do not accept retransmitted packets for new streams
      ert_comm_protocol_log_packet_info(ERT_LOG_LEVEL_WARN, info,
          "Invalid packet for new stream: received retransmitted packet");
      return -EPROTO;
    }

    stream = ert_comm_protocol_receive_stream_find_unused(comm_protocol);

    if (stream == NULL) {
      // No free receive streams found
      pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);
      return -EAGAIN;
    }

    pthread_mutex_lock(&stream->mutex);

    int result = ert_comm_protocol_stream_reset(stream, info->stream_id);
    if (result < 0) {
      pthread_mutex_unlock(&stream->mutex);
      pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);
      ert_log_error("ert_comm_protocol_stream_reset failed with result %d", result);
      return -EINVAL;
    }

    stream->used = true;
    stream->info.port = info->port;
    stream->info.acks_enabled = info->acks_enabled;

    pthread_mutex_unlock(&stream->mutex);

    new_stream = true;
  }

  if (info->acks_enabled) {
    stream->info.acks_enabled = true;
  }

  pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);

  *stream_rcv = stream;

  return new_stream ? 1 : 0;
}

static int ert_comm_protocol_receive_stream_put_data_from_packet_history(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, bool flush_until_last_transferred_packet)
{
  bool new_data = false;

  // Check if there are packets (in history list) that can be written to ring buffer
  uint32_t next_acknowledged_sequence_number = ert_comm_protocol_get_next_sequence_number(stream->info.last_acknowledged_sequence_number);
  while (true) {
    uint32_t history_packet_length;
    uint8_t *history_packet_data;

    bool found = ert_comm_protocol_stream_packet_history_get(stream, stream->info.port, stream->info.stream_id,
        next_acknowledged_sequence_number, &history_packet_length, &history_packet_data);

    if (found) {
      uint32_t payload_length = history_packet_length - ert_comm_protocol_packet_header_length;
      uint8_t *payload_data = history_packet_data + ert_comm_protocol_packet_header_length;

      int result = ert_ring_buffer_write(stream->ring_buffer, payload_length, payload_data);
      if (result < 0) {
        ert_log_error("Error writing %d bytes to ring buffer", payload_length);
        return -EIO;
      }
      if (payload_length > 0) {
        new_data = true;
      }

      ert_comm_protocol_stream_packet_history_pop(stream, stream->info.port, stream->info.stream_id,
          next_acknowledged_sequence_number);

      ert_comm_protocol_increment_counter(comm_protocol, stream,
          ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE, history_packet_length, payload_length);
    } else {
      if (flush_until_last_transferred_packet) {
        int8_t signed_distance_latest = ert_comm_protocol_stream_calculate_sequence_number_distance(
            next_acknowledged_sequence_number, stream->info.last_transferred_sequence_number);
        if (signed_distance_latest > 0) {
          break;
        }

        // Write zeroes to ring buffer, do not increment transferred data
        uint32_t packet_length = comm_protocol->max_packet_size;
        int result = ert_ring_buffer_write_value(stream->ring_buffer, packet_length, 0);
        if (result < 0) {
          ert_log_error("Error writing %d bytes to ring buffer", packet_length);
          return -EIO;
        }

        new_data = true;
      } else {
        break;
      }
    }

    stream->info.last_acknowledged_sequence_number = next_acknowledged_sequence_number;

    next_acknowledged_sequence_number = ert_comm_protocol_get_next_sequence_number(stream->info.last_acknowledged_sequence_number);
  }

  return new_data ? 1 : 0;
}

static int ert_comm_protocol_receive_stream_put_data_for_expected_packet(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, ert_comm_protocol_packet_info *info, uint32_t expected_sequence_number)
{
  bool new_data = false;

  int result = ert_ring_buffer_write(stream->ring_buffer, info->payload_length, info->payload);
  if (result < 0) {
    ert_log_error("Error writing %d bytes to ring buffer", info->payload_length);
    return -EIO;
  }
  if (info->payload_length > 0) {
    new_data = true;
  }

  ert_comm_protocol_increment_counter(comm_protocol, stream,
      ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE, info->raw_packet_length, info->payload_length);

  stream->info.current_sequence_number = expected_sequence_number;
  stream->info.last_acknowledged_sequence_number = stream->info.current_sequence_number;

  if (stream->info.acks_enabled && !comm_protocol->config.passive_mode) {
    result = ert_comm_protocol_stream_acknowledgements_push(stream, info->port, info->stream_id, info->sequence_number);
    if (result < 0) {
      ert_log_error("Packet acknowledgement list full");
      return -EAGAIN;
    }
  }

  return new_data ? 1 : 0;
}

static int ert_comm_protocol_receive_stream_put_data_for_retransmitted_packet(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, ert_comm_protocol_packet_info *info)
{
  int result;
  int8_t signed_distance_after_last_accepted = ert_comm_protocol_stream_calculate_sequence_number_distance(
      info->sequence_number, stream->info.last_acknowledged_sequence_number);

  if (signed_distance_after_last_accepted > 0) {
    // Packet sequence number is *after* stream->info.last_acknowledged_sequence_number
    if (ert_comm_protocol_stream_packet_history_get(stream, info->port, info->stream_id,
        info->sequence_number, NULL, NULL)) {
      // Duplicate packet
      ert_comm_protocol_increment_counter(comm_protocol, stream,
          ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_DUPLICATE, info->raw_packet_length, info->payload_length);

      ert_log_debug("Duplicate retransmitted packet");
    } else {
      if (comm_protocol->config.passive_mode) {
        result = ert_comm_protocol_stream_packet_history_push(stream, info->raw_packet_length, info->raw_packet_data);
        if (result < 0) {
          ert_log_warn("Packet history list full, flushing packet history in passive mode");

          // Force flushing of data from packet history
          result = ert_comm_protocol_receive_stream_put_data_from_packet_history(comm_protocol, stream, true);
          if (result < 0) {
            return result;
          }

          // Retry push
          result = ert_comm_protocol_stream_packet_history_push(stream, info->raw_packet_length, info->raw_packet_data);
          if (result < 0) {
            return -EAGAIN;
          }
        }
      } else {
        result = ert_comm_protocol_stream_packet_history_push(stream, info->raw_packet_length, info->raw_packet_data);
        if (result < 0) {
          ert_log_error("Packet history list full");
          return -EAGAIN;
        }

        result = ert_comm_protocol_stream_acknowledgements_push(stream, info->port, info->stream_id,
            info->sequence_number);
        if (result < 0) {
          ert_comm_protocol_stream_packet_history_pop(stream, info->port, info->stream_id, info->sequence_number);
          ert_log_error("Packet acknowledgement list full");
          return -EAGAIN;
        }
      }

      ert_log_debug("Added retransmitted packet to history list");
    }
  } else {
    // Packet already accepted
    ert_comm_protocol_increment_counter(comm_protocol, stream,
        ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_DUPLICATE, info->raw_packet_length, info->payload_length);

    ert_log_debug("Duplicate packet that had already been acknowledged");
  }

  return 0;
}

static int ert_comm_protocol_receive_stream_put_data_for_out_of_order_packet(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, ert_comm_protocol_packet_info *info, uint32_t expected_sequence_number)
{
  int result;

  // Error in transmission, out of order packet received
  int8_t signed_distance = ert_comm_protocol_stream_calculate_sequence_number_distance(
      info->sequence_number, expected_sequence_number);
  int8_t signed_distance_after_last_accepted = ert_comm_protocol_stream_calculate_sequence_number_distance(
      info->sequence_number, stream->info.last_acknowledged_sequence_number);

  if (signed_distance < 0) {
    // Packet from history, consider this as a duplicate packet
    // NOTE: We should not get duplicate packets without retransmit flag, so just log this for now

    ert_comm_protocol_increment_counter(comm_protocol, stream,
        ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_DUPLICATE, info->raw_packet_length, info->payload_length);

    ert_log_warn("Received packet from history without retransmit flag");
  } else {
    if (ert_comm_protocol_stream_packet_history_get(stream, info->port, info->stream_id, info->sequence_number, NULL,
        NULL)) {
      // Duplicate packet
      ert_comm_protocol_increment_counter(comm_protocol, stream,
          ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_DUPLICATE, info->raw_packet_length, info->payload_length);

      if (signed_distance_after_last_accepted > 0) {
        ert_log_debug("Duplicate future packet that had not been acknowledged");
      } else {
        ert_log_debug("Duplicate future packet that had already been acknowledged");
      }
    } else {
      if (comm_protocol->config.passive_mode) {
        result = ert_comm_protocol_stream_packet_history_push(stream, info->raw_packet_length, info->raw_packet_data);
        if (result < 0) {
          ert_log_warn("Packet history list full, flushing packet history in passive mode");

          // Force flushing of data from packet history
          result = ert_comm_protocol_receive_stream_put_data_from_packet_history(comm_protocol, stream, true);
          if (result < 0) {
            return result;
          }

          // Retry push
          result = ert_comm_protocol_stream_packet_history_push(stream, info->raw_packet_length, info->raw_packet_data);
          if (result < 0) {
            return -EAGAIN;
          }
        }
      } else {
        result = ert_comm_protocol_stream_packet_history_push(stream, info->raw_packet_length, info->raw_packet_data);
        if (result < 0) {
          ert_log_error("Packet history list full");
          return -EAGAIN;
        }

        result = ert_comm_protocol_stream_acknowledgements_push(stream, info->port, info->stream_id,
            info->sequence_number);
        if (result < 0) {
          ert_comm_protocol_stream_packet_history_pop(stream, info->port, info->stream_id, info->sequence_number);
          ert_log_error("Packet acknowledgement list full");
          return -EAGAIN;
        }
      }

      ert_log_debug("Added future packet to list buffer");

      // Advanced sequence number to have it point to the larger sequence number received
      int8_t signed_distance_latest = ert_comm_protocol_stream_calculate_sequence_number_distance(
          info->sequence_number, stream->info.current_sequence_number);

      if (signed_distance_latest > 0) {
        stream->info.current_sequence_number = info->sequence_number;
      }
    }
  }

  return 0;
}

// Must be called stream->mutex locked
static int ert_comm_protocol_receive_stream_put_data(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    ert_comm_protocol_packet_info *info)
{
  if (!stream->used) {
    return -EINVAL;
  }
  if (stream->info.end_of_stream) {
    return -EINVAL;
  }

  if (!ert_ring_buffer_has_space_for(stream->ring_buffer, info->payload_length)) {
    ert_log_error("Receive buffer overflow for stream_id=%d port=%d: used %d + received %d",
        stream->info.stream_id, stream->info.port, ert_ring_buffer_get_used_bytes(stream->ring_buffer), info->payload_length);
    return -ENOBUFS;
  }

  int8_t signed_distance_latest = ert_comm_protocol_stream_calculate_sequence_number_distance(
      info->sequence_number, stream->info.last_transferred_sequence_number);

  if (signed_distance_latest > 0) {
    stream->info.last_transferred_sequence_number = info->sequence_number;
  }

  uint32_t expected_sequence_number = ert_comm_protocol_get_next_sequence_number(stream->info.current_sequence_number);
  int result;
  bool new_data = info->end_of_stream;

  if ((stream->info.current_sequence_number == stream->info.last_acknowledged_sequence_number)
    && (info->sequence_number == expected_sequence_number)) {
    result = ert_comm_protocol_receive_stream_put_data_for_expected_packet(
        comm_protocol, stream, info, expected_sequence_number);
    if (result < 0) {
      return result;
    }

    new_data = new_data || (result > 0);
  } else {
    ert_comm_protocol_increment_counter(comm_protocol, stream,
        ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_SEQUENCE_NUMBER_ERROR, info->raw_packet_length, info->payload_length);

    int8_t signed_distance = ert_comm_protocol_stream_calculate_sequence_number_distance(info->sequence_number, expected_sequence_number);

    ert_log_warn("Sequence number incorrect for stream_id=%d port=%d: expected %d, received %d, last acknowledged %d",
        stream->info.stream_id, stream->info.port, expected_sequence_number, info->sequence_number, stream->info.last_acknowledged_sequence_number);
    ert_log_debug("current=%d, expected=%d, last_acknowledged=%d, signed_distance=%d",
        info->sequence_number, expected_sequence_number, stream->info.last_acknowledged_sequence_number, signed_distance);

    if (info->acks_enabled) {
      if (info->retransmit) {
        result = ert_comm_protocol_receive_stream_put_data_for_retransmitted_packet(
            comm_protocol, stream, info);
        if (result < 0) {
          return result;
        }
      } else {
        result = ert_comm_protocol_receive_stream_put_data_for_out_of_order_packet(
            comm_protocol, stream, info, expected_sequence_number);
        if (result < 0) {
          return result;
        }
      }
    } else {
      if (comm_protocol->config.passive_mode || comm_protocol->config.ignore_errors) {
        // If acks have not been enabled, just skip the data and try to continue as if nothing happened
        ert_log_warn("Ignoring sequence number error, acknowledgements not enabled");
      } else {
        ert_log_warn("Error in data transmission: Sequence number error detected, but acknowledgements not enabled");
        return -EPROTO;
      }

      result = ert_ring_buffer_write(stream->ring_buffer, info->payload_length, info->payload);
      if (result < 0) {
        ert_log_error("Error writing %d bytes to ring buffer", info->payload_length);
        return -EIO;
      }
      new_data = new_data || (result > 0);

      ert_comm_protocol_increment_counter(comm_protocol, stream,
          ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE, info->raw_packet_length, info->payload_length);

      stream->info.current_sequence_number = expected_sequence_number;
      stream->info.last_acknowledged_sequence_number = stream->info.current_sequence_number;
    }
  }

  result = ert_comm_protocol_receive_stream_put_data_from_packet_history(comm_protocol, stream, false);
  if (result < 0) {
    return result;
  }
  new_data = new_data || (result > 0);

  if (info->end_of_stream) {
    ert_log_debug("Setting end_of_stream_pending: port=%d, stream_id=%d", info->port, info->stream_id);
    stream->info.end_of_stream_pending = true;

    if (comm_protocol->config.passive_mode) {
      // Force flushing of data from packet history
      result = ert_comm_protocol_receive_stream_put_data_from_packet_history(comm_protocol, stream, true);
      if (result < 0) {
        return result;
      }
    }
  }

  bool is_end_of_stream = ert_comm_protocol_receive_stream_is_end_of_stream(stream);
  size_t history_count = ert_comm_protocol_stream_packet_history_get_count(stream);
  ert_log_debug("End of stream state check for port=%d, stream_id=%d: end_of_stream_pending=%d is_end_of_stream=%d "
      "current_sequence_number=%d last_acknowledged_sequence_number=%d packet_history_count=%d",
      info->port, info->stream_id, stream->info.end_of_stream_pending,
      is_end_of_stream, stream->info.current_sequence_number,
      stream->info.last_acknowledged_sequence_number, history_count);

  if (is_end_of_stream) {
    stream->info.end_of_stream = true;
  }

  ert_log_debug("End of stream state for port=%d, stream_id=%d: end_of_stream=%d", info->port, info->stream_id, stream->info.end_of_stream);

  return new_data ? 1 : 0;
}

static int ert_comm_protocol_stream_send_acknowledgements(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream)
{
  int result;
  uint32_t payload_length;
  uint8_t payload[comm_protocol->max_packet_size];

  ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_INFO, &stream->info, "Sending acks for stream");

  pthread_mutex_lock(&stream->mutex);
  result = ert_comm_protocol_stream_acknowledgements_create_payload_and_clear(stream, &payload_length, payload);
  pthread_mutex_unlock(&stream->mutex);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_acknowledgements_create_payload_and_clear failed with result %d", result);
    return result;
  }

  ert_comm_protocol_stream *ack_stream;
  result = ert_comm_protocol_transmit_stream_open(comm_protocol, ERT_COMM_PROTOCOL_STREAM_PORT_ACKNOWLEDGEMENTS,
      &ack_stream, ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS);
  if (result < 0) {
    ert_log_error("Error opening transmit stream for acknowledgements, result %d", result);
    return result;
  }

  result = comm_protocol->protocol_device->set_receive_active(comm_protocol->protocol_device, false);
  if (result < 0) {
    ert_comm_protocol_transmit_stream_close(comm_protocol, ack_stream, true);
    ert_log_error("Error disabling reception for comm protocol device, result %d", result);
    return -EIO;
  }

  uint32_t bytes_written;
  result = ert_comm_protocol_transmit_stream_write(comm_protocol, ack_stream, payload_length, payload, &bytes_written);
  if (result < 0) {
    ert_log_error("Error writing acknowledgements packet to stream, result %d", result);
  }

  result = ert_comm_protocol_transmit_stream_close(comm_protocol, ack_stream, false);
  comm_protocol->protocol_device->set_receive_active(comm_protocol->protocol_device, true);
  if (result < 0) {
    ert_log_error("Error closing acknowledgements packet stream, result %d", result);
    return -EIO;
  }

  return 0;
}

typedef struct _ert_comm_protocol_acknowledgement_count {
  bool used;
  uint16_t stream_id;
  uint16_t port;
  uint32_t count;
} ert_comm_protocol_acknowledgement_stats;

static void ert_comm_protocol_count_acknowledgement_stats(size_t ack_stats_size,
    ert_comm_protocol_acknowledgement_stats *ack_stats_array,
    ert_comm_protocol_packet_acknowledgement *ack)
{
  uint16_t stream_id = (uint16_t) ert_comm_protocol_packet_get_stream_id(ack->port_stream_id);
  uint16_t port = (uint16_t) ert_comm_protocol_packet_get_port(ack->port_stream_id);
  ert_comm_protocol_acknowledgement_stats *ack_stats_unused = NULL;

  for (size_t i = 0; i < ack_stats_size; i++) {
    ert_comm_protocol_acknowledgement_stats *ack_stats = &ack_stats_array[i];

    if (ack_stats->used) {
      if (ack_stats->stream_id == stream_id && ack_stats->port == port) {
        ack_stats->count++;
        return;
      }
    } else {
      if (ack_stats_unused == NULL) {
        ack_stats_unused = ack_stats;
      }
    }
  }

  if (ack_stats_unused == NULL) {
    ert_log_warn("Acknowledgement count statistics data full");
  } else {
    ack_stats_unused->used = true;
    ack_stats_unused->stream_id = stream_id;
    ack_stats_unused->port = port;
    ack_stats_unused->count = 1;
  }
}

static void ert_comm_protocol_log_acknowledgement_stats(size_t ack_stats_size,
    ert_comm_protocol_acknowledgement_stats *ack_stats_array,
    ert_comm_protocol_packet_info *ack_packet_info)
{
  size_t stats_message_length_remaining = 1024;
  size_t stats_message_length = 0;
  char stats_message[stats_message_length_remaining];

  for (size_t i = 0; i < ack_stats_size; i++) {
    ert_comm_protocol_acknowledgement_stats *ack_stats = &ack_stats_array[i];

    if (!ack_stats->used) {
      continue;
    }

    int len = snprintf(stats_message + stats_message_length, stats_message_length_remaining, "stream_id=%d port=%d count=%d | ",
      ack_stats->stream_id, ack_stats->port, ack_stats->count);

    stats_message_length_remaining -= len;
    stats_message_length += len;
  }


  ert_comm_protocol_log_packet_info(ERT_LOG_LEVEL_INFO, ack_packet_info,
      "Received acknowledgement packet for streams: %s", stats_message);
}

static int ert_comm_protocol_handle_acknowledgement_packet(ert_comm_protocol *comm_protocol, ert_comm_protocol_packet_info *info)
{
  int result;
  size_t ack_stats_size = 16;
  ert_comm_protocol_acknowledgement_stats ack_stats[ack_stats_size];
  size_t ack_size = sizeof(ert_comm_protocol_packet_acknowledgement);
  size_t ack_count = info->payload_length / ack_size;

  memset(ack_stats, 0, ack_stats_size * sizeof(ert_comm_protocol_acknowledgement_stats));

  ert_comm_protocol_stream *ack_streams[comm_protocol->config.transmit_stream_count];
  size_t ack_streams_count = 0;

  ert_comm_protocol_clear_packet_acknowledgement_timeout(comm_protocol);

  for (size_t i = 0; i < ack_count; i++) {
    ert_comm_protocol_packet_acknowledgement *ack = (ert_comm_protocol_packet_acknowledgement *) (info->payload + (i * ack_size));
    uint16_t stream_id = (uint16_t) ert_comm_protocol_packet_get_stream_id(ack->port_stream_id);
    uint16_t port = (uint16_t) ert_comm_protocol_packet_get_port(ack->port_stream_id);

    ert_comm_protocol_count_acknowledgement_stats(ack_stats_size, ack_stats, ack);

    ert_log_debug("Handling acknowledgement for: stream_id=%d, port=%d, sequence_number=%d",
      stream_id, port, ack->sequence_number);

    pthread_mutex_lock(&comm_protocol->transmit_streams_mutex);
    ert_comm_protocol_stream *stream = ert_comm_protocol_transmit_stream_find(comm_protocol, stream_id, port);
    pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);
    if (stream == NULL) {
      ert_log_error("Skipping ack: no transmit stream for stream_id=%d, port=%d", stream_id, port);
      continue;
    }

    pthread_mutex_lock(&stream->mutex);

    stream->info.ack_request_pending = false;

    bool packet_found = ert_comm_protocol_stream_packet_history_pop(stream, port, stream_id, ack->sequence_number);
    if (!packet_found) {
      ert_log_warn("Packet history does not contain packet for acknowledgement: stream_id=%d, port=%d, sequence_number=%d",
        stream_id, port, ack->sequence_number);
    }

    // Advanced last acknowledged sequence number to the latest sequence number
    int8_t signed_distance_latest = ert_comm_protocol_stream_calculate_sequence_number_distance(
        ack->sequence_number, stream->info.last_acknowledged_sequence_number);

    if (signed_distance_latest > 0) {
      stream->info.last_acknowledged_sequence_number = ack->sequence_number;
    }

    pthread_mutex_unlock(&stream->mutex);

    bool ack_stream_not_added = true;
    for (size_t j = 0; j < ack_streams_count; j++) {
      if (ack_streams[j] == stream) {
        ack_stream_not_added = false;
        break;
      }
    }

    if (ack_stream_not_added) {
      ack_streams[ack_streams_count] = stream;
      ack_streams_count++;
    }
  }

  ert_comm_protocol_log_acknowledgement_stats(ack_stats_size, ack_stats, info);

  usleep(comm_protocol->config.stream_acknowledgement_guard_interval_millis * 1000);

  // Switch mode back to transmit *after* updating streams with acknowledgement data
  result = comm_protocol->protocol_device->set_receive_active(comm_protocol->protocol_device, false);
  if (result < 0) {
    ert_log_error("Error disabling reception for comm protocol device, result %d", result);
  }

  ert_log_debug("Acknowledgement packet handled, back to transmit mode");

  // TODO: FIXME: The current implementation retransmits packets for one stream only

  for (size_t i = 0; i < ack_streams_count; i++) {
    ert_comm_protocol_stream *stream = ack_streams[i];

    pthread_mutex_lock(&stream->mutex);

    if (ert_comm_protocol_transmit_stream_is_end_of_stream(stream)) {
      stream->info.end_of_stream = true;
      pthread_mutex_unlock(&stream->mutex);

      ert_log_debug("Ack handler detected end of stream for: stream_id=%d, port=%d",
          stream->info.stream_id, stream->info.port);

      if (stream->info.close_pending) {
        ert_log_debug("Ack handler closing stream: stream_id=%d, port=%d",
            stream->info.stream_id, stream->info.port);

        ert_comm_protocol_transmit_stream_do_close(comm_protocol, stream);
      }
      continue;
    }

    ert_log_debug("Ack handler retransmitting packet history for stream: stream_id=%d, port=%d",
        stream->info.stream_id, stream->info.port);

    result = ert_comm_protocol_stream_retransmit_packet_history(comm_protocol, stream, true);
    pthread_mutex_unlock(&stream->mutex);
    if (result < 0) {
      ert_log_error("ert_comm_protocol_stream_retransmit_packet_history failed with result %d", result);
      return -EIO;
    }

    if (result > 0) {
      // Acks requested
      result = ert_comm_protocol_set_packet_acknowledgement_timeout(comm_protocol,
          comm_protocol->config.stream_acknowledgement_receive_timeout_millis);
      if (result < 0) {
        ert_log_error("Error setting packet acknowledgement timeout, result %d", result);
        return -EIO;
      }

      ert_log_info("Packet history retransmitted and waiting for acks for stream: stream_id=%d, port=%d",
          stream->info.stream_id, stream->info.port);

      return 0;
    }
  }

  return 0;
}

static void ert_comm_protocol_stream_receive_callback(uint32_t length, uint8_t *data, void *callback_context)
{
  ert_comm_protocol *comm_protocol = (ert_comm_protocol *) callback_context;
  ert_comm_protocol_packet_info info;

  int result = ert_comm_protocol_get_packet_info(length, data, &info);
  if (result < 0) {
    ert_comm_protocol_increment_counter(comm_protocol, NULL,
        ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_INVALID, 0, 0);
    return;
  }

  ert_comm_protocol_log_packet_info(ERT_LOG_LEVEL_DEBUG, &info, "Received packet");

  if (info.request_acks) {
    ert_comm_protocol_log_packet_info(ERT_LOG_LEVEL_INFO, &info, "Acknowledgements request received for");
  }

  if (ert_comm_protocol_is_acknowledgement_packet(&info)) {
    if (comm_protocol->config.passive_mode) {
      ert_log_info("Ignoring acknowledgement packet in passive mode");
      return;
    }

    result = ert_comm_protocol_handle_acknowledgement_packet(comm_protocol, &info);
    if (result < 0) {
      ert_log_error("Error handling acknowledgement packet");
    }
    return;
  }

  ert_comm_protocol_stream *stream;
  result = ert_comm_protocol_receive_stream_find_or_create(comm_protocol, &info, &stream);
  if (result < 0) {
    ert_comm_protocol_increment_counter(comm_protocol, stream,
        ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE_INVALID, info.raw_packet_length, info.payload_length);

    if (result == -EPROTO) {
      return;
    }

    ert_comm_protocol_log_packet_info(ERT_LOG_LEVEL_ERROR, &info, "Error finding free receive stream for new packet");
    return;
  }

  if (result > 0) {
    ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_DEBUG, &stream->info, "Allocated new stream");
    comm_protocol->stream_listener_callback(comm_protocol, stream, comm_protocol->stream_listener_callback_context);
  } else {
    ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_DEBUG, &stream->info, "Found existing stream");
  }

  pthread_mutex_lock(&stream->operation_mutex);
  pthread_mutex_lock(&stream->mutex);

  if (!stream->used) {
    pthread_mutex_unlock(&stream->mutex);
    pthread_mutex_unlock(&stream->operation_mutex);
    ert_log_error("Received packet for stream_id=%d port=%d, but stream is already closed", info.stream_id, info.port);
    return;
  }

  result = ert_comm_protocol_stream_update_transferred_packet_timestamp(stream);
  pthread_mutex_unlock(&stream->mutex);
  if (result < 0) {
    pthread_mutex_unlock(&stream->operation_mutex);
    ert_log_error("ert_comm_protocol_stream_update_transferred_packet_timestamp failed with result %d", result);
    return;
  }

  pthread_mutex_lock(&stream->mutex);
  result = ert_comm_protocol_receive_stream_put_data(comm_protocol, stream, &info);
  pthread_mutex_unlock(&stream->mutex);
  if (result == -EAGAIN) {
    // Stream buffers are full, send acks if requested to get retransmissions
  } else if (result < 0) {
    pthread_mutex_unlock(&stream->operation_mutex);
    ert_log_error("Error putting data to receive stream buffer for stream_id=%d port=%d",
        info.stream_id, info.port);
    return;
  }

  bool new_data = (result > 0);

  if (info.request_acks && !comm_protocol->config.passive_mode) {
    usleep(comm_protocol->config.stream_acknowledgement_guard_interval_millis * 1000);

    result = ert_comm_protocol_stream_send_acknowledgements(comm_protocol, stream);
    if (result < 0) {
      ert_log_error("Error sending acknowledgements for stream_id=%d port=%d", info.stream_id, info.port);
    }
  }

  pthread_mutex_unlock(&stream->operation_mutex);

  ert_log_debug("Notify: stream_id=%d port=%d new_data=%d", info.stream_id, info.port, new_data);

  if (new_data) {
    // Notify stream about new data
    pthread_mutex_lock(&stream->mutex);
    pthread_cond_signal(&stream->change_cond);
    pthread_mutex_unlock(&stream->mutex);

    ert_log_debug("Notified: stream_id=%d port=%d new_data=%d", info.stream_id, info.port, new_data);
  }
}

static void ert_comm_protocol_stream_fail_if_inactive(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream)
{
  pthread_mutex_lock(&stream->mutex);
  if (!stream->used) {
    pthread_mutex_unlock(&stream->mutex);
    return;
  }

  char *stream_type = stream->info.type == ERT_COMM_PROTOCOL_STREAM_TYPE_TRANSMIT ? "transmit" : "receive";

  struct timespec current_time;

  int result = ert_get_current_timestamp(&current_time);
  if (result < 0) {
    pthread_mutex_unlock(&stream->mutex);
    ert_log_error("ert_get_current_timestamp failed with result %d: %s", result, strerror(errno));
    return;
  }

  int32_t inactivity_millis = ert_timespec_diff_milliseconds(&stream->info.last_transferred_packet_timestamp, &current_time);
  if (inactivity_millis < 0) {
    ert_log_error("BUG: Inactivity time for %s stream_id=%d port=%d is negative: %d ms",
        stream_type, stream->info.stream_id, stream->info.port, inactivity_millis);
    inactivity_millis = 0;
  }
  if (inactivity_millis < comm_protocol->config.stream_inactivity_timeout_millis) {
    ert_log_debug("Checked %s stream_id=%d port=%d inactivity timeout (%d ms)",
        stream_type, stream->info.stream_id, stream->info.port, inactivity_millis);
    pthread_mutex_unlock(&stream->mutex);
    return;
  }

  if (comm_protocol->config.passive_mode) {
    ert_log_warn("Reached %s stream_id=%d port=%d inactivity timeout (%d ms): flushing packet history in passive mode and setting end of stream",
        stream_type, stream->info.stream_id, stream->info.port, inactivity_millis);

    // Force flushing of data from packet history
    int result = ert_comm_protocol_receive_stream_put_data_from_packet_history(comm_protocol, stream, true);
    if (result < 0) {
      ert_log_error("ert_comm_protocol_receive_stream_put_data_from_packet_history failed with result %d", result);
    }

    stream->info.end_of_stream_pending = true;
    stream->info.end_of_stream = true;

    pthread_cond_signal(&stream->change_cond);
  } else {
    ert_log_warn("Reached %s stream_id=%d port=%d inactivity timeout (%d ms): failing stream",
        stream_type, stream->info.stream_id, stream->info.port, inactivity_millis);

    stream->info.failed = true;
  }

  pthread_mutex_unlock(&stream->mutex);

  if (stream->info.close_pending) {
    ert_log_info("Closing %s stream_id=%d port=%d, because of inactivity timeout and pending close", stream_type, stream->info.stream_id, stream->info.port);
    if (stream->info.type == ERT_COMM_PROTOCOL_STREAM_TYPE_TRANSMIT) {
      ert_comm_protocol_transmit_stream_do_close(comm_protocol, stream);
    } else {
      ert_comm_protocol_receive_stream_close(comm_protocol, stream);
    }
  }
}

static void ert_comm_protocol_stream_inactivity_check_callback(union sigval sv) {
  ert_comm_protocol *comm_protocol = (ert_comm_protocol *) sv.sival_ptr;
  int result;

  ert_log_debug("Stream inactivity check started");

  for (int i = 0; i < comm_protocol->config.receive_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[i];
    ert_comm_protocol_stream_fail_if_inactive(comm_protocol, stream);
  }

  for (int i = 0; i < comm_protocol->config.transmit_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[i];
    ert_comm_protocol_stream_fail_if_inactive(comm_protocol, stream);
  }

  ert_log_debug("Stream inactivity check finished");
}

static int ert_comm_protocol_stream_inactivity_check_start(ert_comm_protocol *comm_protocol)
{
  struct itimerspec ts = {0};

  ts.it_value.tv_sec = ((long) comm_protocol->stream_inactivity_timeout_check_millis / 1000L);
  ts.it_value.tv_nsec = ((long) comm_protocol->stream_inactivity_timeout_check_millis % 1000L) * 1000000L;

  ts.it_interval.tv_sec = ts.it_value.tv_sec;
  ts.it_interval.tv_nsec = ts.it_value.tv_nsec;

  int result = timer_settime(comm_protocol->stream_inactivity_check_timer, 0, &ts, NULL);
  if (result < 0) {
    ert_log_error("Error setting stream inactivity check timer: %s", strerror(errno));
    return -EIO;
  }

  return 0;
}

static int ert_comm_protocol_stream_inactivity_check_stop(ert_comm_protocol *comm_protocol)
{
  struct itimerspec ts = {0};

  int result = timer_settime(comm_protocol->stream_inactivity_check_timer, 0, &ts, NULL);
  if (result < 0) {
    ert_log_error("Error clearing stream inactivity check timer: %s", strerror(errno));
    return -EIO;
  }

  return 0;
}

int ert_comm_protocol_get_status(ert_comm_protocol *comm_protocol, ert_comm_protocol_status *status)
{
  pthread_mutex_lock(&comm_protocol->status_mutex);
  memcpy(status, &comm_protocol->status, sizeof(ert_comm_protocol_status));
  pthread_mutex_unlock(&comm_protocol->status_mutex);

  return 0;
}

int ert_comm_protocol_create(
    ert_comm_protocol_config *config,
    ert_comm_protocol_stream_listener_callback stream_listener_callback,
    void *stream_listener_callback_context,
    ert_comm_protocol_device *protocol_device,
    ert_comm_protocol **comm_protocol_rcv)
{
  int result;

  if (stream_listener_callback == NULL) {
    ert_log_error("Stream listener callback must be specified");
    return -EINVAL;
  }

  ert_comm_protocol *comm_protocol = calloc(1, sizeof(ert_comm_protocol));
  if (comm_protocol == NULL) {
    ert_log_fatal("Error allocating memory for comm protocol struct: %s", strerror(errno));
    return -ENOMEM;
  }

  result = pthread_mutex_init(&comm_protocol->status_mutex, NULL);
  if (result != 0) {
    free(comm_protocol);
    ert_log_error("Error initializing status mutex");
    goto error_comm_protocol;
  }

  result = pthread_mutex_init(&comm_protocol->transmit_streams_mutex, NULL);
  if (result != 0) {
    free(comm_protocol);
    ert_log_error("Error initializing transmit streams mutex");
    goto error_status_mutex;
  }

  result = pthread_mutex_init(&comm_protocol->receive_streams_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing receive streams mutex");
    goto error_transmit_streams_mutex;
  }

  memcpy(&comm_protocol->config, config, sizeof(ert_comm_protocol_config));

  comm_protocol->protocol_device = protocol_device;
  comm_protocol->max_packet_size = protocol_device->get_max_packet_length(protocol_device);
  comm_protocol->stream_inactivity_timeout_check_millis = comm_protocol->config.stream_inactivity_timeout_millis / 4;

  comm_protocol->stream_listener_callback = stream_listener_callback;
  comm_protocol->stream_listener_callback_context = stream_listener_callback_context;

  comm_protocol->receive_buffer_length_bytes =
      comm_protocol->config.receive_buffer_length_packets * comm_protocol->max_packet_size;
  comm_protocol->next_transmit_stream_id = 0;
  comm_protocol->next_receive_stream_id = 0;

  comm_protocol->transmit_streams = calloc(comm_protocol->config.transmit_stream_count, sizeof(ert_comm_protocol_stream));
  if (comm_protocol->transmit_streams == NULL) {
    ert_log_fatal("Error allocating memory for comm protocol transmit streams: %s", strerror(errno));
    result = -ENOMEM;
    goto error_receive_streams_mutex;
  }

  for (uint16_t stream_id = 0; stream_id < comm_protocol->config.transmit_stream_count; stream_id++) {
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[stream_id];
    stream->info.type = ERT_COMM_PROTOCOL_STREAM_TYPE_TRANSMIT;
    stream->acknowledgement_interval_packet_count = comm_protocol->config.stream_acknowledgement_interval_packet_count;

    result = ert_ring_buffer_create(comm_protocol->max_packet_size, &stream->ring_buffer);
    if (result < 0) {
      ert_log_error("Error allocating memory for comm protocol transmit stream buffer");
      goto error_transmit_streams;
    }

    result = ert_buffer_pool_create(comm_protocol->max_packet_size, stream->acknowledgement_interval_packet_count,
        &stream->packet_history_buffer_pool);
    if (result < 0) {
      ert_log_error("Error initializing transmit stream packet history list pool");
      goto error_transmit_streams;
    }

    stream->packet_history_data_length = calloc(
        comm_protocol->config.stream_acknowledgement_interval_packet_count, sizeof(uint32_t));
    if (stream->packet_history_data_length == NULL) {
      ert_log_error("Error allocating memory for comm protocol transmit stream packet history");
      result = -ENOMEM;
      goto error_transmit_streams;
    }

    stream->packet_history_pointers = calloc(
        comm_protocol->config.stream_acknowledgement_interval_packet_count, sizeof(uint8_t *));
    if (stream->packet_history_pointers == NULL) {
      ert_log_error("Error allocating memory for comm protocol transmit stream packet history data pointers");
      result = -ENOMEM;
      goto error_transmit_streams;
    }

    stream->acknowledgements = calloc(comm_protocol->config.stream_acknowledgement_interval_packet_count,
        sizeof(ert_comm_protocol_packet_acknowledgement));
    if (stream->acknowledgements == NULL) {
      ert_log_error("Error allocating memory for comm protocol transmit stream acknowledgements");
      result = -ENOMEM;
      goto error_transmit_streams;
    }
  }

  comm_protocol->receive_streams = calloc(comm_protocol->config.receive_stream_count, sizeof(ert_comm_protocol_stream));
  if (comm_protocol->receive_streams == NULL) {
    ert_log_fatal("Error allocating memory for comm protocol receive streams: %s", strerror(errno));
    result = -ENOMEM;
    goto error_transmit_streams;
  }

  for (uint16_t i = 0; i < comm_protocol->config.receive_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[i];
    stream->info.type = ERT_COMM_PROTOCOL_STREAM_TYPE_RECEIVE;
    stream->acknowledgement_interval_packet_count = comm_protocol->config.stream_acknowledgement_interval_packet_count;

    result = ert_ring_buffer_create(comm_protocol->receive_buffer_length_bytes, &stream->ring_buffer);
    if (result < 0) {
      ert_log_error("Error allocating memory for comm protocol receive stream buffer");
      goto error_receive_streams;
    }

    result = pthread_mutex_init(&stream->operation_mutex, NULL);
    if (result < 0) {
      ert_log_error("Error initializing receive stream operation mutex");
      goto error_receive_streams;
    }

    result = pthread_mutex_init(&stream->mutex, NULL);
    if (result < 0) {
      ert_log_error("Error initializing receive stream mutex");
      goto error_receive_streams;
    }

    result = pthread_cond_init(&stream->change_cond, NULL);
    if (result < 0) {
      ert_log_error("Error initializing receive stream cond");
      goto error_receive_streams;
    }

    result = ert_buffer_pool_create(comm_protocol->max_packet_size, stream->acknowledgement_interval_packet_count,
        &stream->packet_history_buffer_pool);
    if (result < 0) {
      ert_log_error("Error initializing receive stream packet history list pool");
      goto error_receive_streams;
    }

    stream->packet_history_data_length = calloc(
        comm_protocol->config.stream_acknowledgement_interval_packet_count, sizeof(uint32_t));
    if (stream->packet_history_data_length == NULL) {
      ert_log_error("Error allocating memory for comm protocol receive stream packet history");
      result = -ENOMEM;
      goto error_receive_streams;
    }

    stream->packet_history_pointers = calloc(
        comm_protocol->config.stream_acknowledgement_interval_packet_count, sizeof(uint8_t *));
    if (stream->packet_history_pointers == NULL) {
      ert_log_error("Error allocating memory for comm protocol receive stream packet history data pointers");
      result = -ENOMEM;
      goto error_receive_streams;
    }

    stream->acknowledgements = calloc(comm_protocol->config.stream_acknowledgement_interval_packet_count,
        sizeof(ert_comm_protocol_packet_acknowledgement));
    if (stream->acknowledgements == NULL) {
      ert_log_error("Error allocating memory for comm protocol receive stream acknowledgements");
      result = -ENOMEM;
      goto error_receive_streams;
    }
  }

  protocol_device->set_receive_callback(protocol_device, ert_comm_protocol_stream_receive_callback, comm_protocol);

  struct sigevent acknowledgement_timeout_sigev;

  acknowledgement_timeout_sigev.sigev_notify = SIGEV_THREAD;
  acknowledgement_timeout_sigev.sigev_notify_function = ert_comm_protocol_packet_acknowledgement_timeout_callback;
  acknowledgement_timeout_sigev.sigev_notify_attributes = NULL;
  acknowledgement_timeout_sigev.sigev_value.sival_ptr = comm_protocol;

  result = timer_create(CLOCK_REALTIME, &acknowledgement_timeout_sigev, &comm_protocol->acknowledgement_timeout_timer);
  if (result < 0) {
    ert_log_error("Error creating acknowledgement timeout timer: %s", strerror(errno));
    goto error_receive_streams;
  }

  struct sigevent stream_inactivity_check_sigev;

  stream_inactivity_check_sigev.sigev_notify = SIGEV_THREAD;
  stream_inactivity_check_sigev.sigev_notify_function = ert_comm_protocol_stream_inactivity_check_callback;
  stream_inactivity_check_sigev.sigev_notify_attributes = NULL;
  stream_inactivity_check_sigev.sigev_value.sival_ptr = comm_protocol;

  result = timer_create(CLOCK_REALTIME, &stream_inactivity_check_sigev, &comm_protocol->stream_inactivity_check_timer);
  if (result < 0) {
    ert_log_error("Error creating stream inactivity check timer: %s", strerror(errno));
    goto error_acknowledgement_timeout_timer;
  }

  result = ert_comm_protocol_stream_inactivity_check_start(comm_protocol);
  if (result < 0) {
    ert_log_error("Error starting stream inactivity check timer");
    goto error_inactivity_check_timer;
  }

  *comm_protocol_rcv = comm_protocol;

  return 0;

  int error_result;

  error_acknowledgement_timeout_timer:
  error_result = timer_delete(comm_protocol->acknowledgement_timeout_timer);
  if (error_result < 0) {
    ert_log_error("Error deleting acknowledgement timeout timer: %s", strerror(errno));
  }

  error_inactivity_check_timer:
  error_result = timer_delete(comm_protocol->stream_inactivity_check_timer);
  if (error_result < 0) {
    ert_log_error("Error deleting stream inactivity check timer: %s", strerror(errno));
  }

  error_receive_streams:
  for (uint16_t i = 0; i < comm_protocol->config.receive_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[i];
    pthread_cond_destroy(&stream->change_cond);
    pthread_mutex_destroy(&stream->mutex);
    pthread_mutex_destroy(&stream->operation_mutex);
    if (stream->acknowledgements != NULL) {
      free(stream->acknowledgements);
    }
    if (stream->packet_history_pointers != NULL) {
      free(stream->packet_history_pointers);
    }
    if (stream->packet_history_data_length != NULL) {
      free(stream->packet_history_data_length);
    }
    if (stream->packet_history_buffer_pool != NULL) {
      ert_buffer_pool_destroy(stream->packet_history_buffer_pool);
    }
    if (stream->ring_buffer != NULL) {
      ert_ring_buffer_destroy(stream->ring_buffer);
    }
  }

  free(comm_protocol->receive_streams);

  error_transmit_streams:
  for (uint16_t i = 0; i < comm_protocol->config.transmit_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[i];
    if (stream->acknowledgements != NULL) {
      free(stream->acknowledgements);
    }
    if (stream->packet_history_pointers != NULL) {
      free(stream->packet_history_pointers);
    }
    if (stream->packet_history_data_length != NULL) {
      free(stream->packet_history_data_length);
    }
    if (stream->packet_history_buffer_pool != NULL) {
      ert_buffer_pool_destroy(stream->packet_history_buffer_pool);
    }
    if (stream->ring_buffer != NULL) {
      ert_ring_buffer_destroy(stream->ring_buffer);
    }
  }

  free(comm_protocol->transmit_streams);

  error_receive_streams_mutex:
  pthread_mutex_destroy(&comm_protocol->receive_streams_mutex);

  error_transmit_streams_mutex:
  pthread_mutex_destroy(&comm_protocol->transmit_streams_mutex);

  error_status_mutex:
  pthread_mutex_destroy(&comm_protocol->status_mutex);

  error_comm_protocol:
  free(comm_protocol);

  return result;
}

int ert_comm_protocol_destroy(ert_comm_protocol *comm_protocol)
{
  comm_protocol->protocol_device->set_receive_callback(comm_protocol->protocol_device, NULL, NULL);

  ert_comm_protocol_stream_inactivity_check_stop(comm_protocol);

  int result = timer_delete(comm_protocol->acknowledgement_timeout_timer);
  if (result < 0) {
    ert_log_error("Error deleting acknowledgement timeout timer: %s", strerror(errno));
  }

  result = timer_delete(comm_protocol->stream_inactivity_check_timer);
  if (result < 0) {
    ert_log_error("Error deleting stream inactivity check timer: %s", strerror(errno));
  }

  for (uint16_t i = 0; i < comm_protocol->config.receive_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[i];
    free(stream->acknowledgements);
    free(stream->packet_history_pointers);
    free(stream->packet_history_data_length);
    ert_buffer_pool_destroy(stream->packet_history_buffer_pool);
    pthread_cond_destroy(&stream->change_cond);
    pthread_mutex_destroy(&stream->mutex);
    pthread_mutex_destroy(&stream->operation_mutex);
    ert_ring_buffer_destroy(stream->ring_buffer);
  }
  free(comm_protocol->receive_streams);

  for (uint16_t i = 0; i < comm_protocol->config.transmit_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[i];
    free(stream->acknowledgements);
    free(stream->packet_history_pointers);
    free(stream->packet_history_data_length);
    ert_buffer_pool_destroy(stream->packet_history_buffer_pool);
    ert_ring_buffer_destroy(stream->ring_buffer);
  }
  free(comm_protocol->transmit_streams);

  pthread_mutex_destroy(&comm_protocol->receive_streams_mutex);
  pthread_mutex_destroy(&comm_protocol->transmit_streams_mutex);
  pthread_mutex_destroy(&comm_protocol->status_mutex);

  free(comm_protocol);

  return 0;
}

static int ert_comm_protocol_stream_init_packet_buffer(ert_comm_protocol_stream *stream)
{
  if (!stream->used) {
    ert_log_error("Cannot init packet buffer: Stream ID %d not in use", stream->info.stream_id);
    return -EINVAL;
  }
  if (stream->info.end_of_stream) {
    ert_log_error("Cannot init packet buffer: Stream ID %d has reached end of stream", stream->info.stream_id);
    return -EINVAL;
  }
  if (ert_ring_buffer_get_used_bytes(stream->ring_buffer) > 0) {
    ert_log_error("Cannot init packet buffer: Stream ID %d transmit ring buffer not empty", stream->info.stream_id);
    return -EINVAL;
  }

  ert_comm_protocol_packet_header packet_header;

  packet_header.identifier = ERT_COMM_PROTOCOL_PACKET_IDENTIFIER;
  packet_header.sequence_number = (uint8_t) (stream->info.current_sequence_number & 0xFF);
  packet_header.port_stream_id = (uint8_t) (ert_comm_protocol_packet_set_port(stream->info.port)
                                 | ert_comm_protocol_packet_set_stream_id(stream->info.stream_id));
  packet_header.flags = 0;

  ert_ring_buffer_write(stream->ring_buffer, ert_comm_protocol_packet_header_length, (uint8_t *) &packet_header);

  return 0;
}

static int ert_comm_protocol_stream_set_packet_flags(ert_comm_protocol_stream *stream, bool add_flags, uint8_t flags)
{
  if (!stream->used) {
    return -EINVAL;
  }

  ert_comm_protocol_packet_header *header =
      (ert_comm_protocol_packet_header *) ert_ring_buffer_get_address(stream->ring_buffer);

  if (add_flags) {
    header->flags |= flags;
  } else {
    header->flags = flags;
  }

  return 0;
}

int ert_comm_protocol_transmit_stream_open(ert_comm_protocol *comm_protocol, uint8_t port,
    ert_comm_protocol_stream **stream_rcv, uint32_t stream_flags)
{
  pthread_mutex_lock(&comm_protocol->transmit_streams_mutex);

  ert_comm_protocol_stream *stream = ert_comm_protocol_transmit_stream_find_unused(comm_protocol);

  if (stream == NULL) {
    pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);
    return -EAGAIN;
  }

  pthread_mutex_lock(&stream->mutex);

  int result = ert_comm_protocol_stream_reset(stream, stream->info.stream_id);
  if (result < 0) {
    pthread_mutex_unlock(&stream->mutex);
    pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);
    ert_log_error("ert_comm_protocol_stream_reset failed with result %d", result);
    return -EINVAL;
  }

  stream->used = true;
  stream->info.acks_enabled = (stream_flags & ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED) ? true : false;
  stream->info.acks = (stream_flags & ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS) ? true : false;
  stream->info.start_of_stream = true;
  stream->info.current_sequence_number = 1;
  stream->info.port = (uint8_t) (port & 0x0F);

  ert_ring_buffer_clear(stream->ring_buffer);
  ert_buffer_pool_clear(stream->packet_history_buffer_pool);
  ert_comm_protocol_stream_packet_history_clear(stream);

  result = ert_comm_protocol_stream_init_packet_buffer(stream);
  if (result < 0) {
    stream->used = false;
    pthread_mutex_unlock(&stream->mutex);
    pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);
    return -EIO;
  }

  pthread_mutex_unlock(&stream->mutex);
  pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);

  ert_log_debug("Opened transmit stream_id=%d port=%d", stream->info.stream_id, stream->info.port);

  *stream_rcv = stream;

  return 0;
}

int ert_comm_protocol_transmit_stream_flush(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    bool end_of_stream, uint32_t *bytes_written_rcv)
{
  pthread_mutex_lock(&stream->mutex);

  if (!stream->used) {
    pthread_mutex_unlock(&stream->mutex);
    return -EINVAL;
  }
  if (stream->info.end_of_stream) {
    pthread_mutex_unlock(&stream->mutex);
    return 0;
  }
  if (stream->info.failed) {
    pthread_mutex_unlock(&stream->mutex);
    return -EPROTO;
  }

  int result;

  ert_log_debug("Stream flush: stream_id=%d, port=%d, buffer_used=%d",
      stream->info.stream_id, stream->info.port, ert_ring_buffer_get_used_bytes(stream->ring_buffer));

  if (!end_of_stream && ert_ring_buffer_get_used_bytes(stream->ring_buffer) == 0) {
    pthread_mutex_unlock(&stream->mutex);
    return 0;
  }

  uint8_t packet_flags = 0;

  if (stream->info.start_of_stream) {
    packet_flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_START_OF_STREAM;
  }

  if (end_of_stream) {
    packet_flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_END_OF_STREAM;
  }

  bool request_acks = false;

  if (stream->info.acks_enabled) {
    packet_flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS_ENABLED;
  }

  if (stream->info.acks) {
    packet_flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS;
  } else {
    request_acks = (stream->info.acks_enabled && end_of_stream)
        || ert_comm_protocol_transmit_stream_is_request_acks(stream);
    if (request_acks) {
      packet_flags |= ERT_COMM_PROTOCOL_PACKET_FLAG_REQUEST_ACKS;
    }
  }

  result = ert_comm_protocol_stream_set_packet_flags(stream, true, packet_flags);
  if (result < 0) {
    pthread_mutex_unlock(&stream->mutex);
    ert_log_error("ert_comm_protocol_stream_set_packet_flags failed with result %d", result);
    return -EIO;
  }

  uint8_t *buffer = ert_ring_buffer_get_address(stream->ring_buffer);
  uint32_t bytes_to_write = ert_ring_buffer_get_used_bytes(stream->ring_buffer);

  ert_comm_protocol_packet_header *header =
      (ert_comm_protocol_packet_header *) ert_ring_buffer_get_address(stream->ring_buffer);

  ert_log_debug("Stream flush: Packet header: stream_id=%d, port=%d, sequence_number=%d, buffer_used=%d " \
      "start_of_stream=%d, end_of_stream=%d, acks_enabled=%d, request_acks=%d, acks=%d, retransmit=%d",
      ert_comm_protocol_packet_get_stream_id(header->port_stream_id), ert_comm_protocol_packet_get_port(header->port_stream_id),
      header->sequence_number, ert_ring_buffer_get_used_bytes(stream->ring_buffer),
      (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_START_OF_STREAM) ? true : false,
      (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_END_OF_STREAM) ? true : false,
      (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS_ENABLED) ? true : false,
      (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_REQUEST_ACKS) ? true : false,
      (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS) ? true : false,
      (header->flags & ERT_COMM_PROTOCOL_PACKET_FLAG_RETRANSMIT) ? true : false);

  uint32_t sequence_number = header->sequence_number;

  if (stream->info.acks_enabled) {
    uint16_t port = (uint16_t) ert_comm_protocol_packet_get_port(header->port_stream_id);
    uint16_t stream_id = (uint16_t) ert_comm_protocol_packet_get_stream_id(header->port_stream_id);

    if (ert_comm_protocol_stream_packet_history_get(stream, port, stream_id, sequence_number, NULL, NULL)) {
      // Duplicate packet
      ert_comm_protocol_increment_counter(comm_protocol, stream,
          ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT_DUPLICATE,
          bytes_to_write, bytes_to_write - ert_comm_protocol_packet_header_length);

      ert_log_warn("Transmitting packet that is already in packet history list");
    } else {
      result = ert_comm_protocol_stream_packet_history_push(stream, bytes_to_write, buffer);
      if (result < 0) {
        pthread_mutex_unlock(&stream->mutex);
        ert_log_info("Packet history list full, requesting acks again");
        // TODO: Request acks using the latest packet with sequence number: stream->info.last_transferred_sequence_number
        return -EAGAIN;
      }

      ert_log_debug("Added transmitted packet to packet history list");
    }
  }

  uint32_t write_packet_flags =
      request_acks ? ERT_COMM_PROTOCOL_DEVICE_WRITE_PACKET_FLAG_SET_RECEIVE_ACTIVE : 0;

  pthread_mutex_unlock(&stream->mutex);

  uint32_t bytes_written = 0;
  result = comm_protocol->protocol_device->write_packet(comm_protocol->protocol_device,
      bytes_to_write, buffer, write_packet_flags, &bytes_written);
  if (result < 0) {
    ert_log_error("Protocol device write_packet failed with result %d", result);
    return -EIO;
  }

  pthread_mutex_lock(&stream->mutex);

  result = ert_comm_protocol_stream_update_transferred_packet_timestamp(stream);
  if (result < 0) {
    pthread_mutex_unlock(&stream->mutex);
    ert_log_error("ert_comm_protocol_stream_update_transferred_packet_timestamp failed with result %d", result);
    return -EIO;
  }

  if (stream->info.start_of_stream) {
    stream->info.start_of_stream = false;
  }

  int8_t signed_distance_latest = ert_comm_protocol_stream_calculate_sequence_number_distance(
      sequence_number, stream->info.last_transferred_sequence_number);

  if (signed_distance_latest > 0) {
    stream->info.last_transferred_sequence_number = sequence_number;
  }

  ert_comm_protocol_increment_counter(comm_protocol, stream,
      ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT, bytes_written,
      bytes_written - ert_comm_protocol_packet_header_length);

  if (end_of_stream) {
    ert_log_debug("flush: set end of stream pending");
    stream->info.end_of_stream_pending = true;
  }

  ert_log_debug("flush: last_acknowledged_sequence_number=%d last_transferred_sequence_number=%d history_count=%d",
    stream->info.last_acknowledged_sequence_number, stream->info.last_transferred_sequence_number,
      ert_comm_protocol_stream_packet_history_get_count(stream));

  if (ert_comm_protocol_transmit_stream_is_end_of_stream(stream)) {
    ert_log_debug("flush: set end of stream");
    stream->info.end_of_stream = true;
  }
  ert_ring_buffer_clear(stream->ring_buffer);

  stream->info.current_sequence_number = ert_comm_protocol_get_next_sequence_number(stream->info.current_sequence_number);

  if (!stream->info.end_of_stream) {
    result = ert_comm_protocol_stream_init_packet_buffer(stream);
    if (result < 0) {
      pthread_mutex_unlock(&stream->mutex);
      ert_log_error("ert_comm_protocol_stream_init_packet_buffer failed with result %d", result);
      return -EIO;
    }
  }

  if (request_acks) {
    stream->info.ack_request_pending = true;

    ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_INFO, &stream->info, "Acknowledgements request sent for stream");

    result = ert_comm_protocol_set_packet_acknowledgement_timeout(comm_protocol,
        comm_protocol->config.stream_acknowledgement_receive_timeout_millis);
    if (result < 0) {
      pthread_mutex_unlock(&stream->mutex);
      ert_log_error("Error setting packet acknowledgement timeout, result %d", result);
      return -EIO;
    }
  }

  if (bytes_written_rcv != NULL) {
    *bytes_written_rcv = bytes_written;
  }

  pthread_mutex_unlock(&stream->mutex);

  return 0;
}

int ert_comm_protocol_transmit_stream_write(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t length, uint8_t *data, uint32_t *bytes_written_rcv)
{
  pthread_mutex_lock(&stream->mutex);
  if (!stream->used) {
    pthread_mutex_unlock(&stream->mutex);
    return -EINVAL;
  }
  if (stream->info.end_of_stream) {
    pthread_mutex_unlock(&stream->mutex);
    return -EINVAL;
  }
  if (stream->info.failed) {
    pthread_mutex_unlock(&stream->mutex);
    ert_log_error("Stream write: stream_id=%d, port=%d, sequence_number=%d, length=%d: stream failed",
        stream->info.stream_id, stream->info.port, stream->info.current_sequence_number, length);
    return -EPROTO;
  }

  int result;
  uint32_t bytes_written = 0;
  uint32_t read_offset = 0;
  uint32_t remaining_bytes = length;

  ert_log_debug("Stream write: stream_id=%d, port=%d, sequence_number=%d, length=%d",
    stream->info.stream_id, stream->info.port, stream->info.current_sequence_number, length);

  while (remaining_bytes > 0) {
    uint32_t remaining_bytes_in_packet = comm_protocol->max_packet_size -
        ert_ring_buffer_get_used_bytes(stream->ring_buffer);

    if (remaining_bytes_in_packet == 0) {
      uint32_t bytes_flushed = 0;

      int retries = 3;

      retry_flush:

      pthread_mutex_unlock(&stream->mutex);
      result = ert_comm_protocol_transmit_stream_flush(comm_protocol, stream, false, &bytes_flushed);
      pthread_mutex_lock(&stream->mutex);
      retries--;

      if (result == -EAGAIN && retries > 0) {
        ert_log_info("Stream write: stream_id=%d, port=%d, sequence_number=%d, length=%d: " \
            "Retrying stream flush, waiting for acknowledgements ...",
            stream->info.stream_id, stream->info.port, stream->info.current_sequence_number, length);
        pthread_mutex_unlock(&stream->mutex);
        usleep(comm_protocol->config.stream_acknowledgement_receive_timeout_millis * 1000 * 2);
        pthread_mutex_lock(&stream->mutex);
        goto retry_flush;
      } else if (result < 0) {
        pthread_mutex_unlock(&stream->mutex);
        ert_log_error("Stream write: stream_id=%d, port=%d, sequence_number=%d, length=%d: " \
            "ert_comm_protocol_transmit_stream_flush failed with result %d",
            stream->info.stream_id, stream->info.port, stream->info.current_sequence_number, length, result);
        return result;
      }

      continue;
    }

    uint32_t bytes_to_write = (remaining_bytes > remaining_bytes_in_packet)
                              ? remaining_bytes_in_packet : remaining_bytes;

    result = ert_ring_buffer_write(stream->ring_buffer, bytes_to_write, data + read_offset);
    if (result < 0) {
      pthread_mutex_unlock(&stream->mutex);
      ert_log_error("Error writing %d bytes to ring buffer", bytes_to_write);
      return -EIO;
    }

    read_offset += bytes_to_write;
    remaining_bytes -= bytes_to_write;
    bytes_written += bytes_to_write;
  }

  if (bytes_written_rcv != NULL) {
    *bytes_written_rcv = bytes_written;
  }

  pthread_mutex_unlock(&stream->mutex);

  return 0;
}

int ert_comm_protocol_transmit_stream_close(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream, bool force)
{
  if (!stream->used) {
    return -EINVAL;
  }

  stream->info.close_pending = true;

  int result = 0;

  if (force) {
    ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_INFO, &stream->info, "Closing transmit stream - force=%d", force);
  } else {
    result = ert_comm_protocol_transmit_stream_flush(comm_protocol, stream, true, NULL);

    ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_INFO, &stream->info, "Closing transmit stream - force=%d, waiting for acks...", force);

    if (stream->info.acks_enabled) {
      // Wait for acks
      return result;
    }
  }

  pthread_mutex_lock(&stream->mutex);
  ert_comm_protocol_transmit_stream_do_close(comm_protocol, stream);
  pthread_mutex_unlock(&stream->mutex);

  return result;
}

int ert_comm_protocol_receive_stream_read(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
  uint32_t wait_for_milliseconds, uint32_t length, uint8_t *data, uint32_t *bytes_received)
{
  pthread_mutex_lock(&stream->mutex);

  if (!stream->used) {
    pthread_mutex_unlock(&stream->mutex);
    return -EINVAL;
  }

  if (stream->info.failed) {
    pthread_mutex_unlock(&stream->mutex);
    return -EPROTO;
  }

  if (ert_ring_buffer_get_used_bytes(stream->ring_buffer) == 0) {
    ert_log_debug("port=%d, stream_id=%d, end_of_stream=%d", stream->info.port, stream->info.stream_id, stream->info.end_of_stream);
    if (stream->info.end_of_stream) {
      pthread_mutex_unlock(&stream->mutex);
      *bytes_received = 0;
      return 0;
    }

    struct timespec to;

    int result = ert_get_current_timestamp_offset(&to, wait_for_milliseconds);
    if (result < 0) {
      pthread_mutex_unlock(&stream->mutex);
      return -EIO;
    }

    result = pthread_cond_timedwait(&stream->change_cond, &stream->mutex, &to);

    if (result == ETIMEDOUT || ert_ring_buffer_get_used_bytes(stream->ring_buffer) == 0) {
      pthread_mutex_unlock(&stream->mutex);

      if (stream->info.end_of_stream) {
        *bytes_received = 0;
        return 0;
      }

      return -ETIMEDOUT;
    } else if (result != 0) {
      pthread_mutex_unlock(&stream->mutex);
      ert_log_error("pthread_cond_timedwait failed with result %d", result);
      return -EIO;
    }
  }

  uint32_t read_bytes;

  ert_ring_buffer_read(stream->ring_buffer, length, data, &read_bytes);

  pthread_mutex_unlock(&stream->mutex);

  *bytes_received = read_bytes;

  return 0;
}

int ert_comm_protocol_receive_stream_close(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream)
{
  pthread_mutex_lock(&comm_protocol->receive_streams_mutex);
  pthread_mutex_lock(&stream->operation_mutex);
  pthread_mutex_lock(&stream->mutex);

  if (!stream->used) {
    pthread_mutex_unlock(&stream->mutex);
    pthread_mutex_unlock(&stream->operation_mutex);
    pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);
    return -EINVAL;
  }

  ert_comm_protocol_log_stream_info(ERT_LOG_LEVEL_INFO, &stream->info, "Closing receive stream");

  stream->used = false;

  int result = ert_comm_protocol_stream_reset(stream, 0);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_reset failed with result %d", result);
  }

  pthread_mutex_unlock(&stream->mutex);
  pthread_mutex_unlock(&stream->operation_mutex);
  pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);

  return 0;
}

int ert_comm_protocol_get_active_streams(ert_comm_protocol *comm_protocol,
    size_t *stream_info_count, ert_comm_protocol_stream_info **stream_info_rcv)
{
  size_t total_stream_count = comm_protocol->config.transmit_stream_count + comm_protocol->config.receive_stream_count;
  ert_comm_protocol_stream_info *stream_info = calloc(total_stream_count, sizeof(ert_comm_protocol_stream_info));
  if (stream_info == NULL) {
    ert_log_fatal("Error allocating memory for getting active stream info: %s", strerror(errno));
    return -ENOMEM;
  }

  size_t stream_info_index = 0;

  pthread_mutex_lock(&comm_protocol->transmit_streams_mutex);
  for (size_t i = 0; i < comm_protocol->config.transmit_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->transmit_streams[i];

    if (stream->used) {
      memcpy(&stream_info[stream_info_index], &stream->info, sizeof(ert_comm_protocol_stream_info));
      stream_info_index++;
    }
  }
  pthread_mutex_unlock(&comm_protocol->transmit_streams_mutex);

  pthread_mutex_lock(&comm_protocol->receive_streams_mutex);
  for (size_t i = 0; i < comm_protocol->config.receive_stream_count; i++) {
    ert_comm_protocol_stream *stream = &comm_protocol->receive_streams[i];

    if (stream->used) {
      memcpy(&stream_info[stream_info_index], &stream->info, sizeof(ert_comm_protocol_stream_info));
      stream_info_index++;
    }
  }
  pthread_mutex_unlock(&comm_protocol->receive_streams_mutex);

  *stream_info_count = stream_info_index;
  *stream_info_rcv = stream_info;

  return 0;
}
