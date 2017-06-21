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
#include "ertnode-sender-telemetry-gsm.h"

static int ert_node_telemetry_send_gsm_modem(ert_node *node, ert_data_logger_entry *entry)
{
  int result;

  char telemetry_timestamp[32];
  ert_format_iso8601_timestamp(&entry->timestamp, 32, (uint8_t *) telemetry_timestamp);

  char telemetry_sms[256];
  snprintf(telemetry_sms, 140, "Time: %s, Lat: %011.7f N, Lon: %011.7f E, Alt: %011.2f m",
      telemetry_timestamp, entry->params->gps_data.latitude_degrees, entry->params->gps_data.longitude_degrees,
      entry->params->gps_data.altitude_meters);

  result = ert_driver_gsm_modem_send_sms(node->gsm_modem, node->config.gsm_modem_config.destination_number, telemetry_sms);
  if (result < 0) {
    ert_log_error("ert_driver_gsm_modem_send_sms failed with result: %d", result);
    return result;
  }

  return 0;
}

static void ert_node_sender_telemetry_gsm_telemetry_collected_listener(char *event, void *data, void *context)
{
  ert_node *node = (ert_node *) context;
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  if (node->gsm_modem_initialized && node->config.gsm_modem_config.sms_telemetry_enabled) {
    ert_node_telemetry_send_gsm_modem(node, entry);
  }
}

int ert_node_sender_telemetry_gsm_initialize(ert_node *node)
{
  ert_event_emitter_add_listener(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED,
      ert_node_sender_telemetry_gsm_telemetry_collected_listener, node);

  return 0;
}

int ert_node_sender_telemetry_gsm_uninitialize(ert_node *node)
{
  ert_event_emitter_remove_listener(node->event_emitter, ERT_EVENT_NODE_TELEMETRY_COLLECTED,
      ert_node_sender_telemetry_gsm_telemetry_collected_listener);

  return 0;
}
