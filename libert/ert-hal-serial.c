/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-hal-serial.h"
#include "ert-hal-serial-posix.h"

hal_serial_driver *serial_driver = &serial_driver_posix;

int hal_serial_open(hal_serial_device_config *config, hal_serial_device **serial_device_rcv)
{
  return serial_driver->open(config, serial_device_rcv);
}

int hal_serial_close(hal_serial_device *serial_device)
{
  return serial_driver->close(serial_device);
}

int hal_serial_read(hal_serial_device *serial_device, uint32_t timeout_millis, uint32_t length, uint8_t *buffer, uint32_t *bytes_read)
{
  return serial_driver->read(serial_device, timeout_millis, length, buffer, bytes_read);
}

int hal_serial_write(hal_serial_device *serial_device, uint32_t length, uint8_t *buffer, uint32_t *bytes_written)
{
  return serial_driver->write(serial_device, length, buffer, bytes_written);
}
