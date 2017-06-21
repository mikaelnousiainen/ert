/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_TRANSCEIVER_H
#define __ERT_COMM_TRANSCEIVER_H

#include "ert-common.h"
#include "ert-comm.h"
#include "ert-buffer-pool.h"
#include "ert-pipe.h"
#include <pthread.h>
#include <time.h>

#define ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_BLOCK 0x01
#define ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_SET_RECEIVE_ACTIVE 0x02

typedef enum _ert_comm_transceiver_event_type {
  ERT_COMM_TRANSCEIVER_EVENT_CONFIGURATION_CHANGED = 0x01,
  ERT_COMM_TRANSCEIVER_EVENT_FREQUENCY_CHANGED = 0x02,
  ERT_COMM_TRANSCEIVER_EVENT_STATUS_READ = 0x21
} ert_comm_transceiver_event_type;

typedef void (*ert_comm_transceiver_transmit_callback)(uint32_t id, uint32_t length, void *callback_context);
typedef void (*ert_comm_transceiver_receive_callback)(uint32_t length, uint8_t *data, void *callback_context);
typedef void (*ert_comm_transceiver_event_callback)(ert_comm_transceiver_event_type type, int result,
    void *callback_context);

typedef struct _ert_comm_transceiver_status {
  uint64_t transmitted_packet_count;
  uint64_t received_packet_count;
  uint64_t invalid_received_packet_count;

  uint64_t transmitted_bytes;
  uint64_t received_bytes;

  struct timespec last_transmitted_packet_timestamp;
  struct timespec last_received_packet_timestamp;
  struct timespec last_invalid_received_packet_timestamp;

  struct timespec comm_device_receive_mode_started_timestamp;
} ert_comm_transceiver_status;

typedef struct _ert_comm_transceiver_config {
  uint32_t transmit_buffer_length_packets;
  uint32_t receive_buffer_length_packets;

  uint32_t transmit_timeout_milliseconds;
  uint32_t poll_interval_milliseconds;

  uint32_t maximum_receive_time_milliseconds;

  ert_comm_transceiver_transmit_callback transmit_callback;
  void *transmit_callback_context;

  ert_comm_transceiver_receive_callback receive_callback;
  void *receive_callback_context;

  ert_comm_transceiver_event_callback event_callback;
  void *event_callback_context;
} ert_comm_transceiver_config;

typedef struct _ert_comm_transceiver {
  ert_comm_transceiver_config config;

  ert_comm_device *device;

  pthread_t maintenance_thread;
  pthread_t transmit_dispatch_thread;
  pthread_t receive_dispatch_thread;
  volatile bool running;

  volatile bool receive_active;
  volatile bool transmit_active;
  volatile void *new_config;
  ert_comm_device_config_type new_frequency_config_type;
  volatile double new_frequency;

  pthread_mutex_t transmit_mutex;
  pthread_mutex_t device_mutex;

  pthread_mutex_t event_mutex;
  pthread_cond_t event_cond;
  volatile bool event_signal;

  uint32_t max_packet_length;

  ert_buffer_pool *transmit_buffer_metadata_pool;
  ert_buffer_pool *transmit_buffer_pool;
  ert_pipe *transmit_buffer_queue;
  ert_pipe *transmit_wait_queue;
  ert_pipe *transmit_result_queue;

  ert_buffer_pool *receive_buffer_pool;
  ert_pipe *receive_buffer_queue;

  ert_comm_device_status device_status;

  pthread_mutex_t status_mutex;
  ert_comm_transceiver_status status;
} ert_comm_transceiver;

int ert_comm_transceiver_start(ert_comm_device *device,
    ert_comm_transceiver_config *transceiver_config,
    ert_comm_transceiver **transceiver_rcv);
int ert_comm_transceiver_set_transmit_callback(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_transmit_callback transmit_callback, void *transmit_callback_context);
int ert_comm_transceiver_set_receive_callback(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_receive_callback receive_callback, void *receive_callback_context);
int ert_comm_transceiver_set_event_callback(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_event_callback event_callback, void *event_callback_context);
uint32_t ert_comm_transceiver_get_max_packet_length(ert_comm_transceiver *transceiver);
int ert_comm_transceiver_configure(ert_comm_transceiver *transceiver, void *config);
int ert_comm_transceiver_set_receive_active(ert_comm_transceiver *transceiver, bool receive_active);
int ert_comm_transceiver_transmit(ert_comm_transceiver *transceiver,
    uint32_t id, uint32_t length, uint8_t *data, uint32_t flags, uint32_t *bytes_transmitted_rcv);
int ert_comm_transceiver_set_frequency(ert_comm_transceiver *transceiver, ert_comm_device_config_type config_type, double frequency);
int ert_comm_transceiver_get_status(ert_comm_transceiver *transceiver, ert_comm_transceiver_status *status);
int ert_comm_transceiver_get_device_status(ert_comm_transceiver *transceiver, ert_comm_device_status *status);
int ert_comm_transceiver_stop(ert_comm_transceiver *transceiver);

#endif
