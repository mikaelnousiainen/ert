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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "ert-log.h"
#include "ert-hal-i2c-linux.h"

#define I2C_DEVICE_FILE_NAME_LENGTH 256

const static char *i2c_device_file_template = "/dev/i2c-%d";

int hal_i2c_linux_open(int bus, int device, hal_i2c_device **i2c_device_rcv)
{
  char device_file[I2C_DEVICE_FILE_NAME_LENGTH];
  int fd, retval;
  hal_i2c_device *i2c_device;

  if (snprintf(device_file, I2C_DEVICE_FILE_NAME_LENGTH, i2c_device_file_template, bus) >=
      I2C_DEVICE_FILE_NAME_LENGTH) {
    ert_log_error("Invalid I2C bus: %d", bus);
    return -EINVAL;
  }

  fd = open(device_file, O_RDWR);
  if (fd < 0) {
    ert_log_error("Error opening I2C device file '%s': %s", device_file, strerror(errno));
    return -EIO;
  }

  i2c_device = malloc(sizeof(hal_i2c_device));
  if (i2c_device == NULL) {
    close(fd);
    ert_log_fatal("Error allocating memory for I2C device struct: %s", strerror(errno));
    return -ENOMEM;
  }

  i2c_device->fd = fd;
  i2c_device->bus = bus;
  i2c_device->device = device;

  retval = ioctl(fd, I2C_SLAVE, device);
  if (retval < 0) {
    ert_log_error("Error setting I2C slave device: %s", strerror(errno));
    free(i2c_device);
    close(fd);
    return -EIO;
  }

  i2c_device->open = true;

  *i2c_device_rcv = i2c_device;

  return 0;
}

int hal_i2c_linux_close(hal_i2c_device *i2c_device)
{
  if (i2c_device == NULL) {
    return 0;
  }
  if (!i2c_device->open || i2c_device->fd < 1) {
    return 0;
  }

  i2c_device->open = false;

  close(i2c_device->fd);
  free(i2c_device);

  return 0;
}

static inline int hal_i2c_linux_smbus_access(int fd, uint8_t rw, uint8_t command,
    uint32_t size, union i2c_smbus_data *data)
{
  struct i2c_smbus_ioctl_data ioctl_data;

  ioctl_data.read_write = rw;
  ioctl_data.command = command;
  ioctl_data.size = size;
  ioctl_data.data = data;

  int result = ioctl(fd, I2C_SMBUS, &ioctl_data);
  if (result < 0) {
    return -errno;
  }

  return 0;
}

int hal_i2c_linux_read_byte(hal_i2c_device *i2c_device, uint8_t *value)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  int result = hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
  if (result < 0) {
    return result;
  }

  *value = data.byte;

  return 0;
}

int hal_i2c_linux_write_byte(hal_i2c_device *i2c_device, uint8_t value)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  int result = hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_READ, value, I2C_SMBUS_BYTE, NULL);
  if (result < 0) {
    return result;
  }

  return 0;
}

int hal_i2c_linux_read_byte_command(hal_i2c_device *i2c_device, uint8_t command, uint8_t *value)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  int result = hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data);
  if (result < 0) {
    return result;
  }

  *value = data.byte;

  return 0;
}

int hal_i2c_linux_read_word_command(hal_i2c_device *i2c_device, uint8_t command, uint16_t *value)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  int result = hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_READ, command, I2C_SMBUS_WORD_DATA, &data);
  if (result < 0) {
    return result;
  }

  *value = data.word;

  return 0;
}

int hal_i2c_linux_write_byte_command(hal_i2c_device *i2c_device, uint8_t command, uint8_t value)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  data.byte = value;

  return hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
}

int hal_i2c_linux_write_word_command(hal_i2c_device *i2c_device, uint8_t command, uint16_t value)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  data.word = value;

  return hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA, &data);
}

int hal_i2c_linux_read_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t *values, uint8_t *bytes_read_rcv)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  int result = hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_READ, command, I2C_SMBUS_BLOCK_DATA, &data);
  if (result < 0) {
    return result;
  }

  uint8_t bytes_read = data.block[0];

  for (uint8_t i = 0; i < bytes_read; i++) {
    values[i] = data.block[i + 1];
  }

  if (bytes_read_rcv != NULL) {
    *bytes_read_rcv = bytes_read;
  }

  return 0;
}

int hal_i2c_linux_write_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  if (length > I2C_SMBUS_BLOCK_MAX) {
    length = I2C_SMBUS_BLOCK_MAX;
  }

  data.block[0] = length;
  for (uint8_t i = 0; i < length; i++) {
    data.block[i + 1] = values[i];
  }

  return hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_BLOCK_DATA, &data);
}

int hal_i2c_linux_read_i2c_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values,
    uint8_t *bytes_read_rcv)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  if (length > I2C_SMBUS_BLOCK_MAX) {
    length = I2C_SMBUS_BLOCK_MAX;
  }

  data.block[0] = length;

  int result = hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_READ, command, I2C_SMBUS_I2C_BLOCK_DATA, &data);
  if (result < 0) {
    return result;
  }

  uint8_t bytes_read = data.block[0];

  for (uint8_t i = 0; i < bytes_read; i++) {
    values[i] = data.block[i + 1];
  }

  if (bytes_read_rcv != NULL) {
    *bytes_read_rcv = bytes_read;
  }

  return 0;
}

int hal_i2c_linux_write_i2c_block(hal_i2c_device *i2c_device, uint8_t command, uint8_t length, uint8_t *values)
{
  if (!i2c_device->open || i2c_device->fd < 1) {
    return -EINVAL;
  }

  union i2c_smbus_data data;

  if (length > I2C_SMBUS_BLOCK_MAX) {
    length = I2C_SMBUS_BLOCK_MAX;
  }

  data.block[0] = length;
  for (uint8_t i = 0; i < length; i++) {
    data.block[i + 1] = values[i];
  }

  return hal_i2c_linux_smbus_access(i2c_device->fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_I2C_BLOCK_DATA, &data);
}

hal_i2c_driver hal_i2c_driver_linux = {
    .open = hal_i2c_linux_open,
    .close = hal_i2c_linux_close,
    .read_byte = hal_i2c_linux_read_byte,
    .write_byte = hal_i2c_linux_write_byte,
    .read_byte_command = hal_i2c_linux_read_byte_command,
    .read_word_command = hal_i2c_linux_read_word_command,
    .write_byte_command = hal_i2c_linux_write_byte_command,
    .write_word_command = hal_i2c_linux_write_word_command,
    .read_block = hal_i2c_linux_read_block,
    .write_block = hal_i2c_linux_write_block,
    .read_i2c_block = hal_i2c_linux_read_i2c_block,
    .write_i2c_block = hal_i2c_linux_write_i2c_block,
};
