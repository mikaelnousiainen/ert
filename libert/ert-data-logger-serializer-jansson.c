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
#include <jansson.h>
#include <errno.h>
#include <math.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-data-logger-serializer-jansson.h"
#include "ert-jansson-helpers.h"

static const char *gps_data_mode_label[] = {
  "UNKNOWN", "NO_FIX", "2D", "3D"
};

static json_t *serialize_string(const char *value)
{
  if (value == NULL) {
    return json_null();
  }

  return json_string(value);
}

static json_t *serialize_json_real(double value)
{
  // JSON cannot represent NaN/infinity values
  if (isnan(value)) {
    return json_null();
  }
  if (isinf(value)) {
    return json_null();
  }

  return json_real(value);
}

static int serialize_entry_root(ert_data_logger_entry *entry, json_t *entry_obj)
{
  jansson_check_result(json_object_set_new(entry_obj, "type", json_integer(entry->entry_type)));
  jansson_check_result(json_object_set_new(entry_obj, "id", json_integer(entry->entry_id)));

  jansson_check_result(ert_jansson_serialize_timestamp_iso8601_and_millis(entry_obj, "timestamp", &entry->timestamp));

  jansson_check_result(json_object_set_new(entry_obj, "device_name", serialize_string(entry->device_name)));
  jansson_check_result(json_object_set_new(entry_obj, "device_model", serialize_string(entry->device_model)));

  return 0;
}

static int serialize_gps_data(ert_gps_data *gps_data, json_t *gps_obj)
{
  jansson_check_result(json_object_set_new(gps_obj, "has_fix", json_boolean(gps_data->has_fix)));
  jansson_check_result(json_object_set_new(gps_obj, "mode", json_string(gps_data_mode_label[gps_data->mode])));
  jansson_check_result(json_object_set_new(gps_obj, "satellites_visible", json_integer(gps_data->satellites_visible)));
  jansson_check_result(json_object_set_new(gps_obj, "satellites_used", json_integer(gps_data->satellites_used)));
  jansson_check_result(json_object_set_new(gps_obj, "skyview_time_seconds", serialize_json_real(gps_data->skyview_time_seconds)));

  struct timespec gps_timespec;
  ert_gps_time_seconds_to_timespec(gps_data->time_seconds, &gps_timespec);

  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(gps_obj, "time", &gps_timespec));
  jansson_check_result(json_object_set_new(gps_obj, "time_seconds", serialize_json_real(gps_data->time_seconds)));
  jansson_check_result(json_object_set_new(gps_obj, "time_uncertainty_seconds", serialize_json_real(gps_data->time_uncertainty_seconds)));

  jansson_check_result(json_object_set_new(gps_obj, "latitude_degrees", serialize_json_real(gps_data->latitude_degrees)));
  jansson_check_result(json_object_set_new(gps_obj, "latitude_uncertainty_meters", serialize_json_real(gps_data->latitude_uncertainty_meters)));
  jansson_check_result(json_object_set_new(gps_obj, "longitude_degrees", serialize_json_real(gps_data->longitude_degrees)));
  jansson_check_result(json_object_set_new(gps_obj, "longitude_uncertainty_meters", serialize_json_real(gps_data->longitude_uncertainty_meters)));
  jansson_check_result(json_object_set_new(gps_obj, "altitude_meters", serialize_json_real(gps_data->altitude_meters)));
  jansson_check_result(json_object_set_new(gps_obj, "altitude_uncertainty_meters", serialize_json_real(gps_data->altitude_uncertainty_meters)));

  jansson_check_result(json_object_set_new(gps_obj, "track_degrees", serialize_json_real(gps_data->track_degrees)));
  jansson_check_result(json_object_set_new(gps_obj, "track_uncertainty_degrees", serialize_json_real(gps_data->track_uncertainty_degrees)));

  jansson_check_result(json_object_set_new(gps_obj, "speed_meters_per_sec", serialize_json_real(gps_data->speed_meters_per_sec)));
  jansson_check_result(json_object_set_new(gps_obj, "speed_uncertainty_meters_per_sec", serialize_json_real(gps_data->speed_uncertainty_meters_per_sec)));

  jansson_check_result(json_object_set_new(gps_obj, "climb_meters_per_sec", serialize_json_real(gps_data->climb_meters_per_sec)));
  jansson_check_result(json_object_set_new(gps_obj, "climb_uncertainty_meters_per_sec", serialize_json_real(gps_data->climb_uncertainty_meters_per_sec)));

  return 0;
}

static char *serialize_flight_data_state(ert_flight_state state)
{
  switch (state) {
    case ERT_FLIGHT_STATE_IDLE:
      return "IDLE";
    case ERT_FLIGHT_STATE_LAUNCHED:
      return "LAUNCHED";
    case ERT_FLIGHT_STATE_ASCENDING:
      return "ASCENDING";
    case ERT_FLIGHT_STATE_FLOATING:
      return "FLOATING";
    case ERT_FLIGHT_STATE_BURST:
      return "BURST";
    case ERT_FLIGHT_STATE_DESCENDING:
      return "DESCENDING";
    case ERT_FLIGHT_STATE_LANDED:
      return "LANDED";
    case ERT_FLIGHT_STATE_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

static int serialize_flight_data(ert_flight_data *flight_data, json_t *gps_obj)
{
  jansson_check_result(json_object_set_new(gps_obj, "flight_state", json_string(serialize_flight_data_state(flight_data->flight_state))));

  jansson_check_result(json_object_set_new(gps_obj, "minimum_altitude_meters", serialize_json_real(flight_data->minimum_altitude_meters)));
  jansson_check_result(json_object_set_new(gps_obj, "maximum_altitude_meters", serialize_json_real(flight_data->maximum_altitude_meters)));
  jansson_check_result(json_object_set_new(gps_obj, "climb_rate_meters_per_sec", serialize_json_real(flight_data->climb_rate_meters_per_sec)));

  return 0;
}

static int serialize_comm_device_status(ert_comm_device_status *status, json_t *comm_device_obj)
{
  jansson_check_result(json_object_set_new(comm_device_obj, "name", serialize_string(status->name)));
  jansson_check_result(json_object_set_new(comm_device_obj, "model", serialize_string(status->model)));
  jansson_check_result(json_object_set_new(comm_device_obj, "manufacturer", serialize_string(status->manufacturer)));
  jansson_check_result(json_object_set_new(comm_device_obj, "current_rssi", serialize_json_real(status->current_rssi)));
  jansson_check_result(json_object_set_new(comm_device_obj, "last_received_packet_rssi", serialize_json_real(status->last_received_packet_rssi)));

  jansson_check_result(json_object_set_new(comm_device_obj, "transmitted_packet_count", json_integer(status->transmitted_packet_count)));
  jansson_check_result(json_object_set_new(comm_device_obj, "transmitted_bytes", json_integer(status->transmitted_bytes)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_device_obj, "last_transmitted_packet_timestamp", &status->last_transmitted_packet_timestamp));

  jansson_check_result(json_object_set_new(comm_device_obj, "received_packet_count", json_integer(status->received_packet_count)));
  jansson_check_result(json_object_set_new(comm_device_obj, "received_bytes", json_integer(status->received_bytes)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_device_obj, "last_received_packet_timestamp", &status->last_received_packet_timestamp));

  jansson_check_result(json_object_set_new(comm_device_obj, "invalid_received_packet_count", json_integer(status->invalid_received_packet_count)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_device_obj, "last_invalid_received_packet_timestamp", &status->last_invalid_received_packet_timestamp));

  jansson_check_result(json_object_set_new(comm_device_obj, "frequency", serialize_json_real(status->frequency)));
  jansson_check_result(json_object_set_new(comm_device_obj, "frequency_error", serialize_json_real(status->frequency_error)));

  return 0;
}

static int serialize_comm_protocol_status(ert_comm_protocol_status *status, json_t *comm_protocol_obj)
{
  jansson_check_result(json_object_set_new(comm_protocol_obj, "transmitted_packet_count", json_integer(status->transmitted_packet_count)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "transmitted_data_bytes", json_integer(status->transmitted_data_bytes)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "transmitted_payload_data_bytes", json_integer(status->transmitted_payload_data_bytes)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_protocol_obj, "last_transmitted_packet_timestamp", &status->last_transmitted_packet_timestamp));

  jansson_check_result(json_object_set_new(comm_protocol_obj, "duplicate_transmitted_packet_count", json_integer(status->duplicate_transmitted_packet_count)));

  jansson_check_result(json_object_set_new(comm_protocol_obj, "retransmitted_packet_count", json_integer(status->retransmitted_packet_count)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "retransmitted_data_bytes", json_integer(status->retransmitted_data_bytes)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "retransmitted_payload_data_bytes", json_integer(status->retransmitted_payload_data_bytes)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_protocol_obj, "last_retransmitted_packet_timestamp", &status->last_retransmitted_packet_timestamp));

  jansson_check_result(json_object_set_new(comm_protocol_obj, "received_packet_count", json_integer(status->received_packet_count)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "received_data_bytes", json_integer(status->received_data_bytes)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "received_payload_data_bytes", json_integer(status->received_payload_data_bytes)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_protocol_obj, "last_received_packet_timestamp", &status->last_received_packet_timestamp));

  jansson_check_result(json_object_set_new(comm_protocol_obj, "duplicate_received_packet_count", json_integer(status->duplicate_received_packet_count)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "received_packet_sequence_number_error_count", json_integer(status->received_packet_sequence_number_error_count)));
  jansson_check_result(json_object_set_new(comm_protocol_obj, "invalid_received_packet_count", json_integer(status->invalid_received_packet_count)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601(comm_protocol_obj, "last_invalid_received_packet_timestamp", &status->last_invalid_received_packet_timestamp));

  return 0;
}

static int serialize_comm_devices(ert_data_logger_entry *entry, json_t *comm_devices_array)
{
  int result;

  for (int i = 0; i < entry->params->comm_device_status_count; i++) {
    if (entry->params->comm_device_status_present) {
      struct json_t *comm_device_obj = json_object();
      ert_comm_device_status *comm_device_status = &entry->params->comm_device_status[i];

      result = serialize_comm_device_status(comm_device_status, comm_device_obj);
      if (result < 0) {
        ert_log_error("Error serializing comm device status to JSON");
        return result;
      }


      if (entry->params->comm_protocol_status_present) {
        struct json_t *comm_protocol_obj = json_object();
        ert_comm_protocol_status *comm_protocol_status = &entry->params->comm_protocol_status[i];

        result = serialize_comm_protocol_status(comm_protocol_status, comm_protocol_obj);
        if (result < 0) {
          ert_log_error("Error serializing comm protocol status to JSON");
          return result;
        }

        jansson_check_result(json_object_set_new(comm_device_obj, "comm_protocol", comm_protocol_obj));
      }

      jansson_check_result(json_array_append_new(comm_devices_array, comm_device_obj));
    }
  }

  return 0;
}

static int serialize_gsm_modem(ert_driver_gsm_modem_status *status, json_t *gsm_modem_obj)
{
  jansson_check_result(json_object_set_new(gsm_modem_obj, "model", serialize_string(status->model)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "manufacturer", serialize_string(status->manufacturer)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "revision", serialize_string(status->revision)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "serial_number", serialize_string(status->product_serial_number)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "imsi", serialize_string(status->international_mobile_subscriber_identity)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "smsc_number", serialize_string(status->sms_service_center_number)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "subscriber_number", serialize_string(status->subscriber_number)));

  jansson_check_result(json_object_set_new(gsm_modem_obj, "rssi", serialize_json_real(status->rssi)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "bit_error_rate", serialize_json_real(status->bit_error_rate)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "operator_name", serialize_string(status->operator_name)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "network_selection_mode", serialize_json_real(status->network_selection_mode)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "network_registration_mode", serialize_json_real(status->network_registration_mode)));
  jansson_check_result(json_object_set_new(gsm_modem_obj, "network_registration_status", serialize_json_real(status->network_registration_status)));

  return 0;
}

static int serialize_sensor(ert_sensor *sensor, ert_sensor_with_data *sensor_with_data, json_t *sensor_obj)
{
  jansson_check_result(json_object_set_new(sensor_obj, "id", json_integer(sensor->id)));
  jansson_check_result(json_object_set_new(sensor_obj, "name", serialize_string(sensor->name)));
  jansson_check_result(json_object_set_new(sensor_obj, "model", serialize_string(sensor->model)));
  jansson_check_result(json_object_set_new(sensor_obj, "manufacturer", serialize_string(sensor->manufacturer)));
  jansson_check_result(json_object_set_new(sensor_obj, "available", json_boolean(sensor_with_data->available)));

  return 0;
}

static int serialize_sensor_data(ert_sensor_data *sensor_data, ert_sensor_data_type sensor_data_type, json_t *sensor_data_obj)
{
  jansson_check_result(json_object_set_new(sensor_data_obj, "type", json_integer(sensor_data_type)));
  jansson_check_result(json_object_set_new(sensor_data_obj, "label", serialize_string(sensor_data->label)));
  jansson_check_result(json_object_set_new(sensor_data_obj, "unit", serialize_string(sensor_data->unit)));
  jansson_check_result(json_object_set_new(sensor_data_obj, "available", json_boolean(sensor_data->available)));

  if (sensor_data_type & ERT_SENSOR_TYPE_FLAG_VECTOR3) {
    jansson_check_result(json_object_set_new(sensor_data_obj, "x", serialize_json_real(sensor_data->value.vector3.x)));
    jansson_check_result(json_object_set_new(sensor_data_obj, "y", serialize_json_real(sensor_data->value.vector3.y)));
    jansson_check_result(json_object_set_new(sensor_data_obj, "z", serialize_json_real(sensor_data->value.vector3.z)));
  } else {
    jansson_check_result(json_object_set_new(sensor_data_obj, "value", serialize_json_real(sensor_data->value.value)));
  }

  return 0;
}

static int serialize_sensor_modules(ert_data_logger_entry *entry, json_t *sensor_modules_array)
{
  int result;

  for (int i = 0; i < entry->params->sensor_module_data_count; i++) {
    struct json_t *sensor_module_obj = json_object();
    ert_sensor_module_data *module_data = entry->params->sensor_module_data[i];

    jansson_check_result(json_object_set_new(sensor_module_obj, "name", serialize_string(module_data->module->name)));

    struct json_t *sensors_array = json_array();
    for (int j = 0; j < module_data->module->sensor_count; j++) {
      struct json_t *sensor_obj = json_object();
      ert_sensor_with_data *sensor_with_data = &module_data->sensor_data_array[j];
      ert_sensor *sensor = sensor_with_data->sensor;

      result = serialize_sensor(sensor, sensor_with_data, sensor_obj);
      if (result < 0) {
        ert_log_error("Error serializing sensor info to JSON");
        return result;
      }

      struct json_t *sensor_data_array = json_array();
      for (int k = 0; k < sensor->data_type_count; k++) {
        struct json_t *sensor_data_obj = json_object();
        ert_sensor_data_type sensor_data_type = sensor->data_types[k];
        ert_sensor_data *sensor_data = &sensor_with_data->data_array[k];

        result = serialize_sensor_data(sensor_data, sensor_data_type, sensor_data_obj);
        if (result < 0) {
          ert_log_error("Error serializing sensor data to JSON");
          return result;
        }

        jansson_check_result(json_array_append_new(sensor_data_array, sensor_data_obj));
      }

      jansson_check_result(json_object_set_new(sensor_obj, "values", sensor_data_array));

      jansson_check_result(json_array_append_new(sensors_array, sensor_obj));
    }

    jansson_check_result(json_object_set_new(sensor_module_obj, "sensors", sensors_array));
    jansson_check_result(json_array_append_new(sensor_modules_array, sensor_module_obj));
  }

  return 0;
}

static int serialize_entry(ert_data_logger_entry *entry, json_t *entry_obj)
{
  int result = serialize_entry_root(entry, entry_obj);
  if (result < 0) {
    ert_log_error("Error serializing data logger entry root to JSON");
    return result;
  }

  if (entry->params->gps_data_present) {
    struct json_t *gps_obj = json_object();
    ert_gps_data *gps_data = &entry->params->gps_data;

    result = serialize_gps_data(gps_data, gps_obj);
    if (result < 0) {
      ert_log_error("Error serializing GPS data to JSON");
      return result;
    }

    jansson_check_result(json_object_set_new(entry_obj, "gps", gps_obj));
  }

  if (entry->params->flight_data_present) {
    struct json_t *flight_obj = json_object();

    result = serialize_flight_data(&entry->params->flight_data, flight_obj);
    if (result < 0) {
      ert_log_error("Error serializing flight data to JSON");
      return result;
    }

    jansson_check_result(json_object_set_new(entry_obj, "flight", flight_obj));
  }

  struct json_t *comm_devices_array = json_array();
  result = serialize_comm_devices(entry, comm_devices_array);
  if (result < 0) {
    ert_log_error("Error serializing comm devices");
    return result;
  }

  jansson_check_result(json_object_set_new(entry_obj, "comm_devices", comm_devices_array));

  if (entry->params->gsm_modem_status_present) {
    struct json_t *gsm_modem_obj = json_object();

    result = serialize_gsm_modem(&entry->params->gsm_modem_status, gsm_modem_obj);
    if (result < 0) {
      ert_log_error("Error serializing GSM modem status to JSON");
      return result;
    }

    jansson_check_result(json_object_set_new(entry_obj, "gsm_modem", gsm_modem_obj));
  }

  struct json_t *sensor_modules_array = json_array();
  result = serialize_sensor_modules(entry, sensor_modules_array);
  if (result < 0) {
    ert_log_error("Error serializing sensor modules");
    return result;
  }

  jansson_check_result(json_object_set_new(entry_obj, "sensor_modules", sensor_modules_array));

  return 0;
}

static int serialize_related_entry(ert_data_logger_entry_related *related_entry, json_t *related_obj)
{
  json_t *entry_obj = json_object();

  int result = serialize_entry(related_entry->entry, entry_obj);
  if (result < 0) {
    ert_log_error("Error serializing related entry");
    return result;
  }

  jansson_check_result(json_object_set_new(related_obj, "entry", entry_obj));

  jansson_check_result(json_object_set_new(related_obj, "distance_meters", serialize_json_real(related_entry->distance_meters)));
  jansson_check_result(json_object_set_new(related_obj, "altitude_diff_meters", serialize_json_real(related_entry->altitude_diff_meters)));
  jansson_check_result(json_object_set_new(related_obj, "azimuth_degrees", serialize_json_real(related_entry->azimuth_degrees)));
  jansson_check_result(json_object_set_new(related_obj, "elevation_degrees", serialize_json_real(related_entry->elevation_degrees)));

  return 0;
}

static int serialize_related_entries(uint8_t related_entry_count, ert_data_logger_entry_related *related_entries[], json_t *related_array)
{
  int result;

  for (uint8_t i = 0; i < related_entry_count; i++) {
    ert_data_logger_entry_related *related_entry = related_entries[i];

    json_t *related_obj = json_object();
    result = serialize_related_entry(related_entry, related_obj);
    if (result < 0) {
      return result;
    }

    jansson_check_result(json_array_append_new(related_array, related_obj));
  }

  return 0;
}

int ert_data_logger_serializer_jansson_serialize(ert_data_logger_serializer *serializer,
    ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv)
{
  json_t *entry_obj = json_object();
  int result = serialize_entry(entry, entry_obj);
  if (result < 0) {
    json_decref(entry_obj);
    return -EIO;
  }

  json_t *related_array = json_array();

  result = serialize_related_entries(entry->params->related_entry_count, entry->params->related, related_array);
  if (result < 0) {
    ert_log_error("Error serializing related entries, result %d", result);
    json_decref(related_array);
    json_decref(entry_obj);
    return -EIO;
  }

  jansson_check_result(json_object_set_new(entry_obj, "related", related_array));

  char *data = json_dumps(entry_obj, JSON_COMPACT | JSON_PRESERVE_ORDER);
  json_decref(entry_obj);

  if (data == NULL) {
    ert_log_error("Error serializing JSON to string");
    return -EIO;
  }

  *length = (uint32_t) strlen(data);

  *data_rcv = (uint8_t *) data;

  return 0;
}

int ert_data_logger_serializer_jansson_create(ert_data_logger_serializer **serializer_rcv)
{
  ert_data_logger_serializer *serializer = calloc(1, sizeof(ert_data_logger_serializer));
  if (serializer == NULL) {
    ert_log_fatal("Error allocating memory for serializer struct: %s", strerror(errno));
    return -ENOMEM;
  }

  serializer->serialize = ert_data_logger_serializer_jansson_serialize;
  serializer->deserialize = NULL;
  serializer->priv = NULL;

  *serializer_rcv = serializer;

  return 0;
}

int ert_data_logger_serializer_jansson_destroy(ert_data_logger_serializer *serializer)
{
  free(serializer);

  return 0;
}
