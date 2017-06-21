/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DATA_LOGGER_SERIALIZER_MSGPACK_H
#define __ERT_DATA_LOGGER_SERIALIZER_MSGPACK_H

#include "ert-common.h"
#include "ert-data-logger.h"

#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSOR_DATA_TYPES -1

#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_MINIMAL 0
#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_MEDIUM 1
#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSORS 2
#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_SENSORS_WITH_COMM 3
#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_DATA 3
#define ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_DATA_WITH_TEXT 4

typedef struct _ert_data_logger_serializer_msgpack_settings {
  bool include_comm;
  bool include_modem;
  bool include_text;
  int8_t include_sensor_data_types_count;
  ert_sensor_data_type include_sensor_data_types[16];
} ert_data_logger_serializer_msgpack_settings;

extern ert_data_logger_serializer_msgpack_settings msgpack_serializer_settings[];

int ert_data_logger_serializer_msgpack_serialize(ert_data_logger_serializer *serializer,
    ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv);
int ert_data_logger_serializer_msgpack_serialize_with_settings(ert_data_logger_serializer *serializer,
    ert_data_logger_serializer_msgpack_settings *settings, ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv);
int ert_data_logger_serializer_msgpack_deserialize(ert_data_logger_serializer *serializer,
    uint32_t length, uint8_t *data, ert_data_logger_entry **entry_rcv);
int ert_data_logger_serializer_msgpack_destroy_entry(ert_data_logger_serializer *serializer,
    ert_data_logger_entry *entry);
int ert_data_logger_serializer_msgpack_create(ert_data_logger_serializer_msgpack_settings *settings,
    ert_data_logger_serializer **serializer_rcv);
int ert_data_logger_serializer_msgpack_destroy(ert_data_logger_serializer *serializer);

#endif
