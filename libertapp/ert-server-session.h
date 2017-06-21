/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_SERVER_SESSION_H
#define __ERT_SERVER_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#define ERT_SERVER_BUFFER_COUNT 16

typedef int (*http_send_data_chunked_callback)(void *chunked_callback_context, uint32_t *length, uint8_t **data);
typedef int (*http_send_data_chunked_callback_finished)(void *chunked_callback_context);

typedef struct _ert_server_session {
  uint32_t type;

  uint8_t *body_buffer;
  uint32_t body_buffer_length;
  uint32_t body_buffer_offset;

  struct lws *wsi;
  int32_t next_buffer_index_queue_length;
  int32_t next_buffer_index_queue[ERT_SERVER_BUFFER_COUNT];

  uint8_t *current_buffer;
  uint32_t current_buffer_length;
  uint32_t current_buffer_offset;
  bool current_buffer_processing;
  bool current_buffer_done;

  bool chunked_processing;
  bool chunked_done;
  http_send_data_chunked_callback chunked_callback;
  http_send_data_chunked_callback_finished chunked_callback_finished;
  void *chunked_callback_context;
} ert_server_session;

#endif
