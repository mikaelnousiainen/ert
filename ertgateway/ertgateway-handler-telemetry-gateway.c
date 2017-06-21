/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <inttypes.h>

#include "ertgateway.h"
#include "ert-geo.h"

static void ert_gateway_handler_telemetry_populate_related(
    ert_data_logger_entry *gateway_entry, ert_data_logger_entry *related_entry, ert_data_logger_entry_related *related)
{
  related->entry = related_entry;

  ert_data_logger_entry_params *gateway_params = gateway_entry->params;
  ert_gps_data *gateway_gps = &gateway_params->gps_data;
  ert_data_logger_entry_params *related_params = related_entry->params;
  ert_gps_data *related_gps = &related_params->gps_data;

  if (gateway_params->gps_data_present && gateway_gps->has_fix
      && related_params->gps_data_present && related_gps->has_fix) {
    related->altitude_diff_meters =
        related_entry->params->gps_data.altitude_meters - gateway_entry->params->gps_data.altitude_meters;

    ert_geo_inverse_wgs84(
        gateway_gps->latitude_degrees, gateway_gps->longitude_degrees,
        related_gps->latitude_degrees, related_gps->longitude_degrees,
        &related->distance_meters, &related->azimuth_degrees, NULL);

    // TODO: elevation?
    related->elevation_degrees = 0;
  } else {
    related->altitude_diff_meters = 0;
    related->azimuth_degrees = 0;
    related->elevation_degrees = 0;
    related->distance_meters = 0;
  }
}

int ert_gateway_handler_telemetry_gateway_collect_and_log(ert_gateway *gateway)
{
  int result;

#ifdef ERTGATEWAY_SUPPORT_GPSD
  if (gateway->config.gps_config.enabled) {
    result = ert_data_logger_collect_entry_params_with_gps(gateway->data_logger_gateway,
        gateway->comm_transceiver, gateway->comm_protocol, NULL, gateway->gps_listener, &gateway->data_logger_entry_params_gateway);
  } else {
    result = ert_data_logger_collect_entry_params(gateway->data_logger_gateway,
        gateway->comm_transceiver, gateway->comm_protocol, NULL, &gateway->data_logger_entry_params_gateway);
  }
#else
  result = ert_data_logger_collect_entry_params(gateway->data_logger_gateway,
      gateway->comm_transceiver, gateway->comm_protocol, NULL, &gateway->data_logger_entry_params_gateway);
#endif
  if (result != 0) {
    ert_log_error("ert_data_logger_collect_entry_params failed with result: %d", result);
    return result;
  }

  ert_data_logger_entry entry = {0};
  result = ert_data_logger_populate_entry(gateway->data_logger_gateway, &gateway->data_logger_entry_params_gateway, &entry,
      ERT_DATA_LOGGER_ENTRY_TYPE_GATEWAY);
  if (result != 0) {
    ert_log_error("ert_data_logger_populate_entry failed with result: %d", result);
    return result;
  }

  pthread_mutex_lock(&gateway->related_entry_mutex);

  if (gateway->related_entry == NULL) {
    entry.params->related_entry_count = 0;
    entry.params->related[0] = NULL;
  } else {
    ert_data_logger_entry_related related;
    ert_gateway_handler_telemetry_populate_related(&entry, gateway->related_entry, &related);

    entry.params->related_entry_count = 1;
    entry.params->related[0] = &related;
  }

  result = ert_data_logger_log(gateway->data_logger_gateway, &entry);
  if (result != 0) {
    ert_log_error("ert_data_logger_log failed with result: %d", result);
  }

  ert_event_emitter_emit(gateway->event_emitter, ERT_EVENT_GATEWAY_TELEMETRY_RECEIVED, &entry);

  pthread_mutex_unlock(&gateway->related_entry_mutex);

  return 0;
}

void *ert_gateway_handler_telemetry_gateway(void *context)
{
  ert_gateway *gateway = (ert_gateway *) context;

  ert_log_info("Gateway telemetry handler thread running");

  while (gateway->running) {
    ert_gateway_handler_telemetry_gateway_collect_and_log(gateway);

    ert_process_sleep_with_interrupt(
        gateway->config.handler_telemetry_config.gateway_telemetry_collect_interval_seconds, &gateway->running);
  }

  ert_log_info("Gateway telemetry handler thread stopping");

  return NULL;
}
