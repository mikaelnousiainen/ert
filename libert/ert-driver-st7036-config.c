/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-driver-st7036-config.h"

uint8_t st7036_bias_setting_values[] = {
    ERT_DRIVER_ST7036_BIAS_1_4, ERT_DRIVER_ST7036_BIAS_1_5
};

ert_mapper_enum_entry st7036_enum_bias_settings[] = {
    {
        .name = "1/4",
        .value = &st7036_bias_setting_values[0],
    },
    {
        .name = "1/5",
        .value = &st7036_bias_setting_values[1],
    },
    {
        .name = NULL,
    },
};

ert_mapper_entry *ert_driver_st7036_create_mappings(ert_driver_st7036_static_config *static_config)
{
  ert_mapper_entry st7036_spi_children[] = {
      {
          .name = "bus_index",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT16,
          .value = &static_config->spi_bus_index,
      },
      {
          .name = "device_index",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT16,
          .value = &static_config->spi_device_index,
      },
      {
          .name = "clock_speed",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &static_config->spi_clock_speed,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry st7036_pins_children[] = {
      {
          .name = "reset",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->pin_reset,
      },
      {
          .name = "register_select",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->pin_register_select,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry st7036_children[] = {
      {
          .name = "spi",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = st7036_spi_children,
      },
      {
          .name = "pins",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = st7036_pins_children,
      },
      {
          .name = "columns",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->columns,
      },
      {
          .name = "rows",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->rows,
      },
      {
          .name = "bias",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->bias,
          .enum_entries = st7036_enum_bias_settings,
      },
      {
          .name = "power_boost",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &static_config->power_boost,
      },
      {
          .name = "follower_circuit",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &static_config->follower_circuit,
      },
      {
          .name = "follower_circuit_amplified_ratio",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->follower_circuit_amplified_ratio,
      },
      {
          .name = "initial_contrast",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->initial_contrast,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(st7036_children);
}
