/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_HAL_SERIAL_H
#define __ERT_HAL_SERIAL_H

#include "ert-hal-common.h"

typedef struct _hal_serial_device_config {
  char device[256];
  uint32_t speed;
  bool parity;
  bool stop_bits_2;
} hal_serial_device_config;

typedef struct _hal_serial_device {
  hal_serial_device_config config;
  void *priv;
} hal_serial_device;

typedef struct _hal_serial_driver {
  int (*open)(hal_serial_device_config *config, hal_serial_device **serial_device_rcv);
  int (*close)(hal_serial_device *serial_device);
  int (*read)(hal_serial_device *serial_device, uint32_t timeout_millis, uint32_t length, uint8_t *buffer, uint32_t *bytes_read);
  int (*write)(hal_serial_device *serial_device, uint32_t length, uint8_t *buffer, uint32_t *bytes_written);
} hal_serial_driver;

int hal_serial_open(hal_serial_device_config *config, hal_serial_device **serial_device_rcv);
int hal_serial_close(hal_serial_device *serial_device);
int hal_serial_read(hal_serial_device *serial_device, uint32_t timeout_millis, uint32_t length, uint8_t *buffer, uint32_t *bytes_read);
int hal_serial_write(hal_serial_device *serial_device, uint32_t length, uint8_t *buffer, uint32_t *bytes_written);

#endif
