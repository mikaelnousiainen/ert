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

#include "ert-ring-buffer.h"
#include "ert-log.h"

int ert_ring_buffer_create(uint32_t buffer_length, ert_ring_buffer **ring_buffer_rcv)
{
  ert_ring_buffer *ring_buffer = calloc(1, sizeof(ert_ring_buffer));
  if (ring_buffer == NULL) {
    ert_log_fatal("Error allocating memory for ring buffer struct: %s", strerror(errno));
    return -ENOMEM;
  }

  ring_buffer->buffer = calloc(1, buffer_length);
  if (ring_buffer->buffer == NULL) {
    ert_log_fatal("Error allocating memory for ring buffer data: %s", strerror(errno));
    return -ENOMEM;
  }

  ring_buffer->buffer_used_bytes = 0;
  ring_buffer->buffer_length_bytes = buffer_length;
  ring_buffer->buffer_head = ring_buffer->buffer;
  ring_buffer->buffer_tail = ring_buffer->buffer;

  *ring_buffer_rcv = ring_buffer;

  return 0;
}

int ert_ring_buffer_destroy(ert_ring_buffer *ring_buffer)
{
  free(ring_buffer->buffer);
  free(ring_buffer);

  return 0;
}

int ert_ring_buffer_clear(ert_ring_buffer *ring_buffer)
{
  ring_buffer->buffer_used_bytes = 0;
  ring_buffer->buffer_head = ring_buffer->buffer;
  ring_buffer->buffer_tail = ring_buffer->buffer;

  return 0;
}

uint8_t *ert_ring_buffer_get_address(ert_ring_buffer *ring_buffer)
{
  return ring_buffer->buffer;
}

uint32_t ert_ring_buffer_get_used_bytes(ert_ring_buffer *ring_buffer)
{
  return ring_buffer->buffer_used_bytes;
}

bool ert_ring_buffer_has_space_for(ert_ring_buffer *ring_buffer, uint32_t length)
{
  if (ring_buffer->buffer_used_bytes + length > ring_buffer->buffer_length_bytes) {
    return false;
  }

  return true;
}

int ert_ring_buffer_write(ert_ring_buffer *ring_buffer, uint32_t length, uint8_t *data)
{
  if (!ert_ring_buffer_has_space_for(ring_buffer, length)) {
    return -ENOBUFS;
  }

  uintptr_t buffer_end;
  if ((uintptr_t) ring_buffer->buffer_head <= (uintptr_t) ring_buffer->buffer_tail) {
    buffer_end = (uintptr_t) ring_buffer->buffer + (uintptr_t) ring_buffer->buffer_length_bytes;
  } else {
    buffer_end = (uintptr_t) ring_buffer->buffer_head;
  }

  uintptr_t buffer_end_remaining_bytes = buffer_end - (uintptr_t) ring_buffer->buffer_tail;

  if (length <= buffer_end_remaining_bytes) {
    memcpy(ring_buffer->buffer_tail, data, length);
    ring_buffer->buffer_tail += length;
  } else {
    memcpy(ring_buffer->buffer_tail, data, buffer_end_remaining_bytes);
    ring_buffer->buffer_tail = ring_buffer->buffer;

    uintptr_t payload_remaining_bytes = length - buffer_end_remaining_bytes;
    memcpy(ring_buffer->buffer_tail, data + buffer_end_remaining_bytes, payload_remaining_bytes);
    ring_buffer->buffer_tail += payload_remaining_bytes;
  }

  ring_buffer->buffer_used_bytes += length;

  return 0;
}

int ert_ring_buffer_write_value(ert_ring_buffer *ring_buffer, uint32_t length, uint8_t value)
{
  if (!ert_ring_buffer_has_space_for(ring_buffer, length)) {
    return -ENOBUFS;
  }

  uintptr_t buffer_end;
  if ((uintptr_t) ring_buffer->buffer_head <= (uintptr_t) ring_buffer->buffer_tail) {
    buffer_end = (uintptr_t) ring_buffer->buffer + (uintptr_t) ring_buffer->buffer_length_bytes;
  } else {
    buffer_end = (uintptr_t) ring_buffer->buffer_head;
  }

  uintptr_t buffer_end_remaining_bytes = buffer_end - (uintptr_t) ring_buffer->buffer_tail;

  if (length <= buffer_end_remaining_bytes) {
    memset(ring_buffer->buffer_tail, value, length);
    ring_buffer->buffer_tail += length;
  } else {
    memset(ring_buffer->buffer_tail, value, buffer_end_remaining_bytes);
    ring_buffer->buffer_tail = ring_buffer->buffer;

    uintptr_t payload_remaining_bytes = length - buffer_end_remaining_bytes;
    memset(ring_buffer->buffer_tail, value, payload_remaining_bytes);
    ring_buffer->buffer_tail += payload_remaining_bytes;
  }

  ring_buffer->buffer_used_bytes += length;

  return 0;
}

int ert_ring_buffer_read(ert_ring_buffer *ring_buffer, uint32_t length, uint8_t *data, uint32_t *read_length)
{
  uint32_t bytes_to_read = (ring_buffer->buffer_used_bytes < length)
                           ? ring_buffer->buffer_used_bytes : length;

  if (bytes_to_read == 0) {
    *read_length = bytes_to_read;
    return 0;
  }

  uintptr_t buffer_end;
  if ((uintptr_t) ring_buffer->buffer_head < (uintptr_t) ring_buffer->buffer_tail) {
    buffer_end = (uintptr_t) ring_buffer->buffer_tail;
  } else {
    buffer_end = (uintptr_t) ring_buffer->buffer + (uintptr_t) ring_buffer->buffer_length_bytes;
  }

  uintptr_t buffer_end_remaining_bytes = buffer_end - (uintptr_t) ring_buffer->buffer_head;

  if (bytes_to_read <= buffer_end_remaining_bytes) {
    memcpy(data, ring_buffer->buffer_head, bytes_to_read);
    ring_buffer->buffer_head += bytes_to_read;
  } else {
    memcpy(data, ring_buffer->buffer_head, buffer_end_remaining_bytes);
    ring_buffer->buffer_head = ring_buffer->buffer;

    uintptr_t data_remaining_bytes = bytes_to_read - buffer_end_remaining_bytes;
    memcpy(data + buffer_end_remaining_bytes, ring_buffer->buffer_head, data_remaining_bytes);
    ring_buffer->buffer_head += data_remaining_bytes;
  }

  ring_buffer->buffer_used_bytes -= bytes_to_read;

  *read_length = bytes_to_read;

  return 0;
}
