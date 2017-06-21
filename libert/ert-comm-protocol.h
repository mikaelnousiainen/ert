/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_PROTOCOL_H
#define __ERT_COMM_PROTOCOL_H

#include "ert-common.h"
#include "ert-ring-buffer.h"
#include "ert-buffer-pool.h"
#include <pthread.h>
#include <time.h>

#define ERT_COMM_PROTOCOL_MAX_TRANSMIT_STREAM_COUNT_DEFAULT 16
#define ERT_COMM_PROTOCOL_MAX_RECEIVE_STREAM_COUNT_DEFAULT 32

#define ERT_COMM_PROTOCOL_STREAM_INACTIVITY_TIMEOUT_MILLIS_DEFAULT 20000

#define ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT 32
#define ERT_COMM_PROTOCOL_STREAM_ACK_RECEIVE_TIMEOUT_MILLIS_DEFAULT 1000
#define ERT_COMM_PROTOCOL_STREAM_ACK_GUARD_INTERVAL_MILLIS_DEFAULT 50

#define ERT_COMM_PROTOCOL_STREAM_ACK_REREQUEST_COUNT_MAX_DEFAULT 5
#define ERT_COMM_PROTOCOL_STREAM_END_OF_STREAM_ACK_REREQUEST_COUNT_MAX_DEFAULT 2

#define ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED 0x01
#define ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS 0x02

struct _ert_comm_protocol_stream;
struct _ert_comm_protocol;

typedef struct _ert_comm_protocol_stream ert_comm_protocol_stream;
typedef struct _ert_comm_protocol ert_comm_protocol;

typedef void (*ert_comm_protocol_stream_listener_callback)(struct _ert_comm_protocol *comm_protocol,
    struct _ert_comm_protocol_stream *stream, void *callback_context);

typedef void (*ert_comm_protocol_device_receive_callback)(
    uint32_t length, uint8_t *data, void *callback_context);

#define ERT_COMM_PROTOCOL_DEVICE_WRITE_PACKET_FLAG_SET_RECEIVE_ACTIVE 0x01

typedef struct _ert_comm_protocol_device {
  void *priv;

  uint32_t (*get_max_packet_length)(struct _ert_comm_protocol_device *protocol_device);
  int (*set_receive_callback)(struct _ert_comm_protocol_device *protocol_device,
      ert_comm_protocol_device_receive_callback receive_callback, void *receive_callback_context);
  int (*set_receive_active)(struct _ert_comm_protocol_device *protocol_device, bool receive_active);
  int (*write_packet)(struct _ert_comm_protocol_device *protocol_device, uint32_t length, uint8_t *data,
      uint32_t flags, uint32_t *bytes_written);
  int (*close)(struct _ert_comm_protocol_device *protocol_device);
} ert_comm_protocol_device;

typedef enum _ert_comm_protocol_packet_flags {
  ERT_COMM_PROTOCOL_PACKET_FLAG_START_OF_STREAM = 0x01,
  ERT_COMM_PROTOCOL_PACKET_FLAG_END_OF_STREAM = 0x02,
  ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS_ENABLED = 0x04,
  ERT_COMM_PROTOCOL_PACKET_FLAG_REQUEST_ACKS = 0x08,
  ERT_COMM_PROTOCOL_PACKET_FLAG_RETRANSMIT = 0x10,
  ERT_COMM_PROTOCOL_PACKET_FLAG_ACKS = 0x20,
} ert_comm_protocol_packet_flags;

typedef enum _ert_comm_protocol_stream_type {
  ERT_COMM_PROTOCOL_STREAM_TYPE_UNDEFINED = 0x00,
  ERT_COMM_PROTOCOL_STREAM_TYPE_TRANSMIT = 0x01,
  ERT_COMM_PROTOCOL_STREAM_TYPE_RECEIVE = 0x02,
} ert_comm_protocol_stream_type;

typedef struct _ert_comm_protocol_stream_info {
  ert_comm_protocol_stream_type type;

  uint16_t stream_id;
  uint16_t port;

  bool acks_enabled;
  bool acks;
  volatile bool ack_request_pending;

  volatile bool start_of_stream;
  volatile bool end_of_stream_pending;
  volatile bool end_of_stream;
  volatile bool close_pending;

  volatile bool failed;

  volatile uint32_t current_sequence_number;
  volatile uint32_t last_acknowledged_sequence_number;
  volatile uint32_t last_transferred_sequence_number;

  volatile uint64_t transferred_packet_count;
  volatile uint64_t transferred_data_bytes;
  volatile uint64_t transferred_payload_data_bytes;
  volatile uint64_t duplicate_transferred_packet_count;

  struct timespec last_transferred_packet_timestamp;

  volatile uint16_t ack_rerequest_count;
  volatile uint16_t end_of_stream_ack_rerequest_count;

  volatile uint64_t retransmitted_packet_count;
  volatile uint64_t retransmitted_data_bytes;
  volatile uint64_t retransmitted_payload_data_bytes;

  volatile uint64_t received_packet_sequence_number_error_count;
} ert_comm_protocol_stream_info;

typedef struct _ert_comm_protocol_status {
  uint64_t transmitted_packet_count;
  uint64_t transmitted_data_bytes;
  uint64_t transmitted_payload_data_bytes;
  uint64_t duplicate_transmitted_packet_count;

  uint64_t retransmitted_packet_count;
  uint64_t retransmitted_data_bytes;
  uint64_t retransmitted_payload_data_bytes;

  struct timespec last_transmitted_packet_timestamp;
  struct timespec last_retransmitted_packet_timestamp;

  uint64_t received_packet_count;
  uint64_t received_data_bytes;
  uint64_t received_payload_data_bytes;

  uint64_t duplicate_received_packet_count;
  uint64_t received_packet_sequence_number_error_count;
  uint64_t invalid_received_packet_count;

  struct timespec last_received_packet_timestamp;
  struct timespec last_invalid_received_packet_timestamp;
} ert_comm_protocol_status;

typedef struct _ert_comm_protocol_config {
  bool passive_mode;
  bool transmit_all_data;
  bool ignore_errors;

  uint32_t receive_buffer_length_packets;

  uint32_t stream_inactivity_timeout_millis;

  uint32_t stream_acknowledgement_interval_packet_count;
  uint32_t stream_acknowledgement_receive_timeout_millis;
  uint32_t stream_acknowledgement_guard_interval_millis;
  uint32_t stream_acknowledgement_max_rerequest_count;
  uint32_t stream_end_of_stream_acknowledgement_max_rerequest_count;

  uint16_t transmit_stream_count;
  uint16_t receive_stream_count;
} ert_comm_protocol_config;

void ert_comm_protocol_create_default_config(ert_comm_protocol_config *config);
int ert_comm_protocol_create(
    ert_comm_protocol_config *config,
    ert_comm_protocol_stream_listener_callback stream_listener_callback,
    void *stream_listener_callback_context,
    ert_comm_protocol_device *protocol_device,
    ert_comm_protocol **comm_protocol_rcv);
int ert_comm_protocol_destroy(ert_comm_protocol *comm_protocol);
int ert_comm_protocol_get_status(ert_comm_protocol *comm_protocol, ert_comm_protocol_status *status);
int ert_comm_protocol_stream_get_info(ert_comm_protocol_stream *stream, ert_comm_protocol_stream_info *stream_info);
int ert_comm_protocol_transmit_stream_open(ert_comm_protocol *comm_protocol, uint8_t port,
    ert_comm_protocol_stream **stream_rcv, uint32_t stream_flags);
int ert_comm_protocol_transmit_stream_flush(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    bool end_of_stream, uint32_t *bytes_written_rcv);
int ert_comm_protocol_transmit_stream_write(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t length, uint8_t *data, uint32_t *bytes_written_rcv);
int ert_comm_protocol_transmit_stream_close(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream, bool force);
int ert_comm_protocol_receive_stream_read(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t wait_for_milliseconds, uint32_t length, uint8_t *data, uint32_t *bytes_received);
int ert_comm_protocol_receive_stream_close(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream);
int ert_comm_protocol_get_active_streams(ert_comm_protocol *comm_protocol,
    size_t *stream_info_count, ert_comm_protocol_stream_info **stream_info_rcv);

#endif
