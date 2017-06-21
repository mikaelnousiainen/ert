/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_HAL_SPI_H
#define __ERT_HAL_SPI_H

#include "ert-hal-common.h"

typedef struct _hal_spi_device {
  int bus;
  int device;

  uint32_t speed;
  uint8_t bits_per_word;
  uint16_t delay_usecs;

  int fd;
  bool open;
} hal_spi_device;

typedef struct _hal_spi_driver {
  int (*open)(int bus, int device, uint32_t speed, uint32_t mode, hal_spi_device **spi_device_rcv);
  int (*close)(hal_spi_device *spi_device);
  int (*transfer)(hal_spi_device *spi_device, uint32_t length, uint8_t *data);
} hal_spi_driver;

int hal_spi_open(int bus, int device, uint32_t speed, uint32_t mode, hal_spi_device **spi_device_rcv);
int hal_spi_close(hal_spi_device *spi_device);
int hal_spi_transfer(hal_spi_device *spi_device, int length, uint8_t *data);

#endif
