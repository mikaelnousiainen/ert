/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-server.h"
#include "ertgateway-common.h"
#include "ertgateway-server.h"

static void ert_gateway_server_node_telemetry_listener(char *event, void *data, void *context)
{
  ert_server *server = (ert_server *) context;
  ert_server_config *server_config = ert_server_get_config(server);
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  ert_server_update_data_logger_entry_node(server, server_config->data_logger_entry_serializer, entry);
}

static void ert_gateway_server_gateway_telemetry_listener(char *event, void *data, void *context)
{
  ert_server *server = (ert_server *) context;
  ert_server_config *server_config = ert_server_get_config(server);
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  ert_server_update_data_logger_entry_gateway(server, server_config->data_logger_entry_serializer, entry);
}

static void ert_gateway_server_node_image_listener(char *event, void *data, void *context)
{
  ert_server *server = (ert_server *) context;
  ert_image_metadata *metadata = (ert_image_metadata *) data;

  ert_server_update_node_image(server, metadata);
}

void ert_gateway_server_attach_events(ert_event_emitter *event_emitter, ert_server *server)
{
  ert_event_emitter_add_listener(event_emitter, ERT_EVENT_NODE_TELEMETRY_RECEIVED,
      ert_gateway_server_node_telemetry_listener, server);
  ert_event_emitter_add_listener(event_emitter, ERT_EVENT_GATEWAY_TELEMETRY_RECEIVED,
      ert_gateway_server_gateway_telemetry_listener, server);

  ert_event_emitter_add_listener(event_emitter, ERT_EVENT_NODE_IMAGE_RECEIVED,
      ert_gateway_server_node_image_listener, server);
}

void ert_gateway_server_detach_events(ert_event_emitter *event_emitter)
{
  ert_event_emitter_remove_listener(event_emitter, ERT_EVENT_NODE_TELEMETRY_RECEIVED,
      ert_gateway_server_node_telemetry_listener);
  ert_event_emitter_remove_listener(event_emitter, ERT_EVENT_GATEWAY_TELEMETRY_RECEIVED,
      ert_gateway_server_gateway_telemetry_listener);

  ert_event_emitter_remove_listener(event_emitter, ERT_EVENT_NODE_IMAGE_RECEIVED,
      ert_gateway_server_node_image_listener);
}