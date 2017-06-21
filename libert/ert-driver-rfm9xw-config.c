/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-driver-rfm9xw-config.h"

uint8_t rfm9xw_error_coding_rates[] = {
    ERROR_CODING_4_5, ERROR_CODING_4_6, ERROR_CODING_4_7, ERROR_CODING_4_8
};

ert_mapper_enum_entry rfm9xw_enum_error_coding_rates[] = {
    {
        .name = "4:5",
        .value = &rfm9xw_error_coding_rates[0],
    },
    {
        .name = "4:6",
        .value = &rfm9xw_error_coding_rates[1],
    },
    {
        .name = "4:7",
        .value = &rfm9xw_error_coding_rates[2],
    },
    {
        .name = "4:8",
        .value = &rfm9xw_error_coding_rates[3],
    },
    {
        .name = NULL,
    },
};

uint8_t rfm9xw_bandwidths[] = {
    BANDWIDTH_7K8,
    BANDWIDTH_10K4,
    BANDWIDTH_15K6,
    BANDWIDTH_20K8,
    BANDWIDTH_31K25,
    BANDWIDTH_41K7,
    BANDWIDTH_62K5,
    BANDWIDTH_125K,
    BANDWIDTH_250K,
    BANDWIDTH_500K,
};

ert_mapper_enum_entry rfm9xw_enum_bandwidths[] = {
    {
        .name = "7K8",
        .value = &rfm9xw_bandwidths[0],
    },
    {
        .name = "10K4",
        .value = &rfm9xw_bandwidths[1],
    },
    {
        .name = "15K6",
        .value = &rfm9xw_bandwidths[2],
    },
    {
        .name = "20K8",
        .value = &rfm9xw_bandwidths[3],
    },
    {
        .name = "31K25",
        .value = &rfm9xw_bandwidths[4],
    },
    {
        .name = "41K7",
        .value = &rfm9xw_bandwidths[5],
    },
    {
        .name = "62K5",
        .value = &rfm9xw_bandwidths[6],
    },
    {
        .name = "125K",
        .value = &rfm9xw_bandwidths[7],
    },
    {
        .name = "250K",
        .value = &rfm9xw_bandwidths[8],
    },
    {
        .name = "500K",
        .value = &rfm9xw_bandwidths[9],
    },
    {
        .name = NULL,
    },
};

ert_mapper_entry *ert_driver_rfm9xw_radio_config_create_mappings(ert_driver_rfm9xw_radio_config *radio_config)
{
  ert_mapper_entry rfm9xw_radio_config_children[] = {
      {
          .name = "pa_boost",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &radio_config->pa_boost,
      },
      {
          .name = "pa_output_power",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->pa_output_power,
      },
      {
          .name = "pa_max_power",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->pa_max_power,
      },
      {
          .name = "frequency",
          .type = ERT_MAPPER_ENTRY_TYPE_DOUBLE,
          .value = &radio_config->frequency,
      },
      {
          .name = "implicit_header_mode",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &radio_config->implicit_header_mode,
      },
      {
          .name = "error_coding_rate",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->error_coding_rate,
          .enum_entries = rfm9xw_enum_error_coding_rates,
      },
      {
          .name = "bandwidth",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->bandwidth,
          .enum_entries = rfm9xw_enum_bandwidths,
      },
      {
          .name = "spreading_factor",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->spreading_factor,
      },
      {
          .name = "crc",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &radio_config->crc,
      },
      {
          .name = "low_data_rate_optimize",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &radio_config->low_data_rate_optimize,
      },
      {
          .name = "expected_payload_length",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->expected_payload_length,
      },
      {
          .name = "preamble_length",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT16,
          .value = &radio_config->preamble_length,
      },
      {
          .name = "iq_inverted",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &radio_config->iq_inverted,
      },
      {
          .name = "receive_timeout_symbols",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT16,
          .value = &radio_config->receive_timeout_symbols,
      },
      {
          .name = "frequency_hop_enabled",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &radio_config->frequency_hop_enabled,
      },
      {
          .name = "frequency_hop_period",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &radio_config->frequency_hop_period,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(rfm9xw_radio_config_children);
}

ert_mapper_entry *ert_driver_rfm9xw_create_mappings(ert_driver_rfm9xw_static_config *static_config,
    ert_driver_rfm9xw_config *config)
{
  ert_mapper_entry rfm9xw_spi_children[] = {
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

  ert_mapper_entry rfm9xw_pins_children[] = {
      {
          .name = "dio0",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->pin_dio0,
      },
      {
          .name = "dio5",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT8,
          .value = &static_config->pin_dio5,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  ert_mapper_entry *rfm9xw_transmit_children =
      ert_driver_rfm9xw_radio_config_create_mappings(&config->transmit_config);
  ert_mapper_entry *rfm9xw_receive_children =
      ert_driver_rfm9xw_radio_config_create_mappings(&config->receive_config);

  ert_mapper_entry rfm9xw_children[] = {
      {
          .name = "spi",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = rfm9xw_spi_children,
      },
      {
          .name = "pins",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = rfm9xw_pins_children,
      },
      {
          .name = "receive_single_after_detection",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &static_config->receive_single_after_detection,
      },
      {
          .name = "transmit",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = rfm9xw_transmit_children,
          .children_allocated = true,
      },
      {
          .name = "receive",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = rfm9xw_receive_children,
          .children_allocated = true,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(rfm9xw_children);
}
