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
#include <memory.h>
#include <msgpack.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-data-logger-serializer-msgpack.h"
#include "ert-msgpack-helpers.h"

ert_data_logger_serializer_msgpack_settings msgpack_serializer_settings[] = {
    {
        .include_comm = false,
        .include_modem = false,
        .include_text = false,
        .include_sensor_data_types_count = 2,
        .include_sensor_data_types = {
            ERT_SENSOR_TYPE_TEMPERATURE,
            ERT_SENSOR_TYPE_PRESSURE
        }
    },
    {
        .include_comm = false,
        .include_modem = false,
        .include_text = false,
        .include_sensor_data_types_count = 8,
        .include_sensor_data_types = {
            ERT_SENSOR_TYPE_TEMPERATURE,
            ERT_SENSOR_TYPE_HUMIDITY,
            ERT_SENSOR_TYPE_PRESSURE,
            ERT_SENSOR_TYPE_ORIENTATION,
            ERT_SENSOR_TYPE_ACCELEROMETER,
            ERT_SENSOR_TYPE_SYSTEM_UPTIME,
            ERT_SENSOR_TYPE_SYSTEM_LOAD_AVG,
            ERT_SENSOR_TYPE_SYSTEM_MEM_USED
        }
    },
    {
        .include_comm = false,
        .include_modem = false,
        .include_text = false,
        .include_sensor_data_types_count = ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSOR_DATA_TYPES,
        .include_sensor_data_types = { ERT_SENSOR_TYPE_UNKNOWN }
    },
    {
        .include_comm = true,
        .include_modem = true,
        .include_text = false,
        .include_sensor_data_types_count = ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSOR_DATA_TYPES,
        .include_sensor_data_types = { ERT_SENSOR_TYPE_UNKNOWN }
    },
    {
        .include_comm = true,
        .include_modem = true,
        .include_text = true,
        .include_sensor_data_types_count = ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSOR_DATA_TYPES,
        .include_sensor_data_types = { ERT_SENSOR_TYPE_UNKNOWN }
    },
};


static int serialize_entry_root(msgpack_packer *pk, ert_data_logger_entry *entry)
{
  msgpack_pack_map(pk, 4);

  ert_msgpack_serialize_map_key(pk, "i");
  msgpack_pack_uint32(pk, entry->entry_id);

  ert_msgpack_serialize_map_key(pk, "e");
  msgpack_pack_uint8(pk, entry->entry_type);

  ert_msgpack_serialize_map_key(pk, "t");
  msgpack_pack_uint64(pk,
      ((uint64_t) entry->timestamp.tv_sec) * 1000LL + ((uint64_t) entry->timestamp.tv_nsec) / 1000000LL);

  ert_msgpack_serialize_map_key_string(pk, "n", entry->device_name);

  return 0;
}

static int serialize_gps_data(msgpack_packer *pk, ert_gps_data *gps_data)
{
  msgpack_pack_map(pk, 17);

  ert_msgpack_serialize_map_key(pk, "m");
  msgpack_pack_int8(pk, gps_data->mode);

  ert_msgpack_serialize_map_key(pk, "sv");
  msgpack_pack_int8(pk, gps_data->satellites_visible);

  ert_msgpack_serialize_map_key(pk, "su");
  msgpack_pack_int8(pk, gps_data->satellites_used);

  ert_msgpack_serialize_map_key(pk, "t");
  msgpack_pack_double(pk, gps_data->time_seconds);
  ert_msgpack_serialize_map_key(pk, "tu");
  msgpack_pack_float(pk, (float) gps_data->time_uncertainty_seconds);

  ert_msgpack_serialize_map_key(pk, "la");
  msgpack_pack_float(pk, (float) gps_data->latitude_degrees);
  ert_msgpack_serialize_map_key(pk, "lau");
  msgpack_pack_float(pk, (float) gps_data->latitude_uncertainty_meters);
  ert_msgpack_serialize_map_key(pk, "lo");
  msgpack_pack_float(pk, (float) gps_data->longitude_degrees);
  ert_msgpack_serialize_map_key(pk, "lou");
  msgpack_pack_float(pk, (float) gps_data->longitude_uncertainty_meters);
  ert_msgpack_serialize_map_key(pk, "al");
  msgpack_pack_float(pk, (float) gps_data->altitude_meters);
  ert_msgpack_serialize_map_key(pk, "alu");
  msgpack_pack_float(pk, (float) gps_data->altitude_uncertainty_meters);

  ert_msgpack_serialize_map_key(pk, "tr");
  msgpack_pack_float(pk, (float) gps_data->track);
  ert_msgpack_serialize_map_key(pk, "tru");
  msgpack_pack_float(pk, (float) gps_data->track_uncertainty_degrees);

  ert_msgpack_serialize_map_key(pk, "sp");
  msgpack_pack_float(pk, (float) gps_data->speed_meters_per_sec);
  ert_msgpack_serialize_map_key(pk, "spu");
  msgpack_pack_float(pk, (float) gps_data->speed_uncertainty_meters_per_sec);

  ert_msgpack_serialize_map_key(pk, "c");
  msgpack_pack_float(pk, (float) gps_data->climb_meters_per_sec);
  ert_msgpack_serialize_map_key(pk, "cu");
  msgpack_pack_float(pk, (float) gps_data->climb_uncertainty_meters_per_sec);

  return 0;
}

static int serialize_comm_device(msgpack_packer *pk, ert_comm_device_status *status)
{
  msgpack_pack_map(pk, 9);

  ert_msgpack_serialize_map_key_string(pk, "n", status->name);

  ert_msgpack_serialize_map_key(pk, "st");
  msgpack_pack_uint8(pk, status->device_state);

  ert_msgpack_serialize_map_key(pk, "s");
  msgpack_pack_int16(pk, (int16_t) (status->current_rssi * 10.0));

  ert_msgpack_serialize_map_key(pk, "ps");
  msgpack_pack_int16(pk, (int16_t) (status->last_received_packet_rssi * 10.0));

  ert_msgpack_serialize_map_key(pk, "tp");
  msgpack_pack_uint32(pk, (uint32_t) status->transmitted_packet_count);

  ert_msgpack_serialize_map_key(pk, "rp");
  msgpack_pack_uint32(pk, (uint32_t) status->received_packet_count);

  ert_msgpack_serialize_map_key(pk, "ip");
  msgpack_pack_uint32(pk, (uint32_t) status->invalid_received_packet_count);

  ert_msgpack_serialize_map_key(pk, "f");
  msgpack_pack_float(pk, (float) status->frequency);

  ert_msgpack_serialize_map_key(pk, "e");
  msgpack_pack_float(pk, (float) status->frequency_error);

  return 0;
}

static int serialize_gsm_modem(msgpack_packer *pk, ert_driver_gsm_modem_status *status)
{
  msgpack_pack_map(pk, 3);

  ert_msgpack_serialize_map_key_string(pk, "o", status->operator_name);

  ert_msgpack_serialize_map_key(pk, "s");
  msgpack_pack_int16(pk, (int16_t) (status->rssi));

  ert_msgpack_serialize_map_key(pk, "r");
  msgpack_pack_uint8(pk, (uint8_t) (status->network_registration_status));

  return 0;
}

static int serialize_sensor_data(msgpack_packer *pk,
    ert_data_logger_serializer_msgpack_settings *settings,
    uint8_t module_index, uint8_t sensor_index, ert_sensor *sensor,
    ert_sensor_data_type sensor_data_type, ert_sensor_data *sensor_data)
{
  size_t field_count = 2;

  if (sensor_data->available) {
    if (sensor_data_type & ERT_SENSOR_TYPE_FLAG_VECTOR3) {
      field_count += 3;
    } else {
      field_count += 1;
    }
  }

  if (settings->include_text) {
    field_count += 2;
  }

  msgpack_pack_map(pk, field_count);

  ert_msgpack_serialize_map_key(pk, "i");
  msgpack_pack_uint32(pk, (uint32_t) sensor->id | (sensor_index & 0xFF) << 16 | (module_index & 0xFF) << 24);

  ert_msgpack_serialize_map_key(pk, "t");
  msgpack_pack_uint16(pk, (uint16_t) sensor_data_type);

  if (settings->include_text) {
    ert_msgpack_serialize_map_key_string(pk, "l", sensor_data->label);
    ert_msgpack_serialize_map_key_string(pk, "u", sensor_data->unit);
  }

  if (sensor_data_type & ERT_SENSOR_TYPE_FLAG_VECTOR3) {
    ert_msgpack_serialize_map_key(pk, "x");
    msgpack_pack_float(pk, (float) sensor_data->value.vector3.x);
    ert_msgpack_serialize_map_key(pk, "y");
    msgpack_pack_float(pk, (float) sensor_data->value.vector3.y);
    ert_msgpack_serialize_map_key(pk, "z");
    msgpack_pack_float(pk, (float) sensor_data->value.vector3.z);
  } else {
    ert_msgpack_serialize_map_key(pk, "v");
    msgpack_pack_float(pk, (float) sensor_data->value.value);
  }

  return 0;
}

static bool is_sensor_data_type_included(int8_t include_sensor_data_types_count, ert_sensor_data_type *include_sensor_data_types,
  ert_sensor_data_type data_type)
{
  if (include_sensor_data_types_count == ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSOR_DATA_TYPES) {
    return true;
  }

  for (int8_t i = 0; i < include_sensor_data_types_count; i++) {
    if (include_sensor_data_types[i] == data_type) {
      return true;
    }
  }

  return false;
}

int ert_data_logger_serializer_msgpack_serialize(ert_data_logger_serializer *serializer,
    ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv)
{
  ert_data_logger_serializer_msgpack_settings *settings =
      (ert_data_logger_serializer_msgpack_settings *) serializer->priv;

  return ert_data_logger_serializer_msgpack_serialize_with_settings(serializer, settings, entry, length, data_rcv);
}

int ert_data_logger_serializer_msgpack_serialize_with_settings(ert_data_logger_serializer *serializer,
    ert_data_logger_serializer_msgpack_settings *settings, ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv)
{
  int result;

  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);

  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  size_t entry_field_count = 2;
  bool include_comm = settings->include_comm;
  bool include_modem = settings->include_modem && entry->params->gsm_modem_status_present;
  bool include_gps = entry->params->gps_data_present;

  if (include_comm) {
    entry_field_count++;
  }
  if (include_modem) {
    entry_field_count++;
  }
  if (include_gps) {
    entry_field_count++;
  }

  msgpack_pack_map(&pk, entry_field_count);

  ert_msgpack_serialize_map_key(&pk, "e");
  result = serialize_entry_root(&pk, entry);
  if (result < 0) {
    ert_log_error("Error serializing data logger entry root, result %d", result);
    return result;
  }

  if (include_gps) {
    ert_msgpack_serialize_map_key(&pk, "g");
    result = serialize_gps_data(&pk, &entry->params->gps_data);
    if (result < 0) {
      ert_log_error("Error serializing GPS data, result %d", result);
      return result;
    }
  }

  if (include_comm) {
    ert_msgpack_serialize_map_key(&pk, "c");

    msgpack_pack_array(&pk, entry->params->comm_device_status_count);
    for (uint8_t i = 0; i < entry->params->comm_device_status_count; i++) {
      result = serialize_comm_device(&pk, &entry->params->comm_device_status[i]);
      if (result < 0) {
        ert_log_error("Error serializing comm device status, result %d", result);
        return result;
      }
    }
  }

  if (include_modem) {
    ert_msgpack_serialize_map_key(&pk, "m");

    result = serialize_gsm_modem(&pk, &entry->params->gsm_modem_status);
    if (result < 0) {
      ert_log_error("Error serializing GSM modem status, result %d", result);
    }
  }

  size_t sensor_data_count = 0;

  for (uint8_t i = 0; i < entry->params->sensor_module_data_count; i++) {
    ert_sensor_module_data *module_data = entry->params->sensor_module_data[i];

    for (uint8_t j = 0; j < module_data->module->sensor_count; j++) {
      ert_sensor_with_data *sensor_with_data = &module_data->sensor_data_array[j];
      ert_sensor *sensor = sensor_with_data->sensor;

      for (uint8_t k = 0; k < sensor->data_type_count; k++) {
        ert_sensor_data_type sensor_data_type = sensor->data_types[k];

        if (!is_sensor_data_type_included(settings->include_sensor_data_types_count,
            settings->include_sensor_data_types, sensor_data_type)) {
          continue;
        }
        sensor_data_count++;
      }
    }
  }

  ert_msgpack_serialize_map_key(&pk, "s");
  msgpack_pack_array(&pk, sensor_data_count);

  for (uint8_t i = 0; i < entry->params->sensor_module_data_count; i++) {
    ert_sensor_module_data *module_data = entry->params->sensor_module_data[i];

    for (uint8_t j = 0; j < module_data->module->sensor_count; j++) {
      ert_sensor_with_data *sensor_with_data = &module_data->sensor_data_array[j];
      ert_sensor *sensor = sensor_with_data->sensor;

      for (uint8_t k = 0; k < sensor->data_type_count; k++) {
        ert_sensor_data_type sensor_data_type = sensor->data_types[k];
        ert_sensor_data *sensor_data = &sensor_with_data->data_array[k];

        if (!is_sensor_data_type_included(settings->include_sensor_data_types_count,
            settings->include_sensor_data_types, sensor_data_type)) {
          continue;
        }

        result = serialize_sensor_data(&pk, settings, i, j, sensor, sensor_data_type, sensor_data);
        if (result < 0) {
          ert_log_error("Error serializing sensor data, result %d", result);
          return result;
        }
      }
    }
  }

  uint8_t *data = calloc(1, sbuf.size);
  if (data == NULL) {
    msgpack_sbuffer_destroy(&sbuf);
    ert_log_fatal("Error allocating memory for serialized MsgPack data: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(data, sbuf.data, sbuf.size);

  *length = (uint32_t) sbuf.size;
  *data_rcv = data;

  msgpack_sbuffer_destroy(&sbuf);

  return 0;
}

static int deserialize_entry_root(ert_data_logger_entry *entry, msgpack_object *obj)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Data logger entry: entry root data object is not a map");
    return -1;
  }

  msgpack_object_map *map_obj = &obj->via.map;

  int result;

  for (uint32_t i = 0; i < map_obj->size; i++) {
    msgpack_object_kv *kv_obj = &map_obj->ptr[i];

    if (ert_msgpack_string_equals("i", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'e.i' object is not an integer");
        return -1;
      }

      entry->entry_id = (uint32_t) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("e", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'e.e' object is not an integer");
        return -1;
      }

      entry->entry_type = (uint8_t) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("t", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'e.t' object is not an integer");
        return -1;
      }

      entry->timestamp.tv_sec = kv_obj->val.via.u64 / 1000LL;
      entry->timestamp.tv_nsec = (kv_obj->val.via.u64 % 1000LL) * 1000000LL;
    }

    if (ert_msgpack_string_equals("n", &kv_obj->key.via.str)) {
      result = ert_msgpack_copy_string_object_alloc((char **) &entry->device_name, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Data logger entry 'e.n' object is invalid");
        return -1;
      }
    }
  }

  return 0;
}

static int deserialize_gps_data(ert_gps_data *gps_data, msgpack_object *obj)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Data logger entry: GPS data object is not a map");
    return -1;
  }

  msgpack_object_map *map_obj = &obj->via.map;

  for (uint32_t i = 0; i < map_obj->size; i++) {
    msgpack_object_kv *kv_obj = &map_obj->ptr[i];

    if (ert_msgpack_string_equals("m", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'g.m' object is not an integer");
        return -1;
      }

      gps_data->mode = (ert_gps_fix_mode) kv_obj->val.via.u64;
      gps_data->has_fix = ((gps_data->mode == ERT_GPS_MODE_FIX_2D)
          || (gps_data->mode == ERT_GPS_MODE_FIX_3D)) ? true : false;
    }

    if (ert_msgpack_string_equals("sv", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'g.sv' object is not an integer");
        return -1;
      }

      gps_data->satellites_visible = (int) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("su", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'g.su' object is not an integer");
        return -1;
      }

      gps_data->satellites_used = (int) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("t", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT64) {
        ert_log_error("Data logger entry 'g.t' object is not a float");
        return -1;
      }

      gps_data->time_seconds = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("tu", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.tu' object is not a float");
        return -1;
      }

      gps_data->time_uncertainty_seconds = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("la", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.la' object is not a float");
        return -1;
      }

      gps_data->latitude_degrees = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("lau", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.lau' object is not a float");
        return -1;
      }

      gps_data->latitude_uncertainty_meters = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("lo", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.lo' object is not a float");
        return -1;
      }

      gps_data->longitude_degrees = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("lou", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.lou' object is not a float");
        return -1;
      }

      gps_data->longitude_uncertainty_meters = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("al", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.al' object is not a float");
        return -1;
      }

      gps_data->altitude_meters = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("alu", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.alu' object is not a float");
        return -1;
      }

      gps_data->altitude_uncertainty_meters = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("tr", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.tr' object is not a float");
        return -1;
      }

      gps_data->track = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("tru", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.tru' object is not a float");
        return -1;
      }

      gps_data->track_uncertainty_degrees = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("sp", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.sp' object is not a float");
        return -1;
      }

      gps_data->speed_meters_per_sec = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("spu", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.spu' object is not a float");
        return -1;
      }

      gps_data->speed_uncertainty_meters_per_sec = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("c", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.c' object is not a float");
        return -1;
      }

      gps_data->climb_meters_per_sec = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("cu", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'g.cu' object is not a float");
        return -1;
      }

      gps_data->climb_uncertainty_meters_per_sec = kv_obj->val.via.f64;
    }
  }

  return 0;
}

static int deserialize_comm_device(ert_comm_device_status *status, msgpack_object *obj)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Data logger entry: comm device status object is not a map");
    return -1;
  }

  msgpack_object_map *map_obj = &obj->via.map;

  for (uint32_t i = 0; i < map_obj->size; i++) {
    msgpack_object_kv *kv_obj = &map_obj->ptr[i];

    if (ert_msgpack_string_equals("st", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'c.st' object is not an integer");
        return -1;
      }

      status->device_state = (uint8_t) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("s", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER && kv_obj->val.type != MSGPACK_OBJECT_NEGATIVE_INTEGER) {
        ert_log_error("Data logger entry 'c.s' object is not an integer");
        return -1;
      }

      status->current_rssi = (float) kv_obj->val.via.i64 / 10.0f;
    }

    if (ert_msgpack_string_equals("ps", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER && kv_obj->val.type != MSGPACK_OBJECT_NEGATIVE_INTEGER) {
        ert_log_error("Data logger entry 'c.ps' object is not an integer");
        return -1;
      }

      status->last_received_packet_rssi = (float) kv_obj->val.via.i64 / 10.0f;
    }

    if (ert_msgpack_string_equals("tp", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'c.tp' object is not an integer");
        return -1;
      }

      status->transmitted_packet_count = kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("rp", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'c.rp' object is not an integer");
        return -1;
      }

      status->received_packet_count = kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("ip", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 'c.ip' object is not an integer");
        return -1;
      }

      status->invalid_received_packet_count = kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("f", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'c.f' object is not a float");
        return -1;
      }

      status->frequency = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("e", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 'c.e' object is not a float");
        return -1;
      }

      status->frequency_error = kv_obj->val.via.f64;
    }
  }

  return 0;
}

static int deserialize_gsm_modem(ert_driver_gsm_modem_status *status, msgpack_object *obj)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Data logger entry: gsm modem status object is not a map");
    return -1;
  }

  msgpack_object_map *map_obj = &obj->via.map;
  int result;

  for (uint32_t i = 0; i < map_obj->size; i++) {
    msgpack_object_kv *kv_obj = &map_obj->ptr[i];

    if (ert_msgpack_string_equals("o", &kv_obj->key.via.str)) {
      result = ert_msgpack_copy_string_object(status->operator_name, 64, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Data logger entry 'm.o' object is invalid");
        return -1;
      }
    }

    if (ert_msgpack_string_equals("s", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER && kv_obj->val.type != MSGPACK_OBJECT_NEGATIVE_INTEGER) {
        ert_log_error("Data logger entry 'm.s' object is not an integer");
        return -1;
      }

      status->rssi = (int32_t) kv_obj->val.via.i64;
    }

    if (ert_msgpack_string_equals("r", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER && kv_obj->val.type != MSGPACK_OBJECT_NEGATIVE_INTEGER) {
        ert_log_error("Data logger entry 'm.r' object is not an integer");
        return -1;
      }

      status->network_registration_status = (ert_driver_gsm_modem_network_registration_report_status) kv_obj->val.via.i64;
    }
  }

  return 0;
}

static int deserialize_sensor_data(ert_data_logger_entry_params *params, msgpack_object *obj)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Data logger entry: sensor data object is not a map");
    return -1;
  }

  msgpack_object_map *map_obj = &obj->via.map;

  ert_sensor_data sensor_data = {0};

  uint32_t sensor_identifier = 0;
  int16_t module_index = -1;
  int16_t sensor_index = -1;
  int16_t sensor_id = -1;

  ert_sensor_data_type sensor_data_type = ERT_SENSOR_TYPE_UNKNOWN;

  int result;

  for (uint32_t i = 0; i < map_obj->size; i++) {
    msgpack_object_kv *kv_obj = &map_obj->ptr[i];

    if (ert_msgpack_string_equals("i", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 's.i' object is not an integer");
        return -1;
      }

      sensor_identifier = (uint32_t) kv_obj->val.via.u64;
      sensor_id = (uint16_t) (kv_obj->val.via.u64 & 0xFFFF);
      sensor_index = (uint8_t) (kv_obj->val.via.u64 >> 16) & 0xFF;
      module_index = (uint8_t) (kv_obj->val.via.u64 >> 24) & 0xFF;

      ert_log_debug("Found sensor id %d, sensor index %d, module index %d", sensor_id, sensor_index, module_index);
    }

    if (ert_msgpack_string_equals("t", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Data logger entry 's.t' object is not an integer");
        return -1;
      }

      sensor_data_type = (ert_sensor_data_type) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("l", &kv_obj->key.via.str)) {
      result = ert_msgpack_copy_string_object_alloc((char **) &sensor_data.label, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Data logger entry 's.l' object is invalid");
        return -1;
      }
    }

    if (ert_msgpack_string_equals("u", &kv_obj->key.via.str)) {
      result = ert_msgpack_copy_string_object_alloc((char **) &sensor_data.unit, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Data logger entry 's.u' object is invalid");
        return -1;
      }
    }

    if (ert_msgpack_string_equals("v", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 's.v' object is not a float");
        return -1;
      }

      sensor_data.available = true;
      sensor_data.value.value = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("x", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 's.x' object is not a float");
        return -1;
      }

      sensor_data.available = true;
      sensor_data.value.vector3.x = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("y", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 's.y' object is not a float");
        return -1;
      }

      sensor_data.available = true;
      sensor_data.value.vector3.y = kv_obj->val.via.f64;
    }

    if (ert_msgpack_string_equals("z", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_FLOAT32) {
        ert_log_error("Data logger entry 's.z' object is not a float");
        return -1;
      }

      sensor_data.available = true;
      sensor_data.value.vector3.z = kv_obj->val.via.f64;
    }
  }

  if (sensor_id < 0 || module_index < 0 || sensor_index < 0) {
    ert_log_error("Sensor ID, module index or sensor index invalid in sensor identifier: 0x%08X", sensor_identifier);
    if (sensor_data.label != NULL) {
      free((void *) sensor_data.label);
    }
    if (sensor_data.unit != NULL) {
      free((void *) sensor_data.unit);
    }
    return -1;
  }

  if (params->sensor_module_data_count < (module_index + 1)) {
    for (int16_t i = 0; i < (module_index + 1); i++) {
      if (params->sensor_module_data[i] != NULL) {
        continue;
      }

      params->sensor_module_data[i] = calloc(1, sizeof(ert_sensor_module_data));
      if (params->sensor_module_data[i] == NULL) {
        ert_log_fatal("Error allocating memory for data logger entry sensor module data struct: %s", strerror(errno));
        return -ENOMEM;
      }
    }

    params->sensor_module_data_count = (uint8_t) (module_index + 1);
  }

  ert_sensor_module_data *sensor_module_data = params->sensor_module_data[module_index];

  if (sensor_module_data->module == NULL) {
    sensor_module_data->module = calloc(1, sizeof(ert_sensor_module));
    if (sensor_module_data->module == NULL) {
      ert_log_fatal("Error allocating memory for data logger entry sensor module struct: %s", strerror(errno));
      return -ENOMEM;
    }

    sensor_module_data->module->sensors = calloc(64, sizeof(ert_sensor *));
    if (sensor_module_data->module == NULL) {
      ert_log_fatal("Error allocating memory for data logger entry sensor array: %s", strerror(errno));
      return -ENOMEM;
    }

    // TODO: set module name
  }

  if (sensor_module_data->module->sensor_count < (sensor_index + 1)) {
    for (int16_t i = 0; i < (sensor_index + 1); i++) {
      if (sensor_module_data->module->sensors[i] != NULL) {
        continue;
      }

      sensor_module_data->module->sensors[i] = calloc(1, sizeof(ert_sensor));
      if (sensor_module_data->module->sensors[i] == NULL) {
        ert_log_fatal("Error allocating memory for data logger entry sensor struct: %s", strerror(errno));
        return -ENOMEM;
      }
    }

    sensor_module_data->sensor_data_array = realloc(sensor_module_data->sensor_data_array,
        (sensor_index + 1) * sizeof(ert_sensor_with_data));
    if (sensor_module_data->sensor_data_array == NULL) {
      ert_log_fatal("Error allocating memory for data logger entry sensor with data array: %s", strerror(errno));
      return -ENOMEM;
    }
    memset(sensor_module_data->sensor_data_array + sensor_module_data->module->sensor_count, 0,
        (sensor_index + 1 - sensor_module_data->module->sensor_count) * sizeof(ert_sensor_with_data));

    sensor_module_data->module->sensor_count = (uint8_t) (sensor_index + 1);
  }

  ert_sensor *sensor = sensor_module_data->module->sensors[sensor_index];
  if (sensor->name == NULL) {
    // TODO: set sensor name
  }
  if (sensor->data_types == NULL) {
    // NOTE: hard-coded limit of 32 sensor data types
    sensor->data_types = calloc(32, sizeof(ert_sensor_data_type));
    if (sensor->data_types == NULL) {
      ert_log_fatal("Error allocating memory for data logger entry sensor data types: %s", strerror(errno));
      return -ENOMEM;
    }
  }

  sensor->id = (uint32_t) sensor_id;

  sensor->data_types[sensor->data_type_count] = sensor_data_type;

  sensor_module_data->sensor_data_array[sensor_index].available = true;
  sensor_module_data->sensor_data_array[sensor_index].sensor = sensor;

  sensor_module_data->sensor_data_array[sensor_index].data_array = realloc(sensor_module_data->sensor_data_array[sensor_index].data_array,
      (sensor->data_type_count + 1) * sizeof(ert_sensor_data));
  if (sensor_module_data->sensor_data_array[sensor_index].data_array == NULL) {
    ert_log_fatal("Error allocating memory for data logger entry sensor data array: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(&sensor_module_data->sensor_data_array[sensor_index].data_array[sensor->data_type_count],
      &sensor_data, sizeof(ert_sensor_data));

  sensor->data_type_count++;

  return 0;
}

int ert_data_logger_serializer_msgpack_destroy_entry(ert_data_logger_serializer *serializer,
    ert_data_logger_entry *entry) {

  return ert_data_logger_destroy_entry(entry);
}

int ert_data_logger_serializer_msgpack_deserialize(ert_data_logger_serializer *serializer,
    uint32_t length, uint8_t *data, ert_data_logger_entry **entry_rcv)
{
  int result = 0;

  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 1024);

  msgpack_object root_obj;
  msgpack_unpack_return unpack_result = msgpack_unpack((const char *) data, length, NULL, &mempool, &root_obj);

  if (unpack_result != MSGPACK_UNPACK_SUCCESS && unpack_result != MSGPACK_UNPACK_EXTRA_BYTES) {
    ert_log_error("Invalid MsgPack data, result %d", unpack_result);
    result = -EINVAL;
    goto error_msgpack;
  }

  if (root_obj.type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Data logger entry root object is not a map");
    result = -EINVAL;
    goto error_msgpack;
  }

  ert_data_logger_entry *entry = calloc(1, sizeof(ert_data_logger_entry));
  if (entry == NULL) {
    ert_log_fatal("Error allocating memory for data logger entry struct: %s", strerror(errno));
    return -ENOMEM;
  }

  entry->params = calloc(1, sizeof(ert_data_logger_entry_params));
  if (entry->params == NULL) {
    ert_log_fatal("Error allocating memory for data logger entry params struct: %s", strerror(errno));
    return -ENOMEM;
  }

  for (uint32_t i = 0; i < root_obj.via.map.size; i++) {
    msgpack_object_kv *kv_obj = &root_obj.via.map.ptr[i];

    if (ert_msgpack_string_equals("e", &kv_obj->key.via.str)) {
      result = deserialize_entry_root(entry, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Error deserializing entry root, result %d", result);
        goto error_entry;
      }
    }

    if (ert_msgpack_string_equals("g", &kv_obj->key.via.str)) {
      result = deserialize_gps_data(&entry->params->gps_data, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Error deserializing GPS data, result %d", result);
        goto error_entry;
      }
      entry->params->gps_data_present = true;
    }

    if (ert_msgpack_string_equals("m", &kv_obj->key.via.str)) {
      result = deserialize_gsm_modem(&entry->params->gsm_modem_status, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Error deserializing GSM modem status, result %d", result);
        goto error_entry;
      }
      entry->params->gsm_modem_status_present = true;
    }

    if (ert_msgpack_string_equals("c", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_ARRAY) {
        ert_log_error("Data logger entry comm array is not an array");
        result = -2;
        goto error_entry;
      }

      for (uint32_t comm_device_index = 0; comm_device_index < kv_obj->val.via.array.size; comm_device_index++) {
        result = deserialize_comm_device(&entry->params->comm_device_status[comm_device_index], &kv_obj->val.via.array.ptr[comm_device_index]);
        if (result < 0) {
          ert_log_error("Error deserializing comm device status, result %d", result);
          goto error_entry;
        }
        entry->params->comm_device_status_count++;
        entry->params->comm_device_status_present = true;
      }
    }

    if (ert_msgpack_string_equals("s", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_ARRAY) {
        ert_log_error("Data logger entry sensors array is not an array");
        result = -2;
        goto error_entry;
      }

      for (uint32_t sensor_index = 0; sensor_index < kv_obj->val.via.array.size; sensor_index++) {
        result = deserialize_sensor_data(entry->params, &kv_obj->val.via.array.ptr[sensor_index]);
        if (result < 0) {
          ert_log_error("Error deserializing sensor data, result %d", result);
          goto error_entry;
        }
      }
    }
  }

  msgpack_zone_destroy(&mempool);

  *entry_rcv = entry;

  return 0;

  error_entry:
  ert_data_logger_serializer_msgpack_destroy_entry(serializer, entry);

  error_msgpack:
  msgpack_zone_destroy(&mempool);

  return result;
}

int ert_data_logger_serializer_msgpack_create(ert_data_logger_serializer_msgpack_settings *settings,
    ert_data_logger_serializer **serializer_rcv)
{
  ert_data_logger_serializer *serializer = calloc(1, sizeof(ert_data_logger_serializer));
  if (serializer == NULL) {
    ert_log_fatal("Error allocating memory for serializer struct: %s", strerror(errno));
    return -ENOMEM;
  }

  serializer->serialize = ert_data_logger_serializer_msgpack_serialize;
  serializer->deserialize = ert_data_logger_serializer_msgpack_deserialize;
  serializer->priv = settings;

  *serializer_rcv = serializer;

  return 0;
}

int ert_data_logger_serializer_msgpack_destroy(ert_data_logger_serializer *serializer)
{
  free(serializer);

  return 0;
}
