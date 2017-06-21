/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-server-config.h"

ert_mapper_entry *ert_server_create_mappings(ert_server_config *gateway_server_config)
{
  ert_mapper_entry gateway_server_data_logger_children[] = {
      {
          .name = "path",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gateway_server_config->data_logger_path,
          .maximum_length = 1024,
      },
      {
          .name = "node_filename_template",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gateway_server_config->data_logger_node_filename_template,
          .maximum_length = 256,
      },
      {
          .name = "gateway_filename_template",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gateway_server_config->data_logger_gateway_filename_template,
          .maximum_length = 256,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry gateway_server_children[] = {
      {
          .name = "enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &gateway_server_config->enabled,
      },
      {
          .name = "port",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &gateway_server_config->port,
      },
      {
          .name = "buffer_length",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &gateway_server_config->server_buffer_length,
      },
      {
          .name = "image_path",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gateway_server_config->image_path,
          .maximum_length = 1024,
      },
      {
          .name = "static_path",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gateway_server_config->static_path,
          .maximum_length = 1024,
      },
      {
          .name = "data_logger",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = gateway_server_data_logger_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(gateway_server_children);
}
