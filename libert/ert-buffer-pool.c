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

#include "ert-buffer-pool.h"
#include "ert-log.h"

int ert_buffer_pool_create(size_t element_size, size_t count, ert_buffer_pool **buffer_pool_rcv)
{
  ert_buffer_pool *buffer_pool = calloc(1, sizeof(ert_buffer_pool));
  if (buffer_pool == NULL) {
    ert_log_fatal("Error allocating memory for buffer pool struct: %s", strerror(errno));
    return -ENOMEM;
  }

  buffer_pool->buffer = calloc(count, element_size);
  if (buffer_pool->buffer == NULL) {
    free(buffer_pool);
    ert_log_fatal("Error allocating memory for buffer pool elements: %s", strerror(errno));
    return -ENOMEM;
  }

  buffer_pool->used = calloc(count, sizeof(bool));
  if (buffer_pool->used == NULL) {
    free(buffer_pool->buffer);
    free(buffer_pool);
    ert_log_fatal("Error allocating memory for buffer pool state: %s", strerror(errno));
    return -ENOMEM;
  }

  int result = pthread_mutex_init(&buffer_pool->mutex, NULL);
  if (result != 0) {
    free(buffer_pool->used);
    free(buffer_pool->buffer);
    free(buffer_pool);
    ert_log_error("Error initializing buffer pool mutex: %s", strerror(errno));
    return -EIO;
  }

  buffer_pool->count = count;
  buffer_pool->element_size = element_size;

  *buffer_pool_rcv = buffer_pool;

  return 0;
}

int ert_buffer_pool_clear(ert_buffer_pool *buffer_pool)
{
  pthread_mutex_lock(&buffer_pool->mutex);

  for (size_t index = 0; index < buffer_pool->count; index++) {
    buffer_pool->used[index] = false;
  }

  pthread_mutex_unlock(&buffer_pool->mutex);

  return 0;
}

int ert_buffer_pool_acquire(ert_buffer_pool *buffer_pool, void **pointer_rcv)
{
  size_t index;
  bool found = false;

  pthread_mutex_lock(&buffer_pool->mutex);

  for (index = 0; index < buffer_pool->count; index++) {
    if (buffer_pool->used[index] == false) {
      found = true;
      break;
    }
  }

  if (!found) {
    pthread_mutex_unlock(&buffer_pool->mutex);
    return -ENOBUFS;
  }

  buffer_pool->used[index] = true;

  pthread_mutex_unlock(&buffer_pool->mutex);

  *pointer_rcv = buffer_pool->buffer + (index * buffer_pool->element_size);

  return 0;
}

size_t ert_buffer_pool_get_used_count(ert_buffer_pool *buffer_pool)
{
  size_t count = 0;

  pthread_mutex_lock(&buffer_pool->mutex);

  for (size_t index = 0; index < buffer_pool->count; index++) {
    if (buffer_pool->used[index]) {
      count++;
    }
  }

  pthread_mutex_unlock(&buffer_pool->mutex);

  return count;
}

int ert_buffer_pool_release(ert_buffer_pool *buffer_pool, void *pointer)
{
  if (buffer_pool == NULL) {
    ert_log_error("Null pointer to buffer pool");
    return -EINVAL;
  }
  if (pointer == NULL) {
    ert_log_error("Null pointer to buffer pool entry");
    return -EINVAL;
  }
  uintptr_t offset = ((uintptr_t) pointer - (uintptr_t) buffer_pool->buffer);

  if (offset % buffer_pool->element_size != 0) {
    ert_log_error("Invalid pointer to buffer pool entry: not aligned to element size: %p", pointer);
    return -EINVAL;
  }

  size_t index = offset / buffer_pool->element_size;
  if (index >= buffer_pool->count) {
    ert_log_error("Invalid pointer to buffer pool entry: index %d is higher than buffer count %d: %p",
        index, buffer_pool->count, pointer);
    return -EINVAL;
  }

  pthread_mutex_lock(&buffer_pool->mutex);

  if (!buffer_pool->used[index]) {
    pthread_mutex_unlock(&buffer_pool->mutex);
    ert_log_error("Cannot release pointer to buffer pool entry: buffer not used for index %d: %p", index, pointer);
    return -ENOBUFS;
  }

  buffer_pool->used[index] = false;

  pthread_mutex_unlock(&buffer_pool->mutex);

  return 0;
}

int ert_buffer_pool_destroy(ert_buffer_pool *buffer_pool)
{
  pthread_mutex_destroy(&buffer_pool->mutex);
  free(buffer_pool->used);
  free(buffer_pool->buffer);
  free(buffer_pool);

  return 0;
}
