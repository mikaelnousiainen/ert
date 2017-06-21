/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_BUFFER_POOL_H
#define __ERT_BUFFER_POOL_H

#include "ert-common.h"
#include <pthread.h>

typedef struct _ert_buffer_pool {
  pthread_mutex_t mutex;
  size_t count;
  size_t element_size;
  bool *used;
  uint8_t *buffer;
} ert_buffer_pool;

int ert_buffer_pool_create(size_t element_size, size_t count, ert_buffer_pool **buffer_pool_rcv);
int ert_buffer_pool_clear(ert_buffer_pool *buffer_pool);
int ert_buffer_pool_acquire(ert_buffer_pool *buffer_pool, void **pointer_rcv);
size_t ert_buffer_pool_get_used_count(ert_buffer_pool *buffer_pool);
int ert_buffer_pool_release(ert_buffer_pool *buffer_pool, void *pointer);
int ert_buffer_pool_destroy(ert_buffer_pool *buffer_pool);

#endif
