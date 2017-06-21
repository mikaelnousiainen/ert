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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <libwebsockets.h>

#include "ert-server-session-http.h"
#include "ert-log.h"

#define HTTP_SEND_BUFFER_SIZE 4096
#define HTTP_SEND_CHUNK_BUFFER_SIZE 16384

typedef struct _file_mime_type {
  const char *extension;
  const char *mime_type;
} file_mime_type;

file_mime_type mime_types[] = {
    {
        .extension = "webp",
        .mime_type = "image/webp",
    },
    {
        .extension = "svg",
        .mime_type = "image/svg+xml",
    },
    {
        .extension = "ttf",
        .mime_type = "application/font-sfnt",
    },
    {
        .extension = "eot",
        .mime_type = "application/vnd.ms-fontobject",
    },
    {
        .extension = "eotf",
        .mime_type = "application/vnd.ms-fontobject",
    },
    {
        .extension = "otf",
        .mime_type = "application/font-sfnt",
    },
    {
        .extension = "woff",
        .mime_type = "application/font-woff",
    },
    {
        .extension = "woff2",
        .mime_type = "application/font-woff2",
    },
    {
        .extension = NULL,
        .mime_type = NULL,
    },
};

static const char *find_mime_type(char *filename)
{
  char *ext = strrchr(filename, '.');
  if (ext == NULL) {
    return NULL;
  }

  ext += 1;

  size_t index = 0;
  file_mime_type *fmt;
  while (true) {
    fmt = &mime_types[index];

    if (fmt->extension == NULL || fmt->mime_type == NULL) {
      break;
    }

    if (strcmp(ext, fmt->extension) == 0) {
      return fmt->mime_type;
    }

    index++;
  }

  return NULL;
}

int http_send_file(struct lws *wsi, char *root_path, char *requested_uri)
{
  int result;
  char root_realpath[PATH_MAX];
  char resource_path[PATH_MAX];
  char resource_realpath[PATH_MAX];

  if (realpath(root_path, root_realpath) == NULL) {
    ert_log_error("Invalid root path for serving file: %s (%s)", root_path, strerror(errno));
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  snprintf(resource_path, PATH_MAX, "%s%s", root_realpath, requested_uri);
  ert_log_debug("HTTP request resource path: %s", resource_path);

  if (realpath(resource_path, resource_realpath) == NULL) {
    if (errno == ENOENT || errno == ENOTDIR || errno == EACCES) {
      ert_log_warn("File not found for serving HTTP file: %s (%s)", resource_path, strerror(errno));
      return lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
    }

    ert_log_error("Invalid resource path for serving file: %s (%s)", resource_path, strerror(errno));
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  if (strncmp(root_realpath, resource_realpath, strlen(root_realpath)) != 0) {
    ert_log_error("Invalid URI for file access: %s", requested_uri);
    return lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
  }

  struct stat st = {0};
  if (stat(resource_realpath, &st) != 0) {
    return lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
  }
  if (!S_ISREG(st.st_mode)) {
    return lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
  }

  const char *mime = lws_get_mimetype(requested_uri, NULL);
  if (mime == NULL) {
    mime = find_mime_type(requested_uri);
  }
  if (mime == NULL) {
    mime = "application/octet-stream";
  }

  ert_log_info("Sending HTTP file '%s' as MIME type '%s'", resource_realpath, mime);

  result = lws_serve_http_file(wsi, resource_realpath, mime, NULL, 0);
  if (result < 0) {
    return -1;
  }
  if (result > 0) {
    result = lws_http_transaction_completed(wsi);
    if (result != 0) {
      return -1;
    }
  }

  return 0;
}

int http_send_data_init(ert_server_session *session,
    struct lws *wsi, char *content_type, uint32_t data_length, unsigned char *data)
{
  int result = 0;
  size_t buffer_length = HTTP_SEND_BUFFER_SIZE;
  unsigned char buffer[buffer_length + LWS_PRE];

  unsigned char *buffer_start = buffer + LWS_PRE;
  unsigned char *buffer_end = buffer + buffer_length;
  unsigned char *buffer_current = buffer_start;

  if (session->current_buffer_processing) {
    return -1;
  }

  session->current_buffer = malloc(data_length);
  if (session->current_buffer == NULL) {
    ert_log_fatal("Error allocating memory for server session data: %s", strerror(errno));
    return -ENOMEM;
  }

  session->current_buffer_done = false;
  session->current_buffer_offset = 0;
  session->current_buffer_length = data_length;
  memcpy(session->current_buffer, data, data_length);

  if (lws_add_http_header_status(wsi, 200, &buffer_current, buffer_end))
    return 1;
  if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
      content_type, strlen(content_type), &buffer_current, buffer_end))
    return 1;
  if (lws_add_http_header_content_length(wsi, data_length, &buffer_current, buffer_end))
    return 1;
  if (lws_finalize_http_header(wsi, &buffer_current, buffer_end))
    return 1;

  size_t bytes_to_write = (size_t) (buffer_current - buffer_start);
  result = lws_write(wsi, buffer_start, bytes_to_write, LWS_WRITE_HTTP_HEADERS);
  if (result < 0) {
    ert_log_error("lws_write failed with result: %d", result);
    result = -1;
    goto complete_error;
  }

  result = lws_callback_on_writable(wsi);
  if (result < 0) {
    ert_log_error("lws_callback_on_writable failed with result: %d", result);
    result = -1;
    goto complete_error;
  }

  session->current_buffer_processing = true;

  return 0;

  complete_error:

  session->current_buffer_processing = false;
  session->current_buffer_done = false;
  session->current_buffer_length = 0;
  session->current_buffer_offset = 0;
  free(session->current_buffer);

  int complete_result = lws_http_transaction_completed(wsi);
  if (complete_result != 0) {
    return -1;
  }

  return result;
}

int http_send_data_continue(ert_server_session *session, struct lws *wsi)
{
  int result = 0;
  size_t buffer_length = HTTP_SEND_BUFFER_SIZE;
  unsigned char buffer[buffer_length + LWS_PRE];

  unsigned char *buffer_start = buffer + LWS_PRE;

  if (!session->current_buffer_processing) {
    return -1;
  }

  uint32_t data_remaining = session->current_buffer_length - session->current_buffer_offset;

  if (data_remaining == 0) {
    session->current_buffer_done = true;
    goto complete;
  }

  size_t bytes_to_write = (data_remaining > buffer_length) ? buffer_length : data_remaining;
  memcpy(buffer_start, session->current_buffer + session->current_buffer_offset, bytes_to_write);
  result = lws_write(wsi, buffer_start, bytes_to_write, LWS_WRITE_HTTP);

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

  result = lws_callback_on_writable(wsi);
  if (result < 0) {
    ert_log_error("lws_callback_on_writable failed with result: %d", result);
    result = -1;
    goto complete_error;
  }

  return (int) bytes_to_write;

  complete_error:

  session->current_buffer_done = false;

  complete:

  session->current_buffer_processing = false;
  session->current_buffer_length = 0;
  session->current_buffer_offset = 0;
  free(session->current_buffer);

  int complete_result = lws_http_transaction_completed(wsi);
  if (complete_result != 0) {
    return -1;
  }

  return result;
}

int http_send_data_chunked_init(ert_server_session *session,
    struct lws *wsi, char *content_type,
    http_send_data_chunked_callback chunked_callback,
    http_send_data_chunked_callback_finished chunked_callback_finished,
    void *chunked_callback_context)
{
  int result = 0;
  size_t buffer_length = HTTP_SEND_BUFFER_SIZE;
  unsigned char buffer[buffer_length + LWS_PRE];

  unsigned char *buffer_start = buffer + LWS_PRE;
  unsigned char *buffer_end = buffer + buffer_length;
  unsigned char *buffer_current = buffer_start;

  unsigned char transfer_encoding[] = "chunked";

  if (session->chunked_processing) {
    return -1;
  }

  session->chunked_done = false;

  if (lws_add_http_header_status(wsi, 200, &buffer_current, buffer_end))
    return 1;
  if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
      content_type, strlen(content_type), &buffer_current, buffer_end))
    return 1;
  if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_TRANSFER_ENCODING,
      transfer_encoding, strlen(transfer_encoding), &buffer_current, buffer_end))
    return 1;
  if (lws_finalize_http_header(wsi, &buffer_current, buffer_end))
    return 1;

  session->chunked_callback = chunked_callback;
  session->chunked_callback_finished = chunked_callback_finished;
  session->chunked_callback_context = chunked_callback_context;
  session->chunked_processing = true;

  size_t bytes_to_write = (size_t) (buffer_current - buffer_start);
  result = lws_write(wsi, buffer_start, bytes_to_write, LWS_WRITE_HTTP_HEADERS);
  if (result < 0) {
    ert_log_error("lws_write failed with result: %d", result);
    result = -1;
    goto complete_error;
  }

  result = lws_callback_on_writable(wsi);
  if (result < 0) {
    ert_log_error("lws_callback_on_writable failed with result: %d", result);
    result = -1;
    goto complete_error;
  }

  return 0;

  complete_error:

  session->chunked_callback_finished(session->chunked_callback_context);

  session->chunked_processing = false;
  session->chunked_done = false;
  session->chunked_callback = NULL;
  session->chunked_callback_finished = NULL;
  session->chunked_callback_context = NULL;

  int complete_result = lws_http_transaction_completed(wsi);
  if (complete_result != 0) {
    return -1;
  }

  return result;
}

int http_send_data_chunked_continue(ert_server_session *session, struct lws *wsi)
{
  int result = 0;
  size_t buffer_length = HTTP_SEND_CHUNK_BUFFER_SIZE;
  unsigned char buffer[buffer_length + LWS_PRE + 32];

  unsigned char *buffer_start = buffer + LWS_PRE;

  if (!session->chunked_processing) {
    return -1;
  }

  if (session->chunked_done) {
    goto complete;
  }

  uint32_t data_read_bytes = (uint32_t) buffer_length;
  uint8_t *data_buffer;
  result = session->chunked_callback(session->chunked_callback_context, &data_read_bytes, &data_buffer);
  if (result < 0) {
    goto complete_error;
  }

  const char *delimiter = "\r\n";
  size_t delimiter_bytes = strlen(delimiter);

  int chunk_header_length = snprintf(buffer_start, buffer_length, "%X%s", data_read_bytes, delimiter);
  memcpy(buffer_start + chunk_header_length, data_buffer, data_read_bytes);
  strcpy(buffer_start + chunk_header_length + data_read_bytes, delimiter);

  uint32_t bytes_to_write = chunk_header_length + data_read_bytes + (uint32_t) delimiter_bytes;

  result = lws_write(wsi, buffer_start, bytes_to_write, LWS_WRITE_HTTP);

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

  result = lws_callback_on_writable(wsi);
  if (result < 0) {
    ert_log_error("lws_callback_on_writable failed with result: %d", result);
    result = -1;
    goto complete_error;
  }

  if (data_read_bytes == 0) {
    session->chunked_done = true;
  }

  return (int) bytes_to_write;

  complete_error:

  session->chunked_done = false;

  complete:

  session->chunked_callback_finished(session->chunked_callback_context);

  session->chunked_processing = false;
  session->chunked_callback = NULL;
  session->chunked_callback_finished = NULL;
  session->chunked_callback_context = NULL;

  int complete_result = lws_http_transaction_completed(wsi);
  if (complete_result != 0) {
    return -1;
  }

  return result;
}

int http_receive_body_data_init(ert_server_session *session, struct lws *wsi, uint32_t body_buffer_length, uint32_t session_type)
{
  session->body_buffer = calloc(1, body_buffer_length);
  if (session->body_buffer == NULL) {
    ert_log_fatal("Error allocating memory for HTTP body buffer: %s", strerror(errno));
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  session->type = session_type;
  session->body_buffer_length = body_buffer_length;
  session->body_buffer_offset = 0;

  return 0;
}

int http_receive_body_data(ert_server_session *session, struct lws *wsi, size_t length, void *data)
{
  if (session->body_buffer == NULL) {
    ert_log_error("No HTTP body buffer allocated for session");
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }
  if (session->body_buffer_offset + length >= session->body_buffer_length) {
    ert_log_error("HTTP body larger than maximum of %d bytes", session->body_buffer_length);
    return lws_return_http_status(wsi, HTTP_STATUS_REQ_ENTITY_TOO_LARGE, NULL);
  }

  memcpy(session->body_buffer + session->body_buffer_offset, data, length);
  session->body_buffer_offset += length;

  return 0;
}

void http_receive_body_data_complete(ert_server_session *session)
{
  if (session->body_buffer == NULL) {
    ert_log_error("No HTTP body buffer allocated for session");
    return;
  }

  session->body_buffer[session->body_buffer_offset] = '\0';
}

void http_receive_body_data_destroy(ert_server_session *session)
{
  if (session->body_buffer == NULL) {
    ert_log_error("No HTTP body buffer allocated for session");
    return;
  }

  free(session->body_buffer);

  session->body_buffer = NULL;
  session->body_buffer_length = 0;
  session->body_buffer_offset = 0;
  session->type = 0;
}
