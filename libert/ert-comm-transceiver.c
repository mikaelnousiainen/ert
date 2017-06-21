/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <unistd.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-comm-transceiver.h"

typedef struct _ert_comm_transceiver_packet_receive_buffer_metadata_queue_entry {
  uint32_t length;
  uint8_t *buffer;
} ert_comm_transceiver_packet_receive_buffer_metadata_queue_entry;

typedef struct _ert_comm_transceiver_packet_transmit_buffer_metadata {
  bool transmitted_signal;
  uint32_t bytes_transmitted;
  pthread_mutex_t transmitted_mutex;
  pthread_cond_t transmitted_cond;
  bool destroy_signal;
  bool timeout;
  int result;
} ert_comm_transceiver_packet_transmit_buffer_metadata;

typedef struct _ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry {
  uint32_t id;
  uint32_t length;
  uint8_t *buffer;

  bool set_receive_active;

  bool blocking_enabled;
  bool *transmitted_signal;
  uint32_t *bytes_transmitted;
  pthread_mutex_t *transmitted_mutex;
  pthread_cond_t *transmitted_cond;
  bool *destroy_signal;
  bool *timeout;
  int *result;

  ert_comm_transceiver_packet_transmit_buffer_metadata *metadata;
} ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry;

typedef bool (*cond_func)(ert_comm_transceiver *);

typedef enum _ert_comm_transceiver_counter_type {
  ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT = 1,
  ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE,
  ERT_COMM_TRANSCEIVER_COUNTER_TYPE_RECEIVE_INVALID,
} ert_comm_transceiver_counter_type;

static int ert_comm_transceiver_increment_counter(ert_comm_transceiver *comm_transceiver,
    ert_comm_transceiver_counter_type type, uint64_t packet_bytes)
{
  int result = 0;

  pthread_mutex_lock(&comm_transceiver->status_mutex);

  ert_comm_transceiver_status *status = &comm_transceiver->status;

  switch (type) {
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT:
      ert_get_current_timestamp(&status->last_transmitted_packet_timestamp);
      status->transmitted_packet_count++;
      status->transmitted_bytes += packet_bytes;
      break;
    case ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE:
      ert_get_current_timestamp(&status->last_received_packet_timestamp);
      status->received_packet_count++;
      status->received_bytes += packet_bytes;
      break;
    case ERT_COMM_TRANSCEIVER_COUNTER_TYPE_RECEIVE_INVALID:
      ert_get_current_timestamp(&status->last_invalid_received_packet_timestamp);
      status->invalid_received_packet_count++;
      break;
    default:
      ert_log_error("Invalid counter type: %d", type);
      result = -EINVAL;
      break;
  }

  pthread_mutex_unlock(&comm_transceiver->status_mutex);

  return result;
}

static int ert_comm_transceiver_cond_timedwait(ert_comm_transceiver *transceiver,
    pthread_cond_t *cond, pthread_mutex_t *mutex, uint32_t milliseconds, cond_func is_condition_met)
{
  struct timespec to;

  int result = ert_get_current_timestamp_offset(&to, milliseconds);
  if (result < 0) {
    return -EIO;
  }

  pthread_mutex_lock(mutex);
  if (!is_condition_met(transceiver)) {
    result = pthread_cond_timedwait(cond, mutex, &to);
  }
  pthread_mutex_unlock(mutex);

  if (result == ETIMEDOUT) {
    return -ETIMEDOUT;
  } else if (result != 0) {
    ert_log_error("pthread_cond_timedwait failed with result %d", result);
    return -EIO;
  }

  return 0;
}

static int ert_comm_transceiver_cond_signal(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  int result;

  pthread_mutex_lock(mutex);
  result = pthread_cond_broadcast(cond);
  pthread_mutex_unlock(mutex);

  return result;
}

static int ert_comm_transceiver_signal_event(ert_comm_transceiver *transceiver)
{
  transceiver->event_signal = true;

  return ert_comm_transceiver_cond_signal(&transceiver->event_cond, &transceiver->event_mutex);
}

static bool ert_comm_transceiver_is_event_signal_active(ert_comm_transceiver *transceiver)
{
  return transceiver->event_signal;
}

static int ert_comm_transceiver_wait_for_event(ert_comm_transceiver *transceiver)
{
  int result = ert_comm_transceiver_cond_timedwait(transceiver,
      &transceiver->event_cond, &transceiver->event_mutex, transceiver->config.poll_interval_milliseconds,
      ert_comm_transceiver_is_event_signal_active);
  transceiver->event_signal = false;

  return result;
}

static int ert_comm_transceiver_start_receive(ert_comm_transceiver *transceiver)
{
  pthread_mutex_lock(&transceiver->device_mutex);
  int result = transceiver->device->driver->start_receive(transceiver->device, true);
  pthread_mutex_unlock(&transceiver->device_mutex);

  if (result == 0) {
    ert_get_current_timestamp(&transceiver->status.comm_device_receive_mode_started_timestamp);
  } else {
    ert_log_error("Error starting comm device receive mode");
  }

  return result;
}

static int ert_comm_transceiver_sleep(ert_comm_transceiver *transceiver)
{
  pthread_mutex_lock(&transceiver->device_mutex);
  int result = transceiver->device->driver->sleep(transceiver->device);
  pthread_mutex_unlock(&transceiver->device_mutex);

  if (result != 0) {
    ert_log_error("Error making comm device sleep");
  }

  return result;
}

static void ert_comm_transceiver_device_receive_callback(void *context)
{
  ert_comm_transceiver *transceiver = (ert_comm_transceiver *) context;
  ert_comm_device *device = transceiver->device;
  ert_comm_driver *driver = transceiver->device->driver;

  ert_log_debug("Storing packet");

  ert_comm_transceiver_packet_receive_buffer_metadata_queue_entry packet_buffer = {0};

  int result = ert_buffer_pool_acquire(transceiver->receive_buffer_pool, (void **) &packet_buffer.buffer);
  if (result != 0) {
    ert_log_error("Receive buffer queue overflow: exceeded capacity of %d packets",
        transceiver->config.receive_buffer_length_packets);
    return;
  }

  uint32_t bytes_received = 0;
  pthread_mutex_lock(&transceiver->device_mutex);
  result = driver->receive(device, transceiver->max_packet_length, packet_buffer.buffer, &bytes_received);
  pthread_mutex_unlock(&transceiver->device_mutex);

  if (result < 0) {
    ert_buffer_pool_release(transceiver->receive_buffer_pool, packet_buffer.buffer);
    ert_comm_transceiver_increment_counter(transceiver, ERT_COMM_TRANSCEIVER_COUNTER_TYPE_RECEIVE_INVALID, 0);
    ert_log_error("Error receiving data from comm device, result %d", result);
    return;
  }

  ert_log_debug("Stored %d bytes to packet buffer", bytes_received);

  packet_buffer.length = bytes_received;

  ert_pipe_push(transceiver->receive_buffer_queue, &packet_buffer, 1);
}

static void ert_comm_transceiver_device_transmit_callback(void *context)
{
  ert_comm_transceiver *transceiver = (ert_comm_transceiver *) context;

  ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry packet_buffer_metadata_queue_entry;

  ssize_t result = ert_pipe_pop_timed(transceiver->transmit_wait_queue, &packet_buffer_metadata_queue_entry, 1, 1);
  if (result < 0) {
    ert_log_error("BUG: ert_pipe_pop_timed timed out for transmit_wait_queue -- no packets in queue, but received transmit callback call");
    return;
  }
  if (result == 0) {
    ert_log_warn("Pipe closed for ert_pipe_pop_timed for transmit_wait_queue");
    return;
  }

  ert_log_debug("Transmit callback: pushing packet to transmit result queue: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
      packet_buffer_metadata_queue_entry.id, *packet_buffer_metadata_queue_entry.transmitted_signal,
      *packet_buffer_metadata_queue_entry.bytes_transmitted);

  ert_pipe_push(transceiver->transmit_result_queue, &packet_buffer_metadata_queue_entry, 1);
}

static void ert_comm_transceiver_handle_mode_change(ert_comm_transceiver *transceiver)
{
  ert_comm_device *device = transceiver->device;

  if (transceiver->receive_active
      && (device->status.device_state != ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS)) {
    ert_log_debug("Mode: receive");

    pthread_mutex_lock(&transceiver->transmit_mutex);
    ert_comm_transceiver_start_receive(transceiver);
    pthread_mutex_unlock(&transceiver->transmit_mutex);

    ert_log_debug("Mode: changed to receive");
  } else if (!transceiver->receive_active && !transceiver->transmit_active
      && (device->status.device_state == ERT_COMM_DEVICE_STATE_STANDBY
          || device->status.device_state == ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS)) {
    ert_log_debug("Mode: sleep");

    pthread_mutex_lock(&transceiver->transmit_mutex);
    ert_comm_transceiver_sleep(transceiver);
    pthread_mutex_unlock(&transceiver->transmit_mutex);

    ert_log_debug("Mode: changed to sleep");
  }
}

static void ert_comm_transceiver_handle_config_change(ert_comm_transceiver *transceiver)
{
  if (transceiver->new_config == NULL) {
    return;
  }

  ert_comm_device *device = transceiver->device;
  ert_comm_driver *driver = transceiver->device->driver;

  pthread_mutex_lock(&transceiver->transmit_mutex);
  pthread_mutex_lock(&transceiver->device_mutex);
  int result = driver->configure(device, (void *) transceiver->new_config);
  pthread_mutex_unlock(&transceiver->device_mutex);
  pthread_mutex_unlock(&transceiver->transmit_mutex);

  if (result < 0) {
    ert_log_error("Error configuring comm device, result %d", result);
  } else {
    ert_log_info("Comm device configuration changed successfully");
  }

  if (transceiver->config.event_callback != NULL) {
    transceiver->config.event_callback(ERT_COMM_TRANSCEIVER_EVENT_CONFIGURATION_CHANGED, result,
        transceiver->config.event_callback_context);
  }

  transceiver->new_config = NULL;
}

static void ert_comm_transceiver_handle_frequency_change(ert_comm_transceiver *transceiver)
{
  if (transceiver->new_frequency == 0) {
    return;
  }

  ert_comm_device *device = transceiver->device;
  ert_comm_driver *driver = transceiver->device->driver;

  pthread_mutex_lock(&transceiver->transmit_mutex);
  pthread_mutex_lock(&transceiver->device_mutex);
  int result = driver->set_frequency(device, transceiver->new_frequency_config_type, transceiver->new_frequency);
  pthread_mutex_unlock(&transceiver->device_mutex);
  pthread_mutex_unlock(&transceiver->transmit_mutex);

  if (result < 0) {
    ert_log_error("Error setting comm device frequency, result %d", result);
  } else {
    ert_log_info("Comm device frequency changed to: %f Hz", transceiver->new_frequency);
  }

  if (transceiver->config.event_callback != NULL) {
    transceiver->config.event_callback(ERT_COMM_TRANSCEIVER_EVENT_FREQUENCY_CHANGED, result,
        transceiver->config.event_callback_context);
  }

  transceiver->new_frequency = 0;
}

static void ert_comm_transceiver_handle_read_status(ert_comm_transceiver *transceiver)
{
  ert_comm_device *device = transceiver->device;
  ert_comm_driver *driver = transceiver->device->driver;

  pthread_mutex_lock(&transceiver->device_mutex);
  int result = driver->read_status(device);
  pthread_mutex_unlock(&transceiver->device_mutex);

  if (result == 0) {
    pthread_mutex_lock(&transceiver->status_mutex);
    driver->get_status(device, &transceiver->device_status);
    pthread_mutex_unlock(&transceiver->status_mutex);

    if (transceiver->config.event_callback != NULL) {
      transceiver->config.event_callback(ERT_COMM_TRANSCEIVER_EVENT_STATUS_READ, result,
          transceiver->config.event_callback_context);
    }
  } else {
    ert_log_error("Error reading comm device status");
  }
}

static void *ert_comm_transceiver_maintenance_routine(void *context)
{
  ert_comm_transceiver *transceiver = (ert_comm_transceiver *) context;

  while (transceiver->running) {
    ert_comm_transceiver_handle_mode_change(transceiver);

    ert_comm_transceiver_wait_for_event(transceiver);
    ert_log_debug("Maintenance: wait for event woke up");

    ert_comm_transceiver_handle_config_change(transceiver);
    ert_comm_transceiver_handle_frequency_change(transceiver);

    ert_comm_transceiver_handle_read_status(transceiver);
  }

  return NULL;
}

static bool ert_comm_transceiver_has_maximum_receive_time_been_reached(ert_comm_transceiver *transceiver)
{
  if (transceiver->config.maximum_receive_time_milliseconds == 0) {
    return false;
  }

  struct timespec current_time;
  int result = ert_get_current_timestamp(&current_time);
  if (result != 0) {
    ert_log_error("ert_get_current_timestamp failed with result %d", result);
    return true;
  }

  int32_t receive_time_millis = ert_timespec_diff_milliseconds(
      &transceiver->status.comm_device_receive_mode_started_timestamp, &current_time);

  bool maximum_receive_time_reached =
      (receive_time_millis > transceiver->config.maximum_receive_time_milliseconds);

  ert_log_debug("Checking maximum receive time: %d > %d ms = %d", receive_time_millis,
      transceiver->config.maximum_receive_time_milliseconds, maximum_receive_time_reached);

  return maximum_receive_time_reached;
}

static void ert_comm_transceiver_receive_while_receive_active(ert_comm_transceiver *transceiver)
{
  ert_comm_device *device = transceiver->device;
  int result;

  while (transceiver->running && transceiver->receive_active) {
    ert_log_debug("Mode: receive");
    if (device->status.device_state != ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS) {
      result = ert_comm_transceiver_start_receive(transceiver);
      if (result != 0) {
        return;
      }
    }

    ert_comm_transceiver_wait_for_event(transceiver);
    ert_log_debug("Transmit: wait for event woke up");

    if (!transceiver->receive_active) {
      ert_log_debug("Mode: transmit");
    }

    if (ert_comm_transceiver_has_maximum_receive_time_been_reached(transceiver)) {
      ert_log_error("Maximum comm device receive time of %d ms has been reached, switching back to transmit mode",
          transceiver->config.maximum_receive_time_milliseconds);
      result = ert_comm_transceiver_set_receive_active(transceiver, false);
      if (result != 0) {
        ert_log_error("ert_comm_transceiver_set_receive_active failed with result %d", result);
      }
    }
  }
}

static void ert_comm_transceiver_release_transmit_buffer(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry *packet_buffer_metadata_queue_entry)
{
  if (packet_buffer_metadata_queue_entry->blocking_enabled) {
    pthread_cond_destroy(packet_buffer_metadata_queue_entry->transmitted_cond);
    pthread_mutex_destroy(packet_buffer_metadata_queue_entry->transmitted_mutex);
  }

  ert_buffer_pool_release(transceiver->transmit_buffer_pool, packet_buffer_metadata_queue_entry->buffer);
  ert_buffer_pool_release(transceiver->transmit_buffer_metadata_pool, packet_buffer_metadata_queue_entry->metadata);
}

static void ert_comm_transceiver_signal_transmit_and_release_buffer(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry *packet_buffer_metadata_queue_entry,
    int transmit_result, uint32_t bytes_transmitted, bool timed_out)
{
  int result;

  timeout:
  if (timed_out || *packet_buffer_metadata_queue_entry->timeout) {
    ert_log_warn("Timeout detected for packet id=%d, releasing buffer", packet_buffer_metadata_queue_entry->id);
    pthread_mutex_unlock(packet_buffer_metadata_queue_entry->transmitted_mutex);

    ert_comm_transceiver_release_transmit_buffer(transceiver, packet_buffer_metadata_queue_entry);
    return;
  }

  if (!packet_buffer_metadata_queue_entry->blocking_enabled) {
    ert_buffer_pool_release(transceiver->transmit_buffer_pool, packet_buffer_metadata_queue_entry->buffer);
    ert_buffer_pool_release(transceiver->transmit_buffer_metadata_pool, packet_buffer_metadata_queue_entry->metadata);
    return;
  }

  size_t retries = 3;
  retry_lock:
  result = pthread_mutex_trylock(packet_buffer_metadata_queue_entry->transmitted_mutex);
  if (result == EBUSY) {
    ert_log_warn("Packet buffer mutex locked for packet id=%d, waiting retrying %d times...",
        packet_buffer_metadata_queue_entry->id, retries);

    sleep(1);

    retries--;
    if (retries < 1) {
      ert_log_error("Packet buffer mutex timeout for packet id=%d, releasing buffer",
          packet_buffer_metadata_queue_entry->id);
      timed_out = true;
      goto timeout;
    }

    goto retry_lock;
  } else if (result != 0) {
    ert_log_error("pthread_mutex_trylock failed for packet buffer mutex id=%d, result %d, releasing buffer",
        packet_buffer_metadata_queue_entry->id, result);
    timed_out = true;
    goto timeout;
  }

  *packet_buffer_metadata_queue_entry->result = transmit_result;
  *packet_buffer_metadata_queue_entry->bytes_transmitted = bytes_transmitted;
  *packet_buffer_metadata_queue_entry->transmitted_signal = true;
  pthread_cond_signal(packet_buffer_metadata_queue_entry->transmitted_cond);
  pthread_mutex_unlock(packet_buffer_metadata_queue_entry->transmitted_mutex);

  struct timespec to;
  result = ert_get_current_timestamp_offset(&to, transceiver->config.transmit_timeout_milliseconds);
  if (result < 0) {
    ert_comm_transceiver_release_transmit_buffer(transceiver, packet_buffer_metadata_queue_entry);
    return;
  }

  ert_log_debug("Preparing to wait for packet destruction: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
      packet_buffer_metadata_queue_entry->id, *packet_buffer_metadata_queue_entry->transmitted_signal,
      *packet_buffer_metadata_queue_entry->bytes_transmitted);

  pthread_mutex_lock(packet_buffer_metadata_queue_entry->transmitted_mutex);

  while (!*packet_buffer_metadata_queue_entry->destroy_signal) {
    ert_log_debug("Waiting for packet destruction: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
        packet_buffer_metadata_queue_entry->id, *packet_buffer_metadata_queue_entry->transmitted_signal,
        *packet_buffer_metadata_queue_entry->bytes_transmitted);
    result = pthread_cond_timedwait(packet_buffer_metadata_queue_entry->transmitted_cond,
        packet_buffer_metadata_queue_entry->transmitted_mutex, &to);

    if (result == ETIMEDOUT) {
      ert_log_error("Timeout waiting for packet destruction: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
          packet_buffer_metadata_queue_entry->id, *packet_buffer_metadata_queue_entry->transmitted_signal,
          *packet_buffer_metadata_queue_entry->bytes_transmitted);
      break;
    } else if (result != 0) {
      ert_log_error("Error waiting for destruction for packet id=%d, result %d", result);
      break;
    }
  }

  pthread_mutex_unlock(packet_buffer_metadata_queue_entry->transmitted_mutex);

  ert_comm_transceiver_release_transmit_buffer(transceiver, packet_buffer_metadata_queue_entry);
}

static void *ert_comm_transceiver_transmit_dispatch_routine(void *context)
{
  ert_comm_transceiver *transceiver = (ert_comm_transceiver *) context;
  ert_comm_device *device = transceiver->device;
  ert_comm_driver *driver = transceiver->device->driver;
  int result;

  while (transceiver->running) {
    ert_comm_transceiver_receive_while_receive_active(transceiver);
    if (!transceiver->running) {
      break;
    }

    ert_log_debug("Transmit dispatch routine: queue=%s op=%s", "transmit_buffer", "pop_wait");

    ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry packet_buffer_metadata_queue_entry_incoming;
    ssize_t pop_result = ert_pipe_pop(transceiver->transmit_buffer_queue, &packet_buffer_metadata_queue_entry_incoming, 1);
    if (pop_result == 0) {
      break;
    }

    ert_comm_transceiver_receive_while_receive_active(transceiver);
    if (!transceiver->running) {
      break;
    }

    ert_log_debug("Transmit dispatch routine: queue=%s op=%s packet_id=%d set_receive_active=%d",
        "transmit_buffer", "pop", packet_buffer_metadata_queue_entry_incoming.id, packet_buffer_metadata_queue_entry_incoming.set_receive_active);

    transceiver->transmit_active = true;
    pthread_mutex_lock(&transceiver->transmit_mutex);

    uint32_t bytes_transmitted = 0;
    pthread_mutex_lock(&transceiver->device_mutex);
    result = driver->transmit(device, packet_buffer_metadata_queue_entry_incoming.length,
        packet_buffer_metadata_queue_entry_incoming.buffer, &bytes_transmitted);
    pthread_mutex_unlock(&transceiver->device_mutex);

    if (result < 0) {
      transceiver->transmit_active = false;
      pthread_mutex_unlock(&transceiver->transmit_mutex);

      ert_log_error("Error transmitting data using comm device, transmit result %d", result);

      if (transceiver->config.transmit_callback != NULL) {
        // TODO: indicate transmission failure explicitly for transmit_callback
        transceiver->config.transmit_callback(packet_buffer_metadata_queue_entry_incoming.id, 0, transceiver->config.transmit_callback_context);
      }

      ert_comm_transceiver_signal_transmit_and_release_buffer(
          transceiver, &packet_buffer_metadata_queue_entry_incoming, -EIO, 0, false);
      continue;
    }

    ert_log_debug("Transmit dispatch routine: queue=%s op=%s packet_id=%d set_receive_active=%d",
        "transmit_wait", "push", packet_buffer_metadata_queue_entry_incoming.id, packet_buffer_metadata_queue_entry_incoming.set_receive_active);

    ert_pipe_push(transceiver->transmit_wait_queue, &packet_buffer_metadata_queue_entry_incoming, 1);

    ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry packet_buffer_metadata_queue_entry_result;

    pop_result = ert_pipe_pop_timed(transceiver->transmit_result_queue, &packet_buffer_metadata_queue_entry_result, 1,
        transceiver->config.transmit_timeout_milliseconds);
    transceiver->transmit_active = false;
    pthread_mutex_unlock(&transceiver->transmit_mutex);
    if (pop_result < 0) {
      ert_log_error("ert_pipe_pop_timed failed or timed out for transmit_result_queue, result %d", pop_result);

      // Pop timed out entry from queue
      ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry extra_queue_entry;
      pop_result = ert_pipe_pop_timed(transceiver->transmit_wait_queue, &extra_queue_entry, 1, 1);
      if (pop_result > 0) {
        ert_log_warn("ert_pipe_pop_timed timed out for transmit_result_queue, removed %d packets from transmit_wait_queue", pop_result);
      } else if (pop_result < 0) {
        ert_log_error("ert_pipe_pop_timed timed out for transmit_result_queue, attempted to empty transmit_wait_queue, but no packets found");
      }

      ert_comm_transceiver_signal_transmit_and_release_buffer(
          transceiver, &packet_buffer_metadata_queue_entry_incoming, -ETIMEDOUT, 0, true);

      if (pop_result == 0) {
        break;
      }

      continue;
    }
    if (pop_result == 0) {
      break;
    }

    ert_comm_transceiver_increment_counter(transceiver, ERT_COMM_PROTOCOL_COUNTER_TYPE_TRANSMIT, bytes_transmitted);

    ert_log_debug("Transmit dispatch routine: queue=%s op=%s packet_id=%d set_receive_active=%d",
        "transmit_result", "pop", packet_buffer_metadata_queue_entry_result.id, packet_buffer_metadata_queue_entry_result.set_receive_active);

    if (transceiver->config.transmit_callback != NULL) {
      transceiver->config.transmit_callback(packet_buffer_metadata_queue_entry_result.id, packet_buffer_metadata_queue_entry_result.length,
          transceiver->config.transmit_callback_context);
    }

    if (packet_buffer_metadata_queue_entry_result.set_receive_active) {
      ert_comm_transceiver_set_receive_active(transceiver, true);
    }

    if (packet_buffer_metadata_queue_entry_result.blocking_enabled) {
      ert_log_debug("Transmit dispatch routine: op=%s packet_id=%d set_receive_active=%d",
          "notify", packet_buffer_metadata_queue_entry_result.id, packet_buffer_metadata_queue_entry_result.set_receive_active);

      ert_comm_transceiver_signal_transmit_and_release_buffer(
          transceiver, &packet_buffer_metadata_queue_entry_result, 0, bytes_transmitted, false);
    }
  }

  return NULL;
}

static void *ert_comm_transceiver_receive_dispatch_routine(void *context)
{
  ert_comm_transceiver *transceiver = (ert_comm_transceiver *) context;

  while (transceiver->running) {
    ert_comm_transceiver_packet_receive_buffer_metadata_queue_entry packet_buffer;

    size_t pop_result = ert_pipe_pop(transceiver->receive_buffer_queue, &packet_buffer, 1);
    if (pop_result == 0) {
      break;
    }

    ert_comm_transceiver_increment_counter(transceiver, ERT_COMM_PROTOCOL_COUNTER_TYPE_RECEIVE, packet_buffer.length);

    if (transceiver->config.receive_callback != NULL) {
      transceiver->config.receive_callback(packet_buffer.length, packet_buffer.buffer,
          transceiver->config.receive_callback_context);
    }

    ert_buffer_pool_release(transceiver->receive_buffer_pool, packet_buffer.buffer);
  }

  return NULL;
}

int ert_comm_transceiver_start(ert_comm_device *device,
    ert_comm_transceiver_config *transceiver_config,
    ert_comm_transceiver **transceiver_rcv)
{
  ert_comm_transceiver *transceiver;
  int result;

  transceiver = calloc(1, sizeof(ert_comm_transceiver));
  if (transceiver == NULL) {
    ert_log_fatal("Error allocating memory for comm transceiver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(&transceiver->config, transceiver_config, sizeof(ert_comm_transceiver_config));

  transceiver->device = device;
  transceiver->receive_active = false;
  transceiver->transmit_active = false;
  transceiver->running = false;

  transceiver->max_packet_length = device->driver->get_max_packet_length(device);

  result = pthread_mutex_init(&transceiver->transmit_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing transmit mutex");
    goto error_transceiver;
  }
  result = pthread_mutex_init(&transceiver->device_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing device mutex");
    goto error_transmit_mutex;
  }
  result = pthread_mutex_init(&transceiver->status_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing status mutex");
    goto error_device_mutex;
  }

  result = pthread_mutex_init(&transceiver->event_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing event mutex");
    goto error_status_mutex;
  }
  result = pthread_cond_init(&transceiver->event_cond, NULL);
  if (result != 0) {
    ert_log_error("Error initializing event condition");
    goto error_event_mutex;
  }

  result = ert_buffer_pool_create(transceiver->max_packet_length, transceiver->config.receive_buffer_length_packets,
      &transceiver->receive_buffer_pool);
  if (result != 0) {
    ert_log_error("Error initializing receive buffer pool");
    goto error_event_cond;
  }

  result = ert_pipe_create(sizeof(ert_comm_transceiver_packet_receive_buffer_metadata_queue_entry), transceiver->config.receive_buffer_length_packets,
      &transceiver->receive_buffer_queue);
  if (result != 0) {
    ert_log_error("Error initializing receive queue");
    goto error_receive_buffer_pool;
  }

  result = ert_buffer_pool_create(sizeof(ert_comm_transceiver_packet_transmit_buffer_metadata), transceiver->config.transmit_buffer_length_packets,
      &transceiver->transmit_buffer_metadata_pool);
  if (result != 0) {
    ert_log_error("Error initializing transmit buffer metadata pool");
    goto error_receive_buffer_queue;
  }

  result = ert_buffer_pool_create(transceiver->max_packet_length, transceiver->config.transmit_buffer_length_packets,
      &transceiver->transmit_buffer_pool);
  if (result != 0) {
    ert_log_error("Error initializing transmit buffer pool");
    goto error_transmit_buffer_metadata_pool;
  }

  result = ert_pipe_create(sizeof(ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry), transceiver->config.transmit_buffer_length_packets,
      &transceiver->transmit_buffer_queue);
  if (result != 0) {
    ert_log_error("Error initializing transmit queue");
    goto error_transmit_buffer_pool;
  }

  result = ert_pipe_create(sizeof(ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry), transceiver->config.transmit_buffer_length_packets,
      &transceiver->transmit_wait_queue);
  if (result != 0) {
    ert_log_error("Error initializing transmit queue");
    goto error_transmit_buffer_queue;
  }

  result = ert_pipe_create(sizeof(ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry), transceiver->config.transmit_buffer_length_packets,
      &transceiver->transmit_result_queue);
  if (result != 0) {
    ert_log_error("Error initializing transmit queue");
    goto error_transmit_wait_queue;
  }

  device->driver->set_callback_context(device, transceiver);
  device->driver->set_receive_callback(device, ert_comm_transceiver_device_receive_callback);
  device->driver->set_transmit_callback(device, ert_comm_transceiver_device_transmit_callback);

  transceiver->running = true;

  result = pthread_create(&transceiver->maintenance_thread, NULL, ert_comm_transceiver_maintenance_routine, transceiver);
  if (result != 0) {
    ert_log_error("Error starting maintenance thread for comm transceiver");
    goto error_threads;
  }

  result = pthread_create(&transceiver->transmit_dispatch_thread, NULL, ert_comm_transceiver_transmit_dispatch_routine, transceiver);
  if (result != 0) {
    ert_log_error("Error starting transmit thread for comm transceiver");
    goto error_threads;
  }

  result = pthread_create(&transceiver->receive_dispatch_thread, NULL, ert_comm_transceiver_receive_dispatch_routine, transceiver);
  if (result != 0) {
    ert_log_error("Error starting receive thread for comm transceiver");
    goto error_threads;
  }

  *transceiver_rcv = transceiver;

  return 0;

  error_threads:
  // TODO: stop above threads
  transceiver->running = false;

  error_transmit_result_queue:
  ert_pipe_destroy(transceiver->transmit_result_queue);

  error_transmit_wait_queue:
  ert_pipe_destroy(transceiver->transmit_wait_queue);

  error_transmit_buffer_queue:
  ert_pipe_destroy(transceiver->transmit_buffer_queue);

  error_transmit_buffer_pool:
  ert_buffer_pool_destroy(transceiver->transmit_buffer_pool);

  error_transmit_buffer_metadata_pool:
  ert_buffer_pool_destroy(transceiver->transmit_buffer_metadata_pool);

  error_receive_buffer_queue:
  ert_pipe_destroy(transceiver->receive_buffer_queue);

  error_receive_buffer_pool:
  ert_buffer_pool_destroy(transceiver->receive_buffer_pool);

  error_event_cond:
  pthread_cond_destroy(&transceiver->event_cond);

  error_event_mutex:
  pthread_mutex_destroy(&transceiver->event_mutex);

  error_status_mutex:
  pthread_mutex_destroy(&transceiver->status_mutex);

  error_device_mutex:
  pthread_mutex_destroy(&transceiver->device_mutex);

  error_transmit_mutex:
  pthread_mutex_destroy(&transceiver->transmit_mutex);

  error_transceiver:
  free(transceiver);

  return result;
}

int ert_comm_transceiver_set_receive_callback(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_receive_callback receive_callback, void *receive_callback_context)
{
  transceiver->config.receive_callback = receive_callback;
  transceiver->config.receive_callback_context = receive_callback_context;

  return 0;
}

int ert_comm_transceiver_set_event_callback(ert_comm_transceiver *transceiver,
    ert_comm_transceiver_event_callback event_callback, void *event_callback_context)
{
  transceiver->config.event_callback = event_callback;
  transceiver->config.event_callback_context = event_callback_context;

  return 0;
}

uint32_t ert_comm_transceiver_get_max_packet_length(ert_comm_transceiver *transceiver)
{
  return transceiver->device->driver->get_max_packet_length(transceiver->device);
}

int ert_comm_transceiver_configure(ert_comm_transceiver *transceiver, void *config)
{
  transceiver->new_config = config;
  return ert_comm_transceiver_signal_event(transceiver);
}

int ert_comm_transceiver_set_receive_active(ert_comm_transceiver *transceiver, bool receive_active)
{
  ert_log_debug("Set receive active: %d", receive_active);
  transceiver->receive_active = receive_active;
  return ert_comm_transceiver_signal_event(transceiver);
}

int ert_comm_transceiver_transmit(ert_comm_transceiver *transceiver,
    uint32_t id, uint32_t length, uint8_t *data, uint32_t flags,
    uint32_t *bytes_transmitted_rcv)
{
  int result;

  if (length > transceiver->max_packet_length) {
    return -EINVAL;
  }

  ert_comm_transceiver_packet_transmit_buffer_metadata *packet_buffer_metadata;
  result = ert_buffer_pool_acquire(transceiver->transmit_buffer_metadata_pool, (void **) &packet_buffer_metadata);
  if (result != 0) {
    return -ENOBUFS;
  }
  memset(packet_buffer_metadata, 0, sizeof(ert_comm_transceiver_packet_transmit_buffer_metadata));

  ert_comm_transceiver_packet_transmit_buffer_metadata_queue_entry packet_buffer_metadata_queue_entry = {0};
  result = ert_buffer_pool_acquire(transceiver->transmit_buffer_pool, (void **) &packet_buffer_metadata_queue_entry.buffer);
  if (result != 0) {
    ert_buffer_pool_release(transceiver->transmit_buffer_metadata_pool, packet_buffer_metadata);
    return -ENOBUFS;
  }

  packet_buffer_metadata_queue_entry.id = id;
  packet_buffer_metadata_queue_entry.length = length;
  packet_buffer_metadata_queue_entry.blocking_enabled = (flags & ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_BLOCK) ? true : false;
  packet_buffer_metadata_queue_entry.set_receive_active = (flags & ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_SET_RECEIVE_ACTIVE) ? true : false;
  packet_buffer_metadata_queue_entry.metadata = packet_buffer_metadata;

  ert_log_debug("Transmitting packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
      packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);

  if (packet_buffer_metadata_queue_entry.blocking_enabled) {
    packet_buffer_metadata->bytes_transmitted = 0;
    packet_buffer_metadata->transmitted_signal = false;
    result = pthread_mutex_init(&packet_buffer_metadata->transmitted_mutex, NULL);
    if (result != 0) {
      ert_log_error("Error initializing transmitted_mutex, result %d", result);
      ert_buffer_pool_release(transceiver->transmit_buffer_pool, packet_buffer_metadata_queue_entry.buffer);
      ert_buffer_pool_release(transceiver->transmit_buffer_metadata_pool, packet_buffer_metadata);
      return -EIO;
    }
    result = pthread_cond_init(&packet_buffer_metadata->transmitted_cond, NULL);
    if (result != 0) {
      ert_log_error("Error initializing transmitted_cond, result %d", result);
      pthread_mutex_destroy(&packet_buffer_metadata->transmitted_mutex);
      ert_buffer_pool_release(transceiver->transmit_buffer_pool, packet_buffer_metadata_queue_entry.buffer);
      ert_buffer_pool_release(transceiver->transmit_buffer_metadata_pool, packet_buffer_metadata);
      return -EIO;
    }
    packet_buffer_metadata->destroy_signal = false;
    packet_buffer_metadata->timeout = false;
    packet_buffer_metadata->result = -EINVAL;

    packet_buffer_metadata_queue_entry.bytes_transmitted = &packet_buffer_metadata->bytes_transmitted;
    packet_buffer_metadata_queue_entry.transmitted_signal = &packet_buffer_metadata->transmitted_signal;
    packet_buffer_metadata_queue_entry.transmitted_mutex = &packet_buffer_metadata->transmitted_mutex;
    packet_buffer_metadata_queue_entry.transmitted_cond = &packet_buffer_metadata->transmitted_cond;
    packet_buffer_metadata_queue_entry.destroy_signal = &packet_buffer_metadata->destroy_signal;
    packet_buffer_metadata_queue_entry.timeout = &packet_buffer_metadata->timeout;
    packet_buffer_metadata_queue_entry.result = &packet_buffer_metadata->result;
  }

  memcpy(packet_buffer_metadata_queue_entry.buffer, data, length);

  ert_log_debug("Pushing packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
      packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);

  struct timespec to;
  if (packet_buffer_metadata_queue_entry.blocking_enabled) {
    result = ert_get_current_timestamp_offset(&to, transceiver->config.transmit_timeout_milliseconds);
    if (result < 0) {
      pthread_cond_destroy(&packet_buffer_metadata->transmitted_cond);
      pthread_mutex_destroy(&packet_buffer_metadata->transmitted_mutex);
      ert_buffer_pool_release(transceiver->transmit_buffer_pool, packet_buffer_metadata_queue_entry.buffer);
      ert_buffer_pool_release(transceiver->transmit_buffer_metadata_pool, packet_buffer_metadata);
      return -EIO;
    }
  }

  ert_pipe_push(transceiver->transmit_buffer_queue, &packet_buffer_metadata_queue_entry, 1);

  if (packet_buffer_metadata_queue_entry.blocking_enabled) {
    ert_log_debug("Locking packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
        packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);

    pthread_mutex_lock(packet_buffer_metadata_queue_entry.transmitted_mutex);
    while (!*packet_buffer_metadata_queue_entry.transmitted_signal) {
      ert_log_debug("Waiting for packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
          packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);
      result = pthread_cond_timedwait(packet_buffer_metadata_queue_entry.transmitted_cond,
          packet_buffer_metadata_queue_entry.transmitted_mutex, &to);

      if (result == ETIMEDOUT) {
        packet_buffer_metadata->timeout = true;
        ert_log_error("Timeout for packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
            packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);
        return -ETIMEDOUT;
      } else if (result != 0) {
        packet_buffer_metadata->timeout = true;
        ert_log_error("Error waiting for packet id=%d, pthread_cond_timedwait failed with result %d",
            packet_buffer_metadata_queue_entry.id, result);
        return -EIO;
      }
    }

    ert_log_debug("Done packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
        packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);

    uint32_t bytes_transmitted = packet_buffer_metadata->bytes_transmitted;
    int transmit_result = packet_buffer_metadata->result;

    pthread_mutex_unlock(packet_buffer_metadata_queue_entry.transmitted_mutex);

    ert_log_debug("Unlocked packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
        packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);

    ert_log_debug("Signal destruction for packet: id=%d, transmitted_signal=%d, bytes_transmitted=%d",
        packet_buffer_metadata_queue_entry.id, packet_buffer_metadata->transmitted_signal, packet_buffer_metadata->bytes_transmitted);

    pthread_mutex_lock(packet_buffer_metadata_queue_entry.transmitted_mutex);
    packet_buffer_metadata->destroy_signal = true;
    pthread_cond_signal(packet_buffer_metadata_queue_entry.transmitted_cond);
    pthread_mutex_unlock(packet_buffer_metadata_queue_entry.transmitted_mutex);

    if (transmit_result < 0) {
      return transmit_result;
    }

    *bytes_transmitted_rcv = bytes_transmitted;
  }

  return 0;
}

int ert_comm_transceiver_set_frequency(ert_comm_transceiver *transceiver, ert_comm_device_config_type config_type, double frequency)
{
  if (transceiver->new_frequency != 0) {
    return -EAGAIN;
  }

  transceiver->new_frequency = frequency;
  transceiver->new_frequency_config_type = config_type;

  return ert_comm_transceiver_signal_event(transceiver);
}

int ert_comm_transceiver_get_status(ert_comm_transceiver *transceiver, ert_comm_transceiver_status *status)
{
  pthread_mutex_lock(&transceiver->status_mutex);
  memcpy(status, &transceiver->status, sizeof(ert_comm_transceiver_status));
  pthread_mutex_unlock(&transceiver->status_mutex);

  return 0;
}

int ert_comm_transceiver_get_device_status(ert_comm_transceiver *transceiver, ert_comm_device_status *status)
{
  pthread_mutex_lock(&transceiver->status_mutex);
  memcpy(status, &transceiver->device_status, sizeof(ert_comm_device_status));
  pthread_mutex_unlock(&transceiver->status_mutex);

  return 0;
}

int ert_comm_transceiver_stop(ert_comm_transceiver *transceiver)
{
  transceiver->running = false;

  transceiver->device->driver->set_receive_callback(transceiver->device, NULL);
  transceiver->device->driver->set_transmit_callback(transceiver->device, NULL);

  ert_comm_transceiver_signal_event(transceiver);
  pthread_join(transceiver->maintenance_thread, NULL);

  ert_pipe_close(transceiver->transmit_buffer_queue);
  ert_pipe_close(transceiver->transmit_wait_queue);
  ert_pipe_close(transceiver->transmit_result_queue);
  pthread_join(transceiver->transmit_dispatch_thread, NULL);

  ert_pipe_close(transceiver->receive_buffer_queue);
  pthread_join(transceiver->receive_dispatch_thread, NULL);

  ert_pipe_destroy(transceiver->transmit_buffer_queue);
  ert_pipe_destroy(transceiver->transmit_wait_queue);
  ert_pipe_destroy(transceiver->transmit_result_queue);
  ert_pipe_destroy(transceiver->receive_buffer_queue);

  ert_buffer_pool_destroy(transceiver->transmit_buffer_pool);
  ert_buffer_pool_destroy(transceiver->transmit_buffer_metadata_pool);
  ert_buffer_pool_destroy(transceiver->receive_buffer_pool);

  pthread_cond_destroy(&transceiver->event_cond);
  pthread_mutex_destroy(&transceiver->event_mutex);

  pthread_mutex_destroy(&transceiver->status_mutex);
  pthread_mutex_destroy(&transceiver->device_mutex);
  pthread_mutex_destroy(&transceiver->transmit_mutex);

  free(transceiver);

  return 0;
}
