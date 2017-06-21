/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <poll.h>

#include "ert-hal-serial-posix.h"
#include "ert-log.h"

typedef struct _hal_serial_device_posix {
  int fd;
} hal_serial_device_posix;

static speed_t hal_serial_get_speed(uint32_t speed)
{
  if (speed <= 0) {
    return B0;
  } else if (speed <= 50) {
    return B50;
  } else if (speed <= 75) {
    return B75;
  } else if (speed <= 110) {
    return B110;
  } else if (speed <= 134) {
    return B134;
  } else if (speed <= 150) {
    return B150;
  } else if (speed <= 200) {
    return B200;
  } else if (speed <= 300) {
    return B300;
  } else if (speed <= 600) {
    return B600;
  } else if (speed <= 1200) {
    return B1200;
  } else if (speed <= 1800) {
    return B1800;
  } else if (speed <= 2400) {
    return B2400;
  } else if (speed <= 4800) {
    return B4800;
  } else if (speed <= 9600) {
    return B9600;
  } else if (speed <= 19200) {
    return B19200;
  } else if (speed <= 38400) {
    return B38400;
  } else if (speed <= 57600) {
    return B57600;
  } else if (speed <= 115200) {
    return B115200;
  } else if (speed <= 230400) {
    return B230400;
  } else if (speed <= 460800) {
    return B460800;
  } else if (speed <= 500000) {
    return B500000;
  } else if (speed <= 576000) {
    return B576000;
  } else if (speed <= 1000000) {
    return B1000000;
  } else if (speed <= 1152000) {
    return B1152000;
  } else if (speed <= 1500000) {
    return B1500000;
  } else if (speed <= 2000000) {
    return B2000000;
  } else if (speed <= 2500000) {
    return B2500000;
  } else if (speed <= 3000000) {
    return B3000000;
  } else if (speed <= 3500000) {
    return B3500000;
  } else if (speed <= 4000000) {
    return B4000000;
  }

  return B9600;
}

static int hal_serial_posix_configure(hal_serial_device *serial_device)
{
  hal_serial_device_posix *serial_device_posix = (hal_serial_device_posix *) serial_device->priv;

  struct termios ios = {0};

  int result = tcgetattr(serial_device_posix->fd, &ios);
  if (result != 0) {
    ert_log_error("Error getting serial port attributes: %s", strerror(errno));
    return -EIO;
  }

  speed_t speed = hal_serial_get_speed(serial_device->config.speed);
  result = cfsetospeed(&ios, speed);
  if (result != 0) {
    ert_log_error("Error setting serial port output speed: %s", strerror(errno));
    return -EIO;
  }
  result = cfsetispeed(&ios, speed);
  if (result != 0) {
    ert_log_error("Error setting serial port input speed: %s", strerror(errno));
    return -EIO;
  }

  if (serial_device->config.parity) {
    ios.c_cflag |= PARENB;
  } else {
    ios.c_cflag &= ~PARENB;
  }

  if (serial_device->config.stop_bits_2) {
    ios.c_cflag |= CSTOPB;
  } else {
    ios.c_cflag &= ~CSTOPB;
  }

  ios.c_cflag &= ~CSIZE;
  ios.c_cflag |=  CS8;

  ios.c_cflag &= ~CRTSCTS;
  ios.c_cflag |= CREAD | CLOCAL;

  ios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);

  ios.c_lflag = 0;
  ios.c_oflag = 0;

  ios.c_cc[VMIN] = 1;
  ios.c_cc[VTIME] = 1;

  tcflush(serial_device_posix->fd, TCIFLUSH);

  result = tcsetattr(serial_device_posix->fd, TCSANOW, &ios);
  if (result != 0) {
    ert_log_error("Error setting serial port attributes: %s", strerror(errno));
    return -EIO;
  }

  return 0;
}

int hal_serial_posix_open(hal_serial_device_config *config, hal_serial_device **serial_device_rcv)
{
  int result;

  int fd = open(config->device, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    ert_log_error("Error opening device file '%s': %s", config->device, strerror(errno));
    return -ENOENT;
  }

  hal_serial_device *serial_device;
  serial_device = malloc(sizeof(hal_serial_device));
  if (serial_device == NULL) {
    ert_log_fatal("Error allocating memory for serial device struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_device;
  }

  hal_serial_device_posix *serial_device_posix;
  serial_device_posix = malloc(sizeof(hal_serial_device_posix));
  if (serial_device_posix == NULL) {
    ert_log_fatal("Error allocating memory for serial device private struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_malloc_serial_device;
  }

  memcpy(&serial_device->config, config, sizeof(hal_serial_device_config));
  serial_device->priv = serial_device_posix;

  serial_device_posix->fd = fd;

  result = hal_serial_posix_configure(serial_device);
  if (result < 0) {
    goto error_malloc_serial_device_posix;
  }

  *serial_device_rcv = serial_device;

  return 0;

  error_malloc_serial_device_posix:
  free(serial_device_posix);

  error_malloc_serial_device:
  free(serial_device);

  error_device:
  close(fd);

  return result;
}

int hal_serial_posix_close(hal_serial_device *serial_device)
{
  hal_serial_device_posix *serial_device_posix = (hal_serial_device_posix *) serial_device->priv;

  close(serial_device_posix->fd);
  free(serial_device_posix);
  free(serial_device);

  return 0;
}

int hal_serial_posix_read(hal_serial_device *serial_device, uint32_t timeout_millis, uint32_t length, uint8_t *buffer, uint32_t *bytes_read)
{
  hal_serial_device_posix *serial_device_posix = (hal_serial_device_posix *) serial_device->priv;

  struct pollfd fds[1];
  fds[0].fd = serial_device_posix->fd;
  fds[0].events = POLLIN | POLLPRI;

  int result = poll(fds, 1, timeout_millis);
  if (result < 0) {
    ert_log_error("Call to poll() failed with result %d: %s", result, strerror(errno));
    return -EIO;
  }

  if (result == 0) {
    return -ETIMEDOUT;
  }

  short int revents = fds[0].revents;
  if ((revents & POLLERR) || (revents & POLLHUP) || (revents & POLLNVAL)) {
    ert_log_error("Call to poll() returned failure event: 0x%08X", revents);
    return -EIO;
  }

  if ((revents & POLLIN) || (revents & POLLRDNORM) || (revents & POLLPRI)) {
    ssize_t read_result = read(serial_device_posix->fd, buffer, length);
    if (read_result < 0) {
      ert_log_error("Call to read() failed with result %d: %s", read_result, strerror(errno));
      return -EIO;
    }

    *bytes_read = (uint32_t) read_result;
  } else {
    return -ETIMEDOUT;
  }

  return 0;
}

int hal_serial_posix_write(hal_serial_device *serial_device, uint32_t length, uint8_t *buffer, uint32_t *bytes_written)
{
  hal_serial_device_posix *serial_device_posix = (hal_serial_device_posix *) serial_device->priv;

  ssize_t result = write(serial_device_posix->fd, buffer, length);
  if (result < 0) {
    ert_log_error("Call to write() failed with result %d: %s", result, strerror(errno));
    return -EIO;
  }

  *bytes_written = (uint32_t) result;

  return 0;
}

hal_serial_driver serial_driver_posix = {
    .open = hal_serial_posix_open,
    .close = hal_serial_posix_close,
    .read = hal_serial_posix_read,
    .write = hal_serial_posix_write,
};
