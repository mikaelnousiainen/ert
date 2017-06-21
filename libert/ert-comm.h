/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_H
#define __ERT_COMM_H

#include "ert-common.h"
#include <time.h>

typedef enum _ert_comm_device_config_type {
  ERT_COMM_DEVICE_CONFIG_TYPE_TRANSMIT = 1,
  ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE = 2,
} ert_comm_device_config_type;

typedef enum _ert_comm_device_state {
  ERT_COMM_DEVICE_STATE_SLEEP = 0x01,
  ERT_COMM_DEVICE_STATE_STANDBY = 0x02,
  ERT_COMM_DEVICE_STATE_TRANSMIT = 0x11,
  ERT_COMM_DEVICE_STATE_DETECTION = 0x21,
  ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS = 0x31,
  ERT_COMM_DEVICE_STATE_RECEIVE_SINGLE = 0x32
} ert_comm_device_state;

typedef struct _ert_comm_device_status {
  const char *name;

  const char *model;
  const char *manufacturer;

  struct timespec timestamp;

  ert_comm_device_state device_state;

  double frequency;

  uint64_t transmitted_packet_count;
  uint64_t received_packet_count;
  uint64_t invalid_received_packet_count;

  uint64_t transmitted_bytes;
  uint64_t received_bytes;

  struct timespec last_transmitted_packet_timestamp;
  struct timespec last_received_packet_timestamp;
  struct timespec last_invalid_received_packet_timestamp;

  float last_received_packet_rssi;
  float current_rssi;

  double frequency_error;

  void *custom;
} ert_comm_device_status;

typedef struct _ert_comm_driver ert_comm_driver;

typedef struct _ert_comm_device {
  ert_comm_device_status status;

  ert_comm_driver *driver;

  void *priv;
} ert_comm_device;

typedef void (*ert_comm_driver_callback)(void *callback_context);

typedef struct _ert_comm_driver {
  int (*transmit)(ert_comm_device *device, uint32_t length, uint8_t *payload, uint32_t *bytes_transmitted);
  int (*wait_for_transmit)(ert_comm_device *device, uint32_t milliseconds);

  uint32_t (*get_max_packet_length)(ert_comm_device *device);

  int (*set_receive_callback)(ert_comm_device *device, ert_comm_driver_callback receive_callback);
  int (*set_transmit_callback)(ert_comm_device *device, ert_comm_driver_callback transmit_callback);
  int (*set_detection_callback)(ert_comm_device *device, ert_comm_driver_callback detection_callback);
  int (*set_callback_context)(ert_comm_device *device, void *callback_context);

  int (*configure)(ert_comm_device *device, void *config);
  int (*set_frequency)(ert_comm_device *device, ert_comm_device_config_type type, double frequency);
  int (*get_frequency_error)(ert_comm_device *device, double *frequency_error_hz);

  int (*start_detection)(ert_comm_device *device);
  int (*wait_for_detection)(ert_comm_device *device, uint32_t milliseconds);

  int (*start_receive)(ert_comm_device *device, bool continuous);
  int (*wait_for_data)(ert_comm_device *device, uint32_t milliseconds);
  int (*receive)(ert_comm_device *device, uint32_t length, uint8_t *buffer, uint32_t *bytes_received);

  int (*read_status)(ert_comm_device *device);
  int (*get_status)(ert_comm_device *device, ert_comm_device_status *status);

  int (*standby)(ert_comm_device *device);
  int (*sleep)(ert_comm_device *device);

  int (*close)(ert_comm_device *device);
} ert_comm_driver;

#endif
