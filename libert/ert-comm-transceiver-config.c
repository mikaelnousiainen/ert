/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-comm-transceiver-config.h"

ert_mapper_entry *ert_comm_transceiver_create_mappings(ert_comm_transceiver_config *config)
{
  ert_mapper_entry comm_protocol_children[] = {
      {
          .name = "transmit_buffer_length_packets",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->transmit_buffer_length_packets,
      },
      {
          .name = "receive_buffer_length_packets",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->receive_buffer_length_packets,
      },
      {
          .name = "transmit_timeout_milliseconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->transmit_timeout_milliseconds,
      },
      {
          .name = "poll_interval_milliseconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->poll_interval_milliseconds,
      },
      {
          .name = "maximum_receive_time_milliseconds",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->maximum_receive_time_milliseconds,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(comm_protocol_children);
}
