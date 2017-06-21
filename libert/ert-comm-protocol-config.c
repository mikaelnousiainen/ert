/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-comm-protocol-config.h"

ert_mapper_entry *ert_comm_protocol_create_mappings(ert_comm_protocol_config *config)
{
  ert_mapper_entry comm_protocol_children[] = {
      {
          .name = "passive_mode",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &config->passive_mode,
      },
      {
          .name = "transmit_all_data",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &config->transmit_all_data,
      },
      {
          .name = "ignore_errors",
          .type = ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
          .value = &config->ignore_errors,
      },
      {
          .name = "receive_buffer_length_packets",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->receive_buffer_length_packets,
      },
      {
          .name = "stream_inactivity_timeout_millis",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->stream_inactivity_timeout_millis,
      },
      {
          .name = "stream_acknowledgement_interval_packet_count",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->stream_acknowledgement_interval_packet_count,
      },
      {
          .name = "stream_acknowledgement_receive_timeout_millis",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->stream_acknowledgement_receive_timeout_millis,
      },
      {
          .name = "stream_acknowledgement_guard_interval_millis",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->stream_acknowledgement_guard_interval_millis,
      },
      {
          .name = "stream_acknowledgement_max_rerequest_count",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->stream_acknowledgement_max_rerequest_count,
      },
      {
          .name = "stream_end_of_stream_acknowledgement_max_rerequest_count",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT32,
          .value = &config->stream_end_of_stream_acknowledgement_max_rerequest_count,
      },
      {
          .name = "transmit_stream_count",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT16,
          .value = &config->transmit_stream_count,
      },
      {
          .name = "receive_stream_count",
          .type = ERT_MAPPER_ENTRY_TYPE_UINT16,
          .value = &config->receive_stream_count,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_mapper_allocate(comm_protocol_children);
}
