/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <inttypes.h>
#include "ert-data-logger-utils.h"

#include "ert-log.h"
#include "ert-time.h"
#include "ert-driver-rfm9xw.h"

void ert_data_logger_log_gps_data(ert_log_logger *logger, ert_gps_data *gps_data)
{
  ert_logl_info(logger, "GPS: Fix: %d, Mode: %d,\n"
      "  Satellites used: %d, Satellites visible: %d,\n"
      "  Latitude: %8.6f deg N, Longitude: %8.6f deg E, Altitude: %8.6f m\n"
      "  Speed: %8.4f m/s Climb: %8.4f m/s",
      gps_data->has_fix, gps_data->mode,
      gps_data->satellites_used, gps_data->satellites_visible,
      gps_data->latitude_degrees, gps_data->longitude_degrees, gps_data->altitude_meters,
      gps_data->speed_meters_per_sec, gps_data->climb_meters_per_sec);
}

static void ert_data_logger_log_rfm9xw_status(ert_log_logger *logger, ert_driver_rfm9xw_status *status)
{
  ert_logl_info(logger, "Comm device status for RFM9xW:");
  ert_logl_info(logger, "  Chip version: %02X", status->chip_version);

  ert_logl_info(logger, "  Signal detected: %d", status->signal_detected);
  ert_logl_info(logger, "  Signal synchronized: %d", status->signal_synchronized);
  ert_logl_info(logger, "  RX active: %d", status->rx_active);
  ert_logl_info(logger, "  Header info valid: %d", status->header_info_valid);
  ert_logl_info(logger, "  Modem clear: %d", status->modem_clear);
}

void ert_data_logger_log_comm_device_status(ert_log_logger *logger, ert_comm_device_status *status)
{
  ert_logl_info(logger, "Comm device status:");
  ert_logl_info(logger, "  Device name: %d", status->name);
  ert_logl_info(logger, "  Device state: %d", status->device_state);
  ert_logl_info(logger, "  Frequency: %08.3f", status->frequency);
  ert_logl_info(logger, "  Last received packet RSSI: %07.2f dBm", status->last_received_packet_rssi);
  ert_logl_info(logger, "  Current RSSI: %07.2f dBm", status->current_rssi);

  ert_logl_info(logger, "  TX packet count: %" PRIu64, status->transmitted_packet_count);
  ert_logl_info(logger, "  TX bytes: %" PRIu64, status->transmitted_bytes);
  ert_logl_info(logger, "  RX packet count: %" PRIu64, status->received_packet_count);
  ert_logl_info(logger, "  RX bytes: %" PRIu64, status->received_bytes);
  ert_logl_info(logger, "  RX error count: %" PRIu64, status->invalid_received_packet_count);

  if (status->custom != NULL) {
    ert_data_logger_log_rfm9xw_status(logger, (ert_driver_rfm9xw_status *) status->custom);
  }
}

void ert_data_logger_log_sensor_data(ert_log_logger *logger, ert_sensor_module_data *sensor_module_data)
{
  ert_logl_info(logger, "Sensor module %s:", sensor_module_data->module->name);

  for (int sensor_index = 0; sensor_index < sensor_module_data->module->sensor_count; sensor_index++) {
    ert_sensor_with_data *sensor_with_data = &sensor_module_data->sensor_data_array[sensor_index];
    ert_sensor *sensor = sensor_with_data->sensor;

    ert_logl_info(logger, "  Sensor %d: %s - data available %d:", sensor->id, sensor->name, sensor_with_data->available);

    for (int data_index = 0; data_index < sensor->data_type_count; data_index++) {
      ert_sensor_data_type sensor_data_type = sensor->data_types[data_index];
      ert_sensor_data *sensor_data = &sensor_with_data->data_array[data_index];

      if (sensor_data_type & ERT_SENSOR_TYPE_FLAG_VECTOR3) {
        ert_logl_info(logger, "    Data index %d: (available %d): %s (type 0x%03X) = %f %f %f %s", data_index, sensor_data->available,
            sensor_data->label, sensor_data_type, sensor_data->value.vector3.x, sensor_data->value.vector3.y,
            sensor_data->value.vector3.z, sensor_data->unit);

      } else {
        ert_logl_info(logger, "    Data index %d: (available %d): %s (type 0x%03X) = %f %s", data_index, sensor_data->available,
            sensor_data->label, sensor_data_type, sensor_data->value.value, sensor_data->unit);
      }
    }
  }
}

void ert_data_logger_log_entry_root(ert_log_logger *logger, ert_data_logger_entry *entry)
{
  uint8_t timestamp_string[32];
  ert_format_iso8601_timestamp(&entry->timestamp, 32, timestamp_string);

  ert_logl_info(logger, "Data logger entry:\n"
      "  ID: %d, Device name: %s, Device model: %s, Timestamp: %s",
      entry->entry_id, entry->device_name, entry->device_model, timestamp_string);
}

void ert_data_logger_log_entry(ert_log_logger *logger, ert_data_logger_entry *entry)
{
  ert_data_logger_log_entry_root(logger, entry);

  if (entry->params == NULL) {
    return;
  }

  ert_data_logger_log_gps_data(logger, &entry->params->gps_data);

  for (uint8_t i = 0; i < entry->params->comm_device_status_count; i++) {
    ert_data_logger_log_comm_device_status(logger, &entry->params->comm_device_status[i]);
  }

  for (uint8_t i = 0; i < entry->params->sensor_module_data_count; i++) {
    ert_data_logger_log_sensor_data(logger, entry->params->sensor_module_data[i]);
  }
}