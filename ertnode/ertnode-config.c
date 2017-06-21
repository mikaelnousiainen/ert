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

#include "ert-yaml.h"
#include "ert-driver-rfm9xw-config.h"
#include "ert-driver-gsm-modem-config.h"
#include "ert-comm-transceiver-config.h"
#include "ert-comm-protocol-config.h"
#include "ert-server-config.h"
#include "ertnode-config.h"

int ert_node_rfm9xw_config_updated(ert_mapper_entry *entry, void *context)
{
  ert_node *node = (ert_node *) context;

  ert_log_info("Updating RFM9xW comm device configuration to:");
  ert_mapper_log_entries(entry);

  return ert_comm_transceiver_configure(node->comm_transceiver, &node->config.rfm9xw_config);
}

ert_mapper_entry *ert_node_sender_image_create_mappings(ert_node_sender_image_config *sender_image_config)
{
  ert_mapper_entry original_image_children[] = {
      {
          .name = "quality",
          .type = ERT_MAPPER_ENTRY_TYPE_INT16,
          .value = &sender_image_config->original_image_quality,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry transmitted_image_children[] = {
      {
          .name = "width",
          .type = ERT_MAPPER_ENTRY_TYPE_INT16,
          .value = &sender_image_config->transmitted_image_width,
      },
      {
          .name = "height",
          .type = ERT_MAPPER_ENTRY_TYPE_INT16,
          .value = &sender_image_config->transmitted_image_height,
      },
      {
          .name = "format",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &sender_image_config->transmitted_image_format,
          .maximum_length = 16,
      },
      {
          .name = "quality",
          .type = ERT_MAPPER_ENTRY_TYPE_INT16,
          .value = &sender_image_config->transmitted_image_quality,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry image_sender_children[] = {
      {
          .name = "enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &sender_image_config->enabled,
      },
      {
          .name = "image_capture_interval_seconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &sender_image_config->image_capture_interval_seconds,
      },
      {
          .name = "image_send_interval",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &sender_image_config->image_send_interval,
      },
      {
          .name = "raspistill_command",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &sender_image_config->raspistill_command,
          .maximum_length = 1024,
      },
      {
          .name = "convert_command",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &sender_image_config->convert_command,
          .maximum_length = 1024,
      },
      {
          .name = "horizontal_flip",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &sender_image_config->horizontal_flip,
      },
      {
          .name = "vertical_flip",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &sender_image_config->vertical_flip,
      },
      {
          .name = "image_path",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &sender_image_config->image_path,
          .maximum_length = 1024,
      },
      {
          .name = "original_image",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = original_image_children,
      },
      {
          .name = "transmitted_image",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = transmitted_image_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(image_sender_children);
}

ert_mapper_entry *ert_node_sender_telemetry_create_mappings(ert_node_sender_telemetry_config *sender_telemetry_config)
{
  ert_mapper_entry telemetry_sender_children[] = {
      {
          .name = "enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &sender_telemetry_config->enabled,
      },
      {
          .name = "telemetry_collect_interval_seconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &sender_telemetry_config->telemetry_collect_interval_seconds,
      },
      {
          .name = "telemetry_send_interval",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &sender_telemetry_config->telemetry_send_interval,
      },
      {
          .name = "minimal_telemetry_data_send_interval",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &sender_telemetry_config->minimal_telemetry_data_send_interval,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(telemetry_sender_children);
}

ert_mapper_entry *ert_node_gps_create_mappings(ert_node_gps_config *gps_config)
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

ert_mapper_entry *ert_node_gsm_modem_create_mappings(ert_node_gsm_modem_config *gsm_modem_config)
{
  ert_mapper_entry *modem_children = ert_driver_gsm_modem_create_mappings(&gsm_modem_config->modem_config);

  ert_mapper_entry gsm_modem_children[] = {
      {
          .name = "enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &gsm_modem_config->enabled,
      },
      {
          .name = "sms_telemetry_enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &gsm_modem_config->sms_telemetry_enabled,
      },
      {
          .name = "sms_telemetry_send_interval",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &gsm_modem_config->sms_telemetry_send_interval,
      },
      {
          .name = "destination_number",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &gsm_modem_config->destination_number,
          .maximum_length = 32,
      },
      {
          .name = "modem",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = modem_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(gsm_modem_children);
}

ert_mapper_entry *ert_node_configuration_mapper_create(ert_node_config *config)
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
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry *image_sender_children =
      ert_node_sender_image_create_mappings(&config->sender_image_config);

  ert_mapper_entry *telemetry_sender_children =
      ert_node_sender_telemetry_create_mappings(&config->sender_telemetry_config);

  ert_mapper_entry *gps_children = ert_node_gps_create_mappings(&config->gps_config);

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
          .updated = ert_node_rfm9xw_config_updated,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry *gsm_modem_children = ert_node_gsm_modem_create_mappings(&config->gsm_modem_config);

  ert_mapper_entry root_children[] = {
      {
          .name = "global",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = global_children,
      },
      {
          .name = "telemetry_sender",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = telemetry_sender_children,
          .children_allocated = true,
      },
      {
          .name = "image_sender",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = image_sender_children,
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
          .name = "gsm_modem",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = gsm_modem_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(root_children);
}

int ert_node_read_configuration(ert_node_config *config, char *config_file_name)
{
  FILE *config_file = fopen(config_file_name, "rb");
  if (config_file == NULL) {
    ert_log_error("Error opening configuration file '%s': %s", config_file_name, strerror(errno));
    return -ENOENT;
  }

  ert_mapper_entry *config_root_entry = ert_node_configuration_mapper_create(config);
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
