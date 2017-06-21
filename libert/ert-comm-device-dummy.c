/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>

#include "ert-comm-device-dummy.h"
#include "ert-log.h"

ert_comm_driver ert_comm_driver_dummy;

int ert_comm_driver_dummy_get_frequency_error(ert_comm_device *device, double *frequency_error_hz)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  *frequency_error_hz = 0.0f;

  return 0;
}

int ert_comm_driver_dummy_configure(ert_comm_device *device, ert_comm_driver_dummy_config *config)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  memcpy(&driver->config, config, sizeof(ert_comm_driver_dummy_config));

  return 0;
}

int ert_comm_driver_dummy_configure_generic(ert_comm_device *device, void *config)
{
  return 0;
}

int ert_comm_driver_dummy_set_frequency(ert_comm_device *device, ert_comm_device_config_type type, double frequency)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  return 0;
}

static void ert_driver_comm_device_dummy_transmit_callback(union sigval sv)
{
  ert_comm_device *device = (ert_comm_device *) sv.sival_ptr;
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  ert_driver_comm_device_dummy *other_driver = (ert_driver_comm_device_dummy *) driver->config.other_device->priv;

  device->status.device_state = ERT_COMM_DEVICE_STATE_STANDBY;

  if (!driver->lose_packets) {
    other_driver->inject(driver->config.other_device, driver->transmit_data_length, driver->transmit_data);
  }

  driver->transmit_data_length = 0;
  driver->transmit_active = false;

  if (!driver->no_transmit_callback && driver->config.transmit_callback != NULL) {
    driver->config.transmit_callback(driver->config.callback_context);
  }
}

int ert_comm_driver_dummy_transmit(ert_comm_device *device, uint32_t length, uint8_t *payload, uint32_t *bytes_transmitted)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  if (length > driver->config.max_packet_length) {
    return -EINVAL;
  }
  if (driver->transmit_active) {
    ert_log_error("Dummy device still transmitting");
    return -EBUSY;
  }

  if (driver->fail_transmit) {
    int result = -EIO;
    ert_log_info("Failing transmit with result %d", result);
    return result;
  }

  *bytes_transmitted = length;

  memcpy(driver->transmit_data, payload, length);
  driver->transmit_data_length = length;

  struct itimerspec ts = {0};

  int result = timer_settime(driver->transmit_callback_timer, 0, &ts, NULL);
  if (result < 0) {
    ert_log_error("Error stopping transmit callback timer timeout: %s", strerror(errno));
    return -EIO;
  }

  uint32_t millis = driver->config.transmit_time_millis;
  if (millis > 0) {
    ts.it_value.tv_sec = ((long) millis / 1000L);
    ts.it_value.tv_nsec = ((long) millis % 1000L) * 1000000L;
  } else {
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 1;
  }

  device->status.device_state = ERT_COMM_DEVICE_STATE_TRANSMIT;

  driver->transmit_active = true;

  result = timer_settime(driver->transmit_callback_timer, 0, &ts, NULL);
  if (result < 0) {
    device->status.device_state = ERT_COMM_DEVICE_STATE_STANDBY;
    driver->transmit_active = false;
    ert_log_error("Error setting transmit callback timer timeout: %s", strerror(errno));
    return -EIO;
  }

  return 0;
}

int ert_comm_driver_dummy_wait_for_transmit(ert_comm_device *device, uint32_t milliseconds)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  if (device->status.device_state != ERT_COMM_DEVICE_STATE_TRANSMIT) {
    return -EIO;
  }

  device->status.device_state = ERT_COMM_DEVICE_STATE_STANDBY;

  return 0;
}

int ert_comm_driver_dummy_wait_for_data(ert_comm_device *device, uint32_t milliseconds)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  if (device->status.device_state != ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS &&
      device->status.device_state != ERT_COMM_DEVICE_STATE_RECEIVE_SINGLE) {
    return -EIO;
  }

  device->status.device_state = ERT_COMM_DEVICE_STATE_STANDBY;

  return 0;
}

int ert_comm_driver_dummy_start_receive(ert_comm_device *device, bool continuous)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  device->status.device_state = continuous ?
                                ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS :
                                ERT_COMM_DEVICE_STATE_RECEIVE_SINGLE;

  return 0;
}

int ert_comm_driver_dummy_wait_for_detection(ert_comm_device *device, uint32_t milliseconds)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  if (device->status.device_state != ERT_COMM_DEVICE_STATE_DETECTION) {
    return -EIO;
  }

  device->status.device_state = ERT_COMM_DEVICE_STATE_STANDBY;

  return 0;
}

int ert_comm_driver_dummy_start_detection(ert_comm_device *device)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  device->status.device_state = ERT_COMM_DEVICE_STATE_DETECTION;

  return 0;
}

int ert_comm_driver_dummy_receive(ert_comm_device *device, uint32_t buffer_length, uint8_t *buffer, uint32_t *bytes_received)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  if (driver->receive_data_length == 0) {
    *bytes_received = 0;
    return 0;
  }

  if (driver->fail_receive) {
    return -EBADMSG;
  }

  uint32_t bytes_to_transfer =
      (buffer_length < driver->receive_data_length) ? buffer_length : driver->receive_data_length;

  memcpy(buffer, driver->receive_data, bytes_to_transfer);
  *bytes_received = bytes_to_transfer;

  driver->receive_data_length = 0;

  return 0;
}

int ert_comm_driver_dummy_read_status(ert_comm_device *device)
{
  return 0;
}

int ert_comm_driver_dummy_get_status(ert_comm_device *device, ert_comm_device_status *status) {
  memcpy(status, &device->status, sizeof(ert_comm_device_status));

  return 0;
}

uint32_t ert_comm_driver_dummy_get_max_packet_length(ert_comm_device *device) {
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;
  return driver->config.max_packet_length;
}

int ert_comm_driver_dummy_set_receive_callback(ert_comm_device *device, ert_comm_driver_callback receive_callback)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  driver->config.receive_callback = receive_callback;

  return 0;
}

int ert_comm_driver_dummy_set_transmit_callback(ert_comm_device *device, ert_comm_driver_callback transmit_callback)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  driver->config.transmit_callback = transmit_callback;

  return 0;
}

int ert_comm_driver_dummy_set_detection_callback(ert_comm_device *device, ert_comm_driver_callback detection_callback)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  driver->config.detection_callback = detection_callback;

  return 0;
}

int ert_comm_driver_dummy_set_callback_context(ert_comm_device *device, void *callback_context)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  driver->config.callback_context = callback_context;

  return 0;
}

int ert_comm_driver_dummy_inject(ert_comm_device *device, uint32_t length, uint8_t *data)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  memcpy(driver->receive_data, data, length);
  driver->receive_data_length = length;

  driver->config.receive_callback(driver->config.callback_context);

  return 0;
}

int ert_driver_comm_device_dummy_connect(ert_comm_device *device, ert_comm_device *other_device)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;
  driver->config.other_device = other_device;

  ert_driver_comm_device_dummy *other_driver = (ert_driver_comm_device_dummy *) other_device->priv;
  other_driver->config.other_device = device;

  return 0;
}

void ert_driver_comm_device_dummy_set_fail_transmit(ert_comm_device *device, bool fail_transmit)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;
  driver->fail_transmit = fail_transmit;
}

void ert_driver_comm_device_dummy_set_fail_receive(ert_comm_device *device, bool fail_receive)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;
  driver->fail_receive = fail_receive;
}

void ert_driver_comm_device_dummy_set_lose_packets(ert_comm_device *device, bool lose_packets)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;
  driver->lose_packets = lose_packets;
}

void ert_driver_comm_device_dummy_set_no_transmit_callback(ert_comm_device *device, bool no_transmit_callback)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;
  driver->no_transmit_callback = no_transmit_callback;
}

int ert_driver_comm_device_dummy_open(ert_comm_driver_dummy_config *config, ert_comm_device **device_rcv)
{
  int result;
  ert_comm_device *device;
  ert_driver_comm_device_dummy *driver;

  device = malloc(sizeof(ert_comm_device));
  if (device == NULL) {
    ert_log_fatal("Error allocating memory for comm device struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_first;
  }

  memset(device, 0, sizeof(ert_comm_device));

  driver = malloc(sizeof(ert_driver_comm_device_dummy));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for driver struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_device_malloc;
  }

  memset(driver, 0, sizeof(ert_driver_comm_device_dummy));

  driver->transmit_data = malloc(config->max_packet_length);
  if (driver->transmit_data == NULL) {
    ert_log_fatal("Error allocating memory for driver transmit data: %s", strerror(errno));
    result = -ENOMEM;
    goto error_driver_malloc;
  }

  driver->receive_data = malloc(config->max_packet_length);
  if (driver->receive_data == NULL) {
    ert_log_fatal("Error allocating memory for driver receive data: %s", strerror(errno));
    result = -ENOMEM;
    goto error_transmit_data_malloc;
  }

  struct sigevent transmit_callback_timer_event;

  transmit_callback_timer_event.sigev_notify = SIGEV_THREAD;
  transmit_callback_timer_event.sigev_notify_function = ert_driver_comm_device_dummy_transmit_callback;
  transmit_callback_timer_event.sigev_notify_attributes = NULL;
  transmit_callback_timer_event.sigev_value.sival_ptr = device;

  result = timer_create(CLOCK_REALTIME, &transmit_callback_timer_event, &driver->transmit_callback_timer);
  if (result < 0) {
    ert_log_error("Error creating transmit callback timer: %s", strerror(errno));
    result = -EIO;
    goto error_receive_data_malloc;
  }

  memcpy(&driver->config, config, sizeof(ert_comm_driver_dummy_config));
  driver->inject = ert_comm_driver_dummy_inject;
  device->driver = &ert_comm_driver_dummy;
  device->priv = driver;

  device->status.name = "Dummy";
  device->status.model = "Dummy model";
  device->status.manufacturer = "Dummy manufacturer";
  device->status.custom = NULL;

  device->status.device_state = ERT_COMM_DEVICE_STATE_SLEEP;

  *device_rcv = device;

  return 0;

  error_receive_data_malloc:
  free(driver->receive_data);

  error_transmit_data_malloc:
  free(driver->transmit_data);

  error_driver_malloc:
  free(driver);

  error_device_malloc:
  free(device);

  error_first:

  return result;
}

int ert_driver_comm_device_dummy_standby(ert_comm_device *device)
{
  device->status.device_state = ERT_COMM_DEVICE_STATE_STANDBY;
  return 0;
}

int ert_driver_comm_device_dummy_sleep(ert_comm_device *device)
{
  device->status.device_state = ERT_COMM_DEVICE_STATE_SLEEP;
  return 0;
}

int ert_driver_comm_device_dummy_close(ert_comm_device *device)
{
  ert_driver_comm_device_dummy *driver = (ert_driver_comm_device_dummy *) device->priv;

  device->driver->standby(device);
  device->driver->sleep(device);

  int result = timer_delete(driver->transmit_callback_timer);
  if (result < 0) {
    ert_log_error("Error deleting transmit callback timer: %s", strerror(errno));
  }

  free(driver->receive_data);
  free(driver->transmit_data);
  free(driver);
  free(device);

  return 0;
}

ert_comm_driver ert_comm_driver_dummy = {
    .transmit = ert_comm_driver_dummy_transmit,
    .wait_for_transmit = ert_comm_driver_dummy_wait_for_transmit,
    .start_detection = ert_comm_driver_dummy_start_detection,
    .wait_for_detection = ert_comm_driver_dummy_wait_for_detection,
    .start_receive = ert_comm_driver_dummy_start_receive,
    .wait_for_data = ert_comm_driver_dummy_wait_for_data,
    .receive = ert_comm_driver_dummy_receive,
    .configure = ert_comm_driver_dummy_configure_generic,
    .set_frequency = ert_comm_driver_dummy_set_frequency,
    .get_frequency_error = ert_comm_driver_dummy_get_frequency_error,
    .standby = ert_driver_comm_device_dummy_standby,
    .read_status = ert_comm_driver_dummy_read_status,
    .get_status = ert_comm_driver_dummy_get_status,
    .get_max_packet_length = ert_comm_driver_dummy_get_max_packet_length,
    .set_receive_callback = ert_comm_driver_dummy_set_receive_callback,
    .set_transmit_callback = ert_comm_driver_dummy_set_transmit_callback,
    .set_detection_callback = ert_comm_driver_dummy_set_detection_callback,
    .set_callback_context = ert_comm_driver_dummy_set_callback_context,
    .sleep = ert_driver_comm_device_dummy_sleep,
    .close = ert_driver_comm_device_dummy_close,
};
