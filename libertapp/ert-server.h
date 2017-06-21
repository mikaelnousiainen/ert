/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_SERVER_H
#define __ERT_SERVER_H

#include "ert-common.h"
#include "ert-data-logger.h"
#include "ert-event-emitter.h"
#include "ert-mapper.h"
#include "ert-image-metadata.h"

typedef struct _ert_server_config {
  bool enabled;
  uint32_t port;

  uint32_t server_buffer_length;

  char image_path[1024];
  char static_path[1024];
  char data_logger_path[1024];
  char data_logger_node_filename_template[256];
  char data_logger_gateway_filename_template[256];

  ert_event_emitter *event_emitter;
  ert_data_logger_serializer *data_logger_entry_serializer;

  ert_comm_protocol *app_comm_protocol;

  ert_mapper_entry *app_config_root_entry;
  void *app_config_update_context;
} ert_server_config;

typedef struct _ert_server ert_server;

int ert_server_create(ert_server_config *config, ert_server **server_rcv);
int ert_server_start(ert_server *server);
int ert_server_stop(ert_server *server);
int ert_server_destroy(ert_server *server);
ert_server_config *ert_server_get_config(ert_server *server);

int ert_server_update_server_buffer(ert_server *server, int32_t server_buffer_index,
    uint32_t length, uint8_t *data);
int ert_server_update_data_logger_entry(ert_server *server,
    ert_data_logger_serializer *serializer, ert_data_logger_entry *entry, int32_t server_buffer_index);
int ert_server_update_data_logger_entry_node(ert_server *server,
    ert_data_logger_serializer *serializer, ert_data_logger_entry *entry);
int ert_server_update_data_logger_entry_gateway(ert_server *server,
    ert_data_logger_serializer *serializer, ert_data_logger_entry *entry);
int ert_server_update_node_image(ert_server *server, ert_image_metadata *metadata);

#endif
