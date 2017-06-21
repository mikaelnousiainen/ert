/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-driver-gsm-modem-config.h"

ert_mapper_entry *ert_driver_gsm_modem_create_mappings(ert_driver_gsm_modem_config *modem_config)
{
  ert_mapper_entry modem_serial_device_children[] = {
      {
          .name = "device",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &modem_config->serial_device_config.device,
          .maximum_length = 256,
      },
      {
          .name = "parity",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &modem_config->serial_device_config.parity,
      },
      {
          .name = "stop_bits_2",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &modem_config->serial_device_config.stop_bits_2,
      },
      {
          .name = "speed",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &modem_config->serial_device_config.speed,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry modem_children[] = {
      {
          .name = "command_queue_size",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &modem_config->command_queue_size,
      },
      {
          .name = "poll_interval_millis",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &modem_config->poll_interval_millis,
      },
      {
          .name = "response_timeout_millis",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &modem_config->response_timeout_millis,
      },
      {
          .name = "pin",
          .type = ERT_MAPPER_ENTRY_TYPE_STRING,
          .value = &modem_config->pin,
          .maximum_length = 8,
      },
      {
          .name = "retry_count",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &modem_config->retry_count,
      },
      {
          .name = "serial_device",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = modem_serial_device_children,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(modem_children);
}
