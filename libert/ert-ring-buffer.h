/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_RING_BUFFER_H
#define __ERT_RING_BUFFER_H

#include "ert-common.h"

typedef struct _ert_ring_buffer {
  uint32_t buffer_length_bytes;

  uint32_t buffer_used_bytes;
  uint8_t *buffer;
  uint8_t *buffer_head;
  uint8_t *buffer_tail;
} ert_ring_buffer;

int ert_ring_buffer_create(uint32_t buffer_length, ert_ring_buffer **ring_buffer_rcv);
int ert_ring_buffer_destroy(ert_ring_buffer *ring_buffer);
int ert_ring_buffer_clear(ert_ring_buffer *ring_buffer);
uint8_t *ert_ring_buffer_get_address(ert_ring_buffer *ring_buffer);
uint32_t ert_ring_buffer_get_used_bytes(ert_ring_buffer *ring_buffer);
bool ert_ring_buffer_has_space_for(ert_ring_buffer *ring_buffer, uint32_t length);
int ert_ring_buffer_write(ert_ring_buffer *ring_buffer, uint32_t length, uint8_t *data);
int ert_ring_buffer_write_value(ert_ring_buffer *ring_buffer, uint32_t length, uint8_t value);
int ert_ring_buffer_read(ert_ring_buffer *ring_buffer, uint32_t length, uint8_t *data, uint32_t *read_length);

#endif
