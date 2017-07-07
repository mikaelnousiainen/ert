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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <inttypes.h>
#include <libwebsockets.h>

#include "ert-server.h"
#include "ert-server-session-http.h"
#include "ert-server-session-websocket.h"
#include "ert-data-logger-history-reader.h"
#include "ert-log.h"
#include "ert-mapper-json.h"
#include "ert-comm-protocol-json.h"
#include "ertapp-common.h"
#include "ert-server-status.h"

#define ERT_SERVER_SESSIONS_COUNT 64

#define ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_NODE_TELEMETRY 0
#define ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_GATEWAY_TELEMETRY 1
#define ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_NODE_IMAGE 2

#define ERT_SERVER_DATA_LOGGER_HISTORY_ENTRY_BUFFER_LENGTH 16384

#define ERT_SERVER_SESSION_TYPE_UPDATE_CONFIG 1

static char *content_type_application_json = "application/json";

const char *path_data_logger_latest_entry_node = "/api/data-logger/latest-entry/node";
const char *path_data_logger_latest_entry_gateway = "/api/data-logger/latest-entry/gateway";
const char *path_data_logger_history_node = "/api/data-logger/history/node";
const char *path_data_logger_history_gateway = "/api/data-logger/history/gateway";
const char *path_image_history = "/api/image-history";
const char *path_image_api_prefix = "/api/image/";
const char *path_status = "/api/status";
const char *path_config = "/api/config";
const char *path_comm_protocol_active_streams = "/api/comm-protocol/active-streams";

typedef struct _ert_server_buffer {
  uint8_t *buffer;
  uint32_t buffer_length;
  uint32_t buffer_data_length;
} ert_server_buffer;

struct _ert_server {
  volatile bool running;
  struct lws_context *lws_context;
  pthread_t handler_thread;

  pthread_mutex_t buffers_mutex;
  ert_server_buffer buffers[ERT_SERVER_BUFFER_COUNT];

  pthread_mutex_t sessions_mutex;
  ert_server_session *sessions[ERT_SERVER_SESSIONS_COUNT];

  ert_server_config *config;

  ert_server_status server_status;
};

static ssize_t index_of(char *string, char c)
{
  for (size_t i = 0; string[i] != 0; i++) {
    if (string[i] == c) {
      return i;
    }
  }

  return -1;
}

static bool get_url_parameter_value(struct lws *wsi, char *name, size_t buffer_length, char *buffer)
{
  int param_index = 0;
  while (lws_hdr_copy_fragment(wsi, buffer, buffer_length, WSI_TOKEN_HTTP_URI_ARGS, param_index++) > 0) {
    ssize_t delimiter_index = index_of(buffer, '=');
    if (delimiter_index < 0) {
      continue;
    }

    buffer[delimiter_index] = '\0';
    if (strcmp(buffer, name) == 0) {
      char *value = buffer + delimiter_index + 1;
      memmove(buffer, value, strlen(value));
      return true;
    }
  }

  return false;
}

static int serve_server_buffer(struct lws *wsi, ert_server_session *session,
    char *content_type, int32_t server_buffer_index)
{
  struct lws_context *context = lws_get_context(wsi);
  ert_server *server = lws_context_user(context);
  int result;

  pthread_mutex_lock(&server->buffers_mutex);

  ert_server_buffer *server_buffer = &server->buffers[server_buffer_index];

  result = http_send_data_init(session, wsi, content_type,
      server_buffer->buffer_data_length, server_buffer->buffer);

  pthread_mutex_unlock(&server->buffers_mutex);

  return result;
}

static int serve_data_logger_history(struct lws *wsi, ert_server_session *session,
  char *filename_template)
{
  struct lws_context *context = lws_get_context(wsi);
  ert_server *server = lws_context_user(context);
  int result;

  ert_data_logger_history_reader *reader;

  size_t param_buffer_length = 128;
  char param_buffer[param_buffer_length];
  int history_entry_count = 10;
  if (get_url_parameter_value(wsi, "count", param_buffer_length, param_buffer)) {
    int value = atoi(param_buffer);
    if (value >= 0) {
      history_entry_count = value;
    }
  }

  result = ert_data_logger_history_reader_create_from_filename_template(
      server->config->data_logger_path, filename_template,
      ERT_SERVER_DATA_LOGGER_HISTORY_ENTRY_BUFFER_LENGTH, history_entry_count, &reader);
  if (result < 0) {
    if (result == -2) {
      return lws_return_http_status(wsi, HTTP_STATUS_NO_CONTENT, NULL);
    }

    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  result = http_send_data_chunked_init(session, wsi, content_type_application_json,
      ert_data_logger_history_reader_callback,
      ert_data_logger_history_reader_callback_finished,
      reader);
  if (result < 0) {
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  return result;
}

static int serve_image_history(struct lws *wsi, ert_server_session *session)
{
  struct lws_context *context = lws_get_context(wsi);
  ert_server *server = lws_context_user(context);
  int result;

  size_t param_buffer_length = 128;
  char param_buffer[param_buffer_length];
  int history_entry_count = 10;

  if (get_url_parameter_value(wsi, "count", param_buffer_length, param_buffer)) {
    int value = atoi(param_buffer);
    if (value >= 0) {
      history_entry_count = value;
    }
  }

  uint32_t buffer_length;
  uint8_t *buffer;
  result = ert_data_logger_file_history_list(server->config->image_path, NULL, history_entry_count, &buffer_length, &buffer);
  if (result < 0) {
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  result = http_send_data_init(session, wsi, content_type_application_json, buffer_length, buffer);
  if (result < 0) {
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  free(buffer);

  return result;
}

static int serve_status(struct lws *wsi, ert_server *server, ert_server_session *session)
{
  uint32_t data_length;
  uint8_t *data;

  int result = ert_server_status_json_serialize(&server->server_status, &data_length, &data);
  if (result < 0) {
    ert_log_error("Error serializing server status to JSON string");
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  result = http_send_data_init(session, wsi, content_type_application_json, data_length, data);

  free(data);

  return result;
}

static int serve_config(struct lws *wsi, ert_server *server, ert_server_session *session)
{
  ert_mapper_entry *root_entry = server->config->app_config_root_entry;
    if (root_entry == NULL) {
    ert_log_error("No application config root entry provided");
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  char *data;

  int result = ert_mapper_serialize_to_json(root_entry, &data);
  if (result < 0) {
    ert_log_error("Error serializing config to JSON string");
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  uint32_t length = (uint32_t) strlen(data);

  result = http_send_data_init(session, wsi, content_type_application_json, (uint32_t) length, data);

  free(data);

  return result;
}

static int serve_update_config(struct lws *wsi, ert_server *server, ert_server_session *session)
{
  ert_mapper_entry *root_entry = server->config->app_config_root_entry;
  if (root_entry == NULL) {
    ert_log_error("No application config root entry provided");
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  int result = ert_mapper_update_from_json(root_entry,
      (char *) session->body_buffer, server->config->app_config_update_context);

  if (result < 0) {
    ert_log_error("Invalid config update request: %s", session->body_buffer);
    return lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
  }

  ert_log_info("Updated config with: %s", session->body_buffer);

  return serve_config(wsi, server, session);
}

static int serve_comm_protocol_active_streams(struct lws *wsi, ert_server *server, ert_server_session *session)
{
  ert_comm_protocol *comm_protocol = server->config->app_comm_protocol;
  if (comm_protocol == NULL) {
    ert_log_error("No application comm protocol reference provided");
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  size_t stream_info_count;
  ert_comm_protocol_stream_info *stream_info;
  int result = ert_comm_protocol_get_active_streams(comm_protocol, &stream_info_count, &stream_info);
  if (result < 0) {
    ert_log_error("Error getting active comm protocol streams, result %d", result);
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  uint32_t length;
  uint8_t *data;
  result = ert_comm_protocol_stream_info_json_serialize(stream_info_count, stream_info, &length, &data);
  if (result < 0) {
    free(stream_info);
    ert_log_error("Error serializing comm protocol stream info to JSON, result %d", result);
    return lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
  }

  result = http_send_data_init(session, wsi, content_type_application_json, length, data);

  free(data);
  free(stream_info);

  return result;
}

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
  struct lws_context *context = lws_get_context(wsi);
  ert_server *server = lws_context_user(context);
  ert_server_session *session = user;
  int result;

  size_t client_address_buffer_length = 128;
  char client_address[client_address_buffer_length];

  switch (reason) {
    case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);
      ert_log_debug("HTTP connection established: client_address=%s", client_address);
      break;
    case LWS_CALLBACK_HTTP: {
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);

      char *requested_uri = (char *) in;
      ert_log_info("HTTP request: client_address=%s uri=%s", client_address, requested_uri);

      if (strcmp(requested_uri, path_data_logger_latest_entry_node) == 0) {
        return serve_server_buffer(wsi, session, content_type_application_json,
            ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_NODE_TELEMETRY);
      } else if (strcmp(requested_uri, path_data_logger_latest_entry_gateway) == 0) {
        return serve_server_buffer(wsi, session, content_type_application_json,
            ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_GATEWAY_TELEMETRY);
      } else if (strcmp(requested_uri, path_data_logger_history_node) == 0) {
        return serve_data_logger_history(wsi, session, server->config->data_logger_node_filename_template);
      } else if (strcmp(requested_uri, path_data_logger_history_gateway) == 0) {
        return serve_data_logger_history(wsi, session, server->config->data_logger_gateway_filename_template);
      } else if (strcmp(requested_uri, path_status) == 0) {
        return serve_status(wsi, server, session);
      } else if (strcmp(requested_uri, path_config) == 0) {
        if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
          return http_receive_body_data_init(session, wsi,
              server->config->server_buffer_length, ERT_SERVER_SESSION_TYPE_UPDATE_CONFIG);
        }

        return serve_config(wsi, server, session);
      } else if (strcmp(requested_uri, path_comm_protocol_active_streams) == 0) {
        return serve_comm_protocol_active_streams(wsi, server, session);
      } else if (strncmp(requested_uri, path_image_api_prefix, strlen(path_image_api_prefix)) == 0) {
        char filename[PATH_MAX] = "";

        strncat(filename, requested_uri + strlen(path_image_api_prefix) - 1, PATH_MAX);

        ert_log_info("Requested image file: %s", filename);

        return http_send_file(wsi, server->config->image_path, filename);
      } else if (strcmp(requested_uri, path_image_history) == 0) {
        return serve_image_history(wsi, session);
      }

      ert_log_info("Requested static file: %s", requested_uri);

      char filename[PATH_MAX] = "";
      size_t requested_uri_length = strlen(requested_uri);

      strncat(filename, requested_uri, PATH_MAX);

      if (requested_uri[requested_uri_length - 1] == '/') {
        strncat(filename, "index.html", PATH_MAX - strlen(filename));
      }

      return http_send_file(wsi, server->config->static_path, filename);
    }
    case LWS_CALLBACK_HTTP_BODY:
      return http_receive_body_data(session, wsi, len, in);
    case LWS_CALLBACK_HTTP_BODY_COMPLETION:
      http_receive_body_data_complete(session);

      if (session->type == ERT_SERVER_SESSION_TYPE_UPDATE_CONFIG) {
        result = serve_update_config(wsi, server, session);
      } else {
        result = lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
      }

      http_receive_body_data_destroy(session);

      return result;
    case LWS_CALLBACK_HTTP_WRITEABLE:
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);

      if (session->current_buffer_processing) {
        result = http_send_data_continue(session, wsi);
        if (result < 0) {
          return result;
        }
        if (result == 0) {
          ert_log_debug("HTTP data send finished: client_address=%s", client_address);
        }
        break;
      }

      if (session->chunked_processing) {
        result = http_send_data_chunked_continue(session, wsi);
        if (result < 0) {
          return result;
        }
        if (result == 0) {
          ert_log_debug("HTTP chunked data send finished: client_address=%s", client_address);
        }
        break;
      }

      break;
    case LWS_CALLBACK_HTTP_FILE_COMPLETION:
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);
      ert_log_info("HTTP file send completed: client_address=%s", client_address);
      result = lws_http_transaction_completed(wsi);
      if (result != 0) {
        return -1;
      }
      break;
    case LWS_CALLBACK_CLOSED_HTTP:
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);
      ert_log_debug("HTTP session closed: client_address=%s", client_address);
      // TODO: free session data if not freed
      return -1;
    case LWS_CALLBACK_GET_THREAD_ID:
      break;
    default:
      break;
  }

  return 0;
}

static int ert_server_session_add(ert_server *server, ert_server_session *session)
{
  pthread_mutex_lock(&server->sessions_mutex);

  for (size_t i = 0; i < ERT_SERVER_SESSIONS_COUNT; i++) {
    if (server->sessions[i] == NULL) {
      server->sessions[i] = session;
      pthread_mutex_unlock(&server->sessions_mutex);
      return 0;
    }
  }

  pthread_mutex_unlock(&server->sessions_mutex);

  return -1;
}

static int ert_server_session_remove(ert_server *server, ert_server_session *session)
{
  pthread_mutex_lock(&server->sessions_mutex);

  for (size_t i = 0; i < ERT_SERVER_SESSIONS_COUNT; i++) {
    if (server->sessions[i] == session) {
      server->sessions[i] = NULL;
    }
  }

  pthread_mutex_unlock(&server->sessions_mutex);

  return 0;
}

// Execute server->buffers_mutex locked
static int ert_server_next_buffer_index_push(ert_server_session *session, int32_t index)
{
  if (session->next_buffer_index_queue_length >= ERT_SERVER_BUFFER_COUNT) {
    return -1;
  }

  for (int32_t i = 0; i < session->next_buffer_index_queue_length; i++) {
    if (session->next_buffer_index_queue[i] == index) {
      return 0;
    }
  }
  session->next_buffer_index_queue[session->next_buffer_index_queue_length] = index;
  session->next_buffer_index_queue_length++;

  return 0;
}

// Execute server->buffers_mutex locked
static int ert_server_next_buffer_index_pop(ert_server_session *session, int32_t *index)
{
  if (session->next_buffer_index_queue_length == 0) {
    *index = -1;
    return 0;
  }

  *index = session->next_buffer_index_queue[0];

  for (int32_t i = 1; i < session->next_buffer_index_queue_length; i++) {
    session->next_buffer_index_queue[i - 1] = session->next_buffer_index_queue[i];
  }

  session->next_buffer_index_queue_length--;

  return 0;
}

static int serve_websocket_data(struct lws *wsi, ert_server_session *session)
{
  struct lws_context *context = lws_get_context(wsi);
  ert_server *server = lws_context_user(context);
  int result;

  bool continue_writing = false;

  pthread_mutex_lock(&server->buffers_mutex);
  pthread_mutex_lock(&server->sessions_mutex);

  if (!session->current_buffer_processing || session->current_buffer_done) {
    int32_t server_buffer_index;
    result = ert_server_next_buffer_index_pop(session, &server_buffer_index);

    if (result < 0) {
      pthread_mutex_unlock(&server->sessions_mutex);
      pthread_mutex_unlock(&server->buffers_mutex);
      ert_log_error("ert_server_next_buffer_index_pop failed with result: %d", result);
      return result;
    }

    if (server_buffer_index >= 0) {
      ert_server_buffer *server_buffer = &server->buffers[server_buffer_index];
      if (server_buffer->buffer_data_length > 0) {
        ws_send_message_init(session, wsi, server_buffer->buffer_data_length, server_buffer->buffer);
        continue_writing = true;
      }
    }
  } else if (session->current_buffer_processing) {
    continue_writing = true;
  }

  pthread_mutex_unlock(&server->sessions_mutex);
  pthread_mutex_unlock(&server->buffers_mutex);

  if (continue_writing) {
    result = ws_send_message_continue(session, wsi);
    if (result < 0) {
      ert_log_error("lws_callback_on_writable failed with result: %d", result);
      return result;
    }

    result = lws_callback_on_writable(wsi);
    if (result < 0) {
      ert_log_error("lws_callback_on_writable failed with result: %d", result);
      return result;
    }
  }

  return 0;
}

static int callback_websocket_datalogger(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
  struct lws_context *context = lws_get_context(wsi);
  ert_server *server = lws_context_user(context);
  ert_server_session *session = user;
  int result = 0;

  size_t client_address_buffer_length = 128;
  char client_address[client_address_buffer_length];

  switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);
      ert_log_info("WebSocket connection established: client_address=%s", client_address);

      session->wsi = wsi;

      pthread_mutex_lock(&server->buffers_mutex);
      ert_server_next_buffer_index_push(session, ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_NODE_TELEMETRY);
      ert_server_next_buffer_index_push(session, ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_GATEWAY_TELEMETRY);
      pthread_mutex_unlock(&server->buffers_mutex);

      result = ert_server_session_add(server, session);
      if (result < 0) {
        ert_log_error("No free WebSocket sessions available");
        return -1;
      }

      result = lws_callback_on_writable(wsi);
      if (result < 0) {
        ert_log_error("lws_callback_on_writable failed with result: %d", result);
        return result;
      }
      break;
    case LWS_CALLBACK_CLOSED:
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);
      ert_log_debug("WebSocket session closed: client_address=%s", client_address);

      ert_server_session_remove(server, session);

      // TODO: free session data if not freed
      return -1;
    case LWS_CALLBACK_SERVER_WRITEABLE:
      return serve_websocket_data(wsi, session);
    case LWS_CALLBACK_RECEIVE: {
      lws_get_peer_simple(wsi, client_address, client_address_buffer_length);
      ert_log_info("WebSocket received data: client_address=%s data=\"%s\"", client_address, (char *) in);
      break;
    }
    default:
      break;
  }

  return 0;
}

static struct lws_protocols protocols[] = {
    {
        "http-server",
        callback_http,
        sizeof(ert_server_session)
    },
    {
        ERT_SERVER_WEBSOCKET_PROTOCOL_DATA_LOGGER,
        callback_websocket_datalogger,
        sizeof(ert_server_session)
    },
    {
        NULL, NULL, 0
    }
};

static void *ert_server_handler(void *context)
{
  ert_server *server = (ert_server *) context;

  while (server->running) {
    int result = lws_service(server->lws_context, 50);
    if (result < 0) {
      ert_log_error("lws_service failed with result %d", result);
    }
  }

  return NULL;
}

static void ert_server_lws_log(int level, const char *message)
{
  switch (level) {
    case LLL_ERR:
      ert_log_error(message);
      break;
    case LLL_WARN:
      ert_log_warn(message);
      break;
    case LLL_NOTICE:
      ert_log_info(message);
      break;
    case LLL_INFO:
      ert_log_debug(message);
      break;
    case LLL_DEBUG:
      ert_log_debug(message);
      break;
    case LLL_PARSER:
      ert_log_debug(message);
      break;
    case LLL_HEADER:
      ert_log_debug(message);
      break;
    case LLL_EXT:
      ert_log_debug(message);
      break;
    case LLL_CLIENT:
      ert_log_debug(message);
      break;
    case LLL_LATENCY:
      ert_log_debug(message);
      break;
  }
}

static void ert_server_init_lws_logging()
{
  int levels = LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_CLIENT | LLL_LATENCY | LLL_EXT;
  lws_set_log_level(levels, ert_server_lws_log);
}

int ert_server_create(ert_server_config *config, ert_server **server_rcv)
{
  int result;

  ert_server *server = calloc(1, sizeof(ert_server));
  if (server == NULL) {
    ert_log_fatal("Error allocating memory for server struct: %s", strerror(errno));
    return -ENOMEM;
  }

  server->config = config;

  for (uint32_t i = 0; i < ERT_SERVER_BUFFER_COUNT; i++) {
    server->buffers[i].buffer = malloc(server->config->server_buffer_length);
    server->buffers[i].buffer_length = server->config->server_buffer_length;
    server->buffers[i].buffer_data_length = 0;

    if (server->buffers[i].buffer == NULL) {
      for (uint32_t j = 0; j < ERT_SERVER_BUFFER_COUNT; j++) {
        if (server->buffers[j].buffer != NULL) {
          free(server->buffers[j].buffer);
        }
      }
      free(server);
      ert_log_fatal("Error allocating memory for server buffer: %s", strerror(errno));
      return -ENOMEM;
    }
  }

  result = pthread_mutex_init(&server->buffers_mutex, NULL);
  if (result != 0) {
    for (uint32_t i = 0; i < ERT_SERVER_BUFFER_COUNT; i++) {
      free(server->buffers[i].buffer);
    }
    free(server);
    ert_log_error("Error initializing server data logger buffers mutex");
    return -EIO;
  }

  result = pthread_mutex_init(&server->sessions_mutex, NULL);
  if (result != 0) {
    pthread_mutex_destroy(&server->buffers_mutex);
    for (uint32_t i = 0; i < ERT_SERVER_BUFFER_COUNT; i++) {
      free(server->buffers[i].buffer);
    }
    free(server);
    ert_log_error("Error initializing server data logger sessions mutex");
    return -EIO;
  }

  ert_server_init_lws_logging();

  struct lws_context_creation_info info = {0};

  info.port = server->config->port;
  info.iface = NULL;
  info.ssl_cert_filepath = NULL;
  info.ssl_private_key_filepath = NULL;
  info.protocols = protocols;
  info.options = 0;
  info.uid = -1;
  info.gid = -1;
  info.user = server;
  info.ka_time = 60; // TCP keep-alive in seconds
  info.ka_probes = 3; // TCP keep-alive
  info.ka_interval = 1; // TCP keep-alive probe interval in seconds
  info.timeout_secs = 10; // Network operation timeout in seconds
  info.keepalive_timeout = 60; // HTTP keep-alive in seconds
  info.max_http_header_data = 16384;
  info.max_http_header_pool = 32;
  info.count_threads = 0; // Single service thread

  server->lws_context = lws_create_context(&info);

  if (server->lws_context == NULL) {
    pthread_mutex_destroy(&server->sessions_mutex);
    pthread_mutex_destroy(&server->buffers_mutex);
    for (uint32_t i = 0; i < ERT_SERVER_BUFFER_COUNT; i++) {
      free(server->buffers[i].buffer);
    }
    free(server);
    ert_log_error("lws_create_context failed");
    return -1;
  }

  *server_rcv = server;

  return 0;
}

ert_server_config *ert_server_get_config(ert_server *server)
{
  return server->config;
}

int ert_server_update_server_buffer(ert_server *server, int32_t server_buffer_index,
    uint32_t length, uint8_t *data)
{
  pthread_mutex_lock(&server->buffers_mutex);

  ert_server_buffer *server_buffer = &server->buffers[server_buffer_index];

  if (length > server_buffer->buffer_length) {
    pthread_mutex_unlock(&server->buffers_mutex);
    ert_log_error("Data length (%d) is greater than server buffer length (%d)",
        length, server_buffer->buffer_length);
    return -1;
  }

  memcpy(server_buffer->buffer, data, length);
  server_buffer->buffer_data_length = length;

  pthread_mutex_lock(&server->sessions_mutex);

  for (size_t i = 0; i < ERT_SERVER_SESSIONS_COUNT; i++) {
    ert_server_session *session = server->sessions[i];

    if (session != NULL) {
      ert_server_next_buffer_index_push(session, server_buffer_index);
      lws_callback_on_writable(session->wsi);
    }
  }

  pthread_mutex_unlock(&server->sessions_mutex);
  pthread_mutex_unlock(&server->buffers_mutex);

  return 0;
}

int ert_server_update_data_logger_entry(ert_server *server,
    ert_data_logger_serializer *serializer, ert_data_logger_entry *entry, int32_t server_buffer_index)
{
  uint32_t length;
  uint8_t *data;

  int result = serializer->serialize(serializer, entry, &length, &data);
  if (result < 0) {
    return result;
  }

  ert_server_update_server_buffer(server, server_buffer_index, length, data);

  free(data);

  return 0;
}

int ert_server_update_data_logger_entry_node(ert_server *server,
    ert_data_logger_serializer *serializer, ert_data_logger_entry *entry)
{
  return ert_server_update_data_logger_entry(server, serializer, entry,
      ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_NODE_TELEMETRY);
}

int ert_server_update_data_logger_entry_gateway(ert_server *server,
    ert_data_logger_serializer *serializer, ert_data_logger_entry *entry)
{
  return ert_server_update_data_logger_entry(server, serializer, entry,
      ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_GATEWAY_TELEMETRY);
}

int ert_server_update_node_image(ert_server *server, ert_image_metadata *metadata)
{
  int result;

  uint32_t length;
  uint8_t *data;

  result = ert_image_metadata_json_serialize(metadata, ERT_DATA_LOGGER_ENTRY_TYPE_IMAGE, &length, &data);
  if (result < 0) {
    ert_log_error("Error serializing image metadata, result %d", result);
    return -EIO;
  }

  ert_server_update_server_buffer(server, ERT_SERVER_BUFFER_INDEX_DATA_LOGGER_NODE_IMAGE, length, data);

  free(data);

  return 0;
}

int ert_server_update_data_logger_entry_transmitted(ert_server *server, ert_data_logger_entry *entry)
{
  return ert_server_status_update_last_transmitted_telemetry(entry, &server->server_status);
}

int ert_server_update_data_logger_entry_received(ert_server *server, ert_data_logger_entry *entry)
{
  return ert_server_status_update_last_received_telemetry(entry, &server->server_status);
}

int ert_server_record_data_logger_entry_transmission_failure(ert_server *server)
{
  return ert_server_status_record_telemetry_transmission_failure(&server->server_status);
}

int ert_server_record_data_logger_entry_reception_failure(ert_server *server)
{
  return ert_server_status_record_telemetry_reception_failure(&server->server_status);
}

int ert_server_start(ert_server *server)
{
  server->running = true;

  int result = pthread_create(&server->handler_thread, NULL, ert_server_handler, server);
  if (result != 0) {
    server->running = false;
    ert_log_error("Error starting handler thread for server: %s", strerror(errno));
    return -EIO;
  }

  return 0;
}

int ert_server_stop(ert_server *server)
{
  server->running = false;
  pthread_join(server->handler_thread, NULL);

  return 0;
}

int ert_server_destroy(ert_server *server)
{
  lws_context_destroy(server->lws_context);
  pthread_mutex_destroy(&server->sessions_mutex);
  pthread_mutex_destroy(&server->buffers_mutex);
  for (uint32_t i = 0; i < ERT_SERVER_BUFFER_COUNT; i++) {
    free(server->buffers[i].buffer);
  }
  free(server);

  return 0;
}
