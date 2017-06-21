/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-mapper.h"
#include "ert-yaml.h"
#include "ert-driver-rfm9xw-config.h"
#include "ert-driver-st7036-config.h"
#include "ert-comm-transceiver-config.h"
#include "ert-comm-protocol-config.h"
#include "ert-server-config.h"
#include "ertgateway-config.h"

int ert_gateway_rfm9xw_config_updated(ert_mapper_entry *entry, void *context)
{
  ert_gateway *gateway = (ert_gateway *) context;

  ert_log_info("Updating RFM9xW comm device configuration to:");
  ert_mapper_log_entries(entry);

  return ert_comm_transceiver_configure(gateway->comm_transceiver, &gateway->config.rfm9xw_config);
}

ert_mapper_entry *ert_gateway_handler_display_create_mappings(ert_gateway_handler_display_config *handler_display_config)
{
  ert_mapper_entry handler_display_children[] = {
      {
          .name = "display_update_interval_seconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &handler_display_config->display_update_interval_seconds,
      },
      {
          .name = "display_mode_cycle_length_in_update_intervals",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &handler_display_config->display_mode_cycle_length_in_update_intervals,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(handler_display_children);
}

ert_mapper_entry *ert_gateway_dothat_create_mappings(ert_gateway_ui_dothat_config *ui_dothat_config)
{
  ert_mapper_entry children[] = {
      {
          .name = "backlight_enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &ui_dothat_config->backlight_enabled,
      },
      {
          .name = "touch_enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &ui_dothat_config->touch_enabled,
      },
      {
          .name = "leds_enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &ui_dothat_config->leds_enabled,
      },
      {
          .name = "brightness",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &ui_dothat_config->brightness,
      },
      {
          .name = "backlight_color",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &ui_dothat_config->backlight_color,
      },
      {
          .name = "backlight_error_color",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &ui_dothat_config->backlight_error_color,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(children);
}

ert_mapper_entry *ert_gateway_handler_telemetry_create_mappings(ert_gateway_handler_telemetry_config *handler_telemetry_config)
{
  ert_mapper_entry handler_display_children[] = {
      {
          .name = "gateway_telemetry_collect_interval_seconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &handler_telemetry_config->gateway_telemetry_collect_interval_seconds,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(handler_display_children);
}

ert_mapper_entry *ert_gateway_handler_image_create_mappings(ert_gateway_handler_image_config *handler_image_config)
{
  ert_mapper_entry handler_image_children[] = {
      {
          .name = "image_path",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &handler_image_config->image_path,
          .maximum_length = 1024,
      },
      {
          .name = "image_format",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &handler_image_config->image_format,
          .maximum_length = 16,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(handler_image_children);
}

ert_mapper_entry *ert_gateway_gps_create_mappings(ert_gateway_gps_config *gps_config)
{
  ert_mapper_entry gps_gpsd_children[] = {
      {
          .name = "enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &gps_config->enabled,
      },
      {
          .name = "host",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gps_config->gpsd_host,
          .maximum_length = 256,
      },
      {
          .name = "port",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &gps_config->gpsd_port,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry gps_children[] = {
      {
          .name = "gpsd",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = gps_gpsd_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(gps_children);
}

ert_mapper_entry *ert_gateway_configuration_mapper_create(ert_gateway_config *config)
{
  ert_mapper_entry global_children[] = {
      {
          .name = "device_name",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = config->device_name,
          .maximum_length = 128,
      },
      {
          .name = "device_model",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = config->device_model,
          .maximum_length = 128,
      },
      {
          .name = "thread_count_per_comm_handler",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->thread_count_per_comm_handler,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry *display_handler_children =
      ert_gateway_handler_display_create_mappings(&config->handler_display_config);

  ert_mapper_entry *image_handler_children =
      ert_gateway_handler_image_create_mappings(&config->handler_image_config);

  ert_mapper_entry *telemetry_handler_children =
      ert_gateway_handler_telemetry_create_mappings(&config->handler_telemetry_config);

  ert_mapper_entry *gps_children = ert_gateway_gps_create_mappings(&config->gps_config);

  ert_mapper_entry *server_children =
      ert_server_create_mappings(&config->server_config);

  ert_mapper_entry *rfm9xw_children = ert_driver_rfm9xw_create_mappings(
      &config->rfm9xw_static_config, &config->rfm9xw_config);

  ert_mapper_entry *comm_transceiver_children =
      ert_comm_transceiver_create_mappings(&config->comm_transceiver_config);
  ert_mapper_entry *comm_protocol_children =
      ert_comm_protocol_create_mappings(&config->comm_protocol_config);

  ert_mapper_entry comm_devices_children[] = {
      {
          .name = "rfm9xw",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = rfm9xw_children,
          .children_allocated = true,
          .updated = ert_gateway_rfm9xw_config_updated,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry *st7036_children = ert_driver_st7036_create_mappings(&config->st7036_static_config);
  ert_mapper_entry *dothat_children = ert_gateway_dothat_create_mappings(&config->dothat_config);

  ert_mapper_entry ui_children[] = {
      {
          .name = "st7036_enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &config->st7036_enabled,
      },
      {
          .name = "st7036",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = st7036_children,
          .children_allocated = true,
      },
      {
          .name = "dothat",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = dothat_children,
          .children_allocated = true,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry root_children[] = {
      {
          .name = "global",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = global_children,
      },
      {
          .name = "display_handler",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = display_handler_children,
          .children_allocated = true,
      },
      {
          .name = "image_handler",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = image_handler_children,
          .children_allocated = true,
      },
      {
          .name = "telemetry_handler",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = telemetry_handler_children,
          .children_allocated = true,
      },
      {
          .name = "gps",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = gps_children,
          .children_allocated = true,
      },
      {
          .name = "server",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = server_children,
          .children_allocated = true,
      },
      {
          .name = "ui",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = ui_children,
      },
      {
          .name = "comm_transceiver",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = comm_transceiver_children,
          .children_allocated = true,
      },
      {
          .name = "comm_protocol",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = comm_protocol_children,
          .children_allocated = true,
      },
      {
          .name = "comm_devices",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = comm_devices_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(root_children);
}

int ert_gateway_read_configuration(ert_gateway_config *config, char *config_file_name)
{
  FILE *config_file = fopen(config_file_name, "rb");
  if (config_file == NULL) {
    ert_log_error("Error opening configuration file '%s': %s", config_file_name, strerror(errno));
    return -ENOENT;
  }

  ert_mapper_entry *config_root_entry = ert_gateway_configuration_mapper_create(config);
  if (config_root_entry == NULL) {
    fclose(config_file);
    ert_log_fatal("Error creating mappings for configuration: %s", strerror(errno));
    return -ENOMEM;
  }

  int result = ert_yaml_parse_file(config_file, config_root_entry);
  if (result == 0) {
    ert_log_info("Using configuration:");
    ert_mapper_log_entries(config_root_entry);
  }
  ert_mapper_deallocate(config_root_entry);

  fclose(config_file);

  if (result < 0) {
    ert_log_error("Error parsing configuration file '%s'", config_file_name);
    return -EBADMSG;
  }

  return 0;
}
