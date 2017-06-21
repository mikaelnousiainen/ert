/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "ert-log.h"
#include "ert-hal-spi-linux.h"

#define SPI_DEVICE_FILE_NAME_LENGTH 256
#define SPI_BITS_PER_WORD_DEFAULT 8

const static char *spi_device_file_template = "/dev/spidev%d.%d" ;

// TODO: add constants for mode
// TODO: add functions for changing mode, speed and bits per word
// TODO: add function to set cs_change
// TODO: add function to set high CS

int hal_spi_linux_open(int bus, int device, uint32_t speed, uint32_t mode, hal_spi_device **spi_device_rcv)
{
  char device_file[SPI_DEVICE_FILE_NAME_LENGTH];
  int fd, retval;
  hal_spi_device *spi_device;

  if (snprintf(device_file, SPI_DEVICE_FILE_NAME_LENGTH, spi_device_file_template, bus, device) >= SPI_DEVICE_FILE_NAME_LENGTH) {
    ert_log_error("Invalid SPI bus or device");
    return -EINVAL;
  }
  
  fd = open(device_file, O_RDWR);
  if (fd < 0) {
    ert_log_error("Error opening SPI device file");
    return -ENOENT;
  }

  spi_device = malloc(sizeof(hal_spi_device));
  if (spi_device == NULL) {
    close(fd);
    ert_log_fatal("Error allocating memory for SPI device struct: %s", strerror(errno));
    return -ENOMEM;
  }

  spi_device->fd = fd;
  spi_device->bus = bus;
  spi_device->device = device;
  spi_device->speed = speed;
  spi_device->bits_per_word = SPI_BITS_PER_WORD_DEFAULT;
  spi_device->delay_usecs = 0;

  retval = ioctl(fd, SPI_IOC_WR_MODE, &mode);
  
  if (retval < 0) {
    free(spi_device);
    close(fd);
    ert_log_error("Error setting SPI mode");
    return -EIO;
  }

  retval = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_device->bits_per_word);
  if (retval < 0) {
    free(spi_device);
    close(fd);
    ert_log_error("Error setting SPI bits per word");
    return -EIO;
  }

  retval = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_device->speed);
  if (retval < 0) {
    free(spi_device);
    close(fd);
    ert_log_error("Error setting SPI speed");
    return -EIO;
  }

  spi_device->open = true;

  *spi_device_rcv = spi_device;

  return 0;
}

int hal_spi_linux_close(hal_spi_device *spi_device)
{
  if (spi_device == NULL) {
    return 0;
  }
  if (!spi_device->open || spi_device->fd < 1) {
    return 0;
  }

  spi_device->open = false;

  close(spi_device->fd);
  free(spi_device);

  return 0;
}

int hal_spi_linux_transfer(hal_spi_device *spi_device, uint32_t length, uint8_t *data)
{
  struct spi_ioc_transfer spi_transfer;

  if (!spi_device->open || spi_device->fd < 1) {
    return -1;
  }

  memset(&spi_transfer, 0, sizeof(struct spi_ioc_transfer));

  spi_transfer.speed_hz = spi_device->speed;
  spi_transfer.bits_per_word = spi_device->bits_per_word;
  spi_transfer.delay_usecs = spi_device->delay_usecs;
  spi_transfer.len = length;
  spi_transfer.tx_buf = (uintptr_t) data;
  spi_transfer.rx_buf = (uintptr_t) data;

  return ioctl(spi_device->fd, SPI_IOC_MESSAGE(1), &spi_transfer);
}

hal_spi_driver hal_spi_driver_linux = {
  .open = hal_spi_linux_open,
  .close = hal_spi_linux_close,
  .transfer = hal_spi_linux_transfer
};
