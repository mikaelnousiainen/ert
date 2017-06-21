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
#include <string.h>
#include <errno.h>
#include <libwebsockets.h>

#include "ert-server-session-websocket.h"
#include "ert-log.h"

#define WEBSOCKET_SEND_BUFFER_SIZE 4096

int ws_send_message_init(ert_server_session *session,
    struct lws *wsi, uint32_t data_length, unsigned char *data)
{
  session->current_buffer = malloc(data_length);
  if (session->current_buffer == NULL) {
    ert_log_fatal("Error allocating memory for server session data: %s", strerror(errno));
    return -ENOMEM;
  }

  session->current_buffer_processing = true;
  session->current_buffer_done = false;
  session->current_buffer_offset = 0;
  session->current_buffer_length = data_length;
  memcpy(session->current_buffer, data, data_length);

  return 0;
}

int ws_send_message_continue(ert_server_session *session, struct lws *wsi)
{
  int result = 0;
  size_t buffer_length = WEBSOCKET_SEND_BUFFER_SIZE;
  unsigned char buffer[buffer_length + LWS_PRE];

  unsigned char *buffer_start = buffer + LWS_PRE;

  uint32_t data_remaining = session->current_buffer_length - session->current_buffer_offset;

  if (data_remaining == 0) {
    result = lws_write(wsi, buffer_start, 0, LWS_WRITE_CONTINUATION);
    session->current_buffer_done = true;
    goto complete;
  }

  enum lws_write_protocol write_flags = LWS_WRITE_NO_FIN;

  if (session->current_buffer_offset == 0) {
    write_flags |= LWS_WRITE_TEXT;
  } else {
    write_flags |= LWS_WRITE_CONTINUATION;
  }

  size_t bytes_to_write = (data_remaining > buffer_length) ? buffer_length : data_remaining;
  memcpy(buffer_start, session->current_buffer + session->current_buffer_offset, bytes_to_write);
  result = lws_write(wsi, buffer_start, bytes_to_write, write_flags);

  if (result < 0) {
    ert_log_error("lws_write failed with result: %d", result);
    result = -1;
    goto complete_error;
  }
  if (result < bytes_to_write) {
    ert_log_error("lws_write wrote only %d bytes, expected %d bytes", result, bytes_to_write);
    result = -1;
    goto complete_error;
  }

  session->current_buffer_offset += bytes_to_write;

  return (int) bytes_to_write;

  complete_error:

  session->current_buffer_done = false;

  complete:

  session->current_buffer_processing = false;
  session->current_buffer_length = 0;
  session->current_buffer_offset = 0;
  free(session->current_buffer);

  return result;
}
