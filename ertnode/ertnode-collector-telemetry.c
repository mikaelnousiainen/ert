/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ertnode.h"

void *ert_node_telemetry_collector(void *context)
{
  ert_node *node = (ert_node *) context;
  int result;

  size_t telemetry_counter = 0;

  ert_log_info("Telemetry collector thread running");

  while (node->running) {
    ert_process_sleep_with_interrupt(
        node->config.sender_telemetry_config.telemetry_collect_interval_seconds, &node->running);

    if (!node->running) {
      break;
    }

    if (node->config.gps_config.enabled) {
      result = ert_data_logger_collect_entry_params_with_gps(node->data_logger,
          node->comm_transceiver, node->comm_protocol, node->gsm_modem, node->gps_listener, &node->data_logger_entry_params);
    } else {
      result = ert_data_logger_collect_entry_params(node->data_logger,
          node->comm_transceiver, node->comm_protocol, node->gsm_modem, &node->data_logger_entry_params);
    }
    if (result != 0) {
      ert_log_error("ert_data_logger_collect_entry_params failed with result: %d", result);
      continue;
    }

    ert_data_logger_entry entry = {0};
    result = ert_data_logger_populate_entry(node->data_logger, &node->data_logger_entry_params, &entry,
        ERT_DATA_LOGGER_ENTRY_TYPE_TELEMETRY);
    if (result != 0) {
      ert_log_error("ert_data_logger_populate_entry failed with result: %d", result);
      continue;
    }

    result = ert_data_logger_log(node->data_logger, &entry);
    if (result != 0) {
      ert_log_error("ert_data_logger_log failed with result: %d", result);
    }

    bool send_telemetry =
        (telemetry_counter % node->config.sender_telemetry_config.telemetry_send_interval) == 0;
    if (send_telemetry) {
      ert_event_emitter_emit(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED, &entry);
    }

    telemetry_counter++;
  }

  ert_log_info("Telemetry collector thread stopping");

  return NULL;
}
