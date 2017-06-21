/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-hal-i2c.h"
#include "ert-hal-i2c-linux.h"

hal_i2c_driver *i2c_driver = &hal_i2c_driver_linux;

int hal_i2c_open(int bus, int device, hal_i2c_device **i2c_device_rcv)
{
  return i2c_driver->open(bus, device, i2c_device_rcv);
}

int hal_i2c_close(hal_i2c_device *i2c_device)
{
  return i2c_driver->close(i2c_device);
}

int hal_i2c_read_byte(hal_i2c_device *i2c_device, uint8_t *value)
{
  return i2c_driver->read_byte(i2c_device, value);
}

int hal_i2c_write_byte(hal_i2c_device *i2c_device, uint8_t value)
{
  return i2c_driver->write_byte(i2c_device, value);
}

int hal_i2c_read_byte_command(hal_i2c_device *i2c_device, uint8_t command, uint8_t *value)
{
  return i2c_driver->read_byte_command(i2c_device, command, value);
}

int hal_i2c_read_word_command(hal_i2c_device *i2c_device, uint8_t command, uint16_t *value)
{
  return i2c_driver->read_word_command(i2c_device, command, value);
}

int hal_i2c_write_byte_command(hal_i2c_device *i2c_device, uint8_t command, uint8_t value)
{
  return i2c_driver->write_byte_command(i2c_device, command, value);
}

int hal_i2c_write_word_command(hal_i2c_device *i2c_device, uint8_t command, uint16_t value)
{
  return i2c_driver->write_word_command(i2c_device, command, value);
}

int hal_i2c_read_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t *values, uint8_t *bytes_read_rcv)
{
  return i2c_driver->read_block(i2c_device, command, values, bytes_read_rcv);
}

int hal_i2c_write_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values)
{
  return i2c_driver->write_block(i2c_device, command, length, values);
}

int hal_i2c_read_i2c_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values, uint8_t *bytes_read_rcv)
{
  return i2c_driver->read_i2c_block(i2c_device, command, length, values, bytes_read_rcv);
}

int hal_i2c_write_i2c_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values)
{
  return i2c_driver->write_i2c_block(i2c_device, command, length, values);
}
