/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_HAL_I2C_H
#define __ERT_HAL_I2C_H

#include "ert-hal-common.h"

typedef struct _hal_i2c_device {
  int bus;
  int device;

  int fd;
  bool open;
} hal_i2c_device;

typedef struct _hal_i2c_driver {
  int (*open)(int bus, int device, hal_i2c_device  **i2c_device_rcv);
  int (*close)(hal_i2c_device *i2c_device);
  int (*read_byte)(hal_i2c_device *i2c_device, uint8_t *value);
  int (*write_byte)(hal_i2c_device *i2c_device, uint8_t value);
  int (*read_byte_command)(hal_i2c_device *i2c_device, uint8_t command, uint8_t *value);
  int (*read_word_command)(hal_i2c_device *i2c_device, uint8_t command, uint16_t *value);
  int (*write_byte_command)(hal_i2c_device *i2c_device, uint8_t command, uint8_t value);
  int (*write_word_command)(hal_i2c_device *i2c_device, uint8_t command, uint16_t value);
  int (*read_block)(hal_i2c_device *i2c_device, uint8_t command, uint8_t *values, uint8_t *bytes_read_rcv);
  int (*write_block)(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values);
  int (*read_i2c_block)(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values, uint8_t *bytes_read_rcv);
  int (*write_i2c_block)(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values);
} hal_i2c_driver;

int hal_i2c_open(int bus, int device, hal_i2c_device  **i2c_device_rcv);
int hal_i2c_close(hal_i2c_device *i2c_device);
int hal_i2c_read_byte(hal_i2c_device *i2c_device, uint8_t *value);
int hal_i2c_write_byte(hal_i2c_device *i2c_device, uint8_t value);
int hal_i2c_read_byte_command(hal_i2c_device *i2c_device, uint8_t command, uint8_t *value);
int hal_i2c_read_word_command(hal_i2c_device *i2c_device, uint8_t command, uint16_t *value);
int hal_i2c_write_byte_command(hal_i2c_device *i2c_device, uint8_t command, uint8_t value);
int hal_i2c_write_word_command(hal_i2c_device *i2c_device, uint8_t command, uint16_t value);
int hal_i2c_read_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t *values, uint8_t *bytes_read_rcv);
int hal_i2c_write_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values);
int hal_i2c_read_i2c_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values, uint8_t *bytes_read_rcv);
int hal_i2c_write_i2c_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values);

#endif
