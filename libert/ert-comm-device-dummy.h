/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_DEVICE_DUMMY_H
#define __ERT_COMM_DEVICE_DUMMY_H

#include "ert-common.h"
#include "ert-comm.h"

#include <time.h>

typedef struct _ert_comm_driver_dummy_config {
  uint32_t transmit_time_millis;
  uint32_t max_packet_length;
  ert_comm_device *other_device;

  ert_comm_driver_callback receive_callback;
  ert_comm_driver_callback transmit_callback;
  ert_comm_driver_callback detection_callback;
  void *callback_context;
} ert_comm_driver_dummy_config;

typedef struct _ert_comm_driver_dummy {
  ert_comm_driver_dummy_config config;

  timer_t transmit_callback_timer;

  uint32_t transmit_data_length;
  uint8_t *transmit_data;

  uint32_t receive_data_length;
  uint8_t *receive_data;

  bool fail_transmit;
  bool fail_receive;
  bool lose_packets;
  bool no_transmit_callback;

  volatile bool transmit_active;

  int (*inject)(ert_comm_device *device, uint32_t length, uint8_t *data);
} ert_driver_comm_device_dummy;

int ert_driver_comm_device_dummy_open(ert_comm_driver_dummy_config *config, ert_comm_device **device_rcv);
int ert_driver_comm_device_dummy_connect(ert_comm_device *device, ert_comm_device *other_device);
void ert_driver_comm_device_dummy_set_fail_transmit(ert_comm_device *device, bool fail_transmit);
void ert_driver_comm_device_dummy_set_fail_receive(ert_comm_device *device, bool fail_receive);
void ert_driver_comm_device_dummy_set_lose_packets(ert_comm_device *device, bool lose_packets);
void ert_driver_comm_device_dummy_set_no_transmit_callback(ert_comm_device *device, bool no_transmit_callback);
int ert_driver_comm_device_dummy_close(ert_comm_device *device);

#endif
