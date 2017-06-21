/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-hal-spi.h"
#include "ert-hal-spi-linux.h"

hal_spi_driver *spi_driver = &hal_spi_driver_linux;

int hal_spi_open(int bus, int device, uint32_t speed, uint32_t mode, hal_spi_device **spi_device_rcv)
{
  return spi_driver->open(bus, device, speed, mode, spi_device_rcv);
}

int hal_spi_close(hal_spi_device *spi_device)
{
  return spi_driver->close(spi_device);
}

int hal_spi_transfer(hal_spi_device *spi_device, int length, uint8_t *data)
{
  return spi_driver->transfer(spi_device, length, data);
}
