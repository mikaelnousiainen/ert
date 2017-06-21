/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DATA_LOGGER_H
#define __ERT_DATA_LOGGER_H

#include "ert-common.h"
#include "ert-sensor.h"
#include "ert-gps-listener.h"
#include "ert-comm-transceiver.h"
#include "ert-comm-protocol.h"
#include "ert-driver-gsm-modem.h"

#include <time.h>

#define ERT_DATA_LOGGER_SENSOR_MODULE_COUNT 16
#define ERT_DATA_LOGGER_COMM_DEVICE_COUNT 16
#define ERT_DATA_LOGGER_ENTRY_RELATED_COUNT 16

#define ERT_DATA_LOGGER_ENTRY_TYPE_TELEMETRY 0x01
#define ERT_DATA_LOGGER_ENTRY_TYPE_GATEWAY   0x02

struct _ert_data_logger_entry;

typedef struct _ert_data_logger_writer {
  void *priv;

  int (*write)(struct _ert_data_logger_writer *writer, uint32_t length, uint8_t *data);
} ert_data_logger_writer;

typedef struct _ert_data_logger_serializer {
  void *priv;

  int (*serialize)(struct _ert_data_logger_serializer *serializer,
      struct _ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv);
  int (*deserialize)(struct _ert_data_logger_serializer *serializer,
      uint32_t length, uint8_t *data, struct _ert_data_logger_entry **entry);
} ert_data_logger_serializer;

typedef struct _ert_data_logger_entry_related ert_data_logger_entry_related;

typedef struct _ert_data_logger_entry_params {
  bool gps_data_present;
  ert_gps_data gps_data;

  uint8_t sensor_module_data_count;
  ert_sensor_module_data *sensor_module_data[ERT_DATA_LOGGER_SENSOR_MODULE_COUNT];

  uint8_t comm_device_status_count;
  bool comm_device_status_present;
  ert_comm_device_status comm_device_status[ERT_DATA_LOGGER_COMM_DEVICE_COUNT];
  bool comm_protocol_status_present;
  ert_comm_protocol_status comm_protocol_status[ERT_DATA_LOGGER_COMM_DEVICE_COUNT];

  bool gsm_modem_status_present;
  ert_driver_gsm_modem_status gsm_modem_status;

  uint8_t related_entry_count;
  ert_data_logger_entry_related *related[ERT_DATA_LOGGER_ENTRY_RELATED_COUNT];
} ert_data_logger_entry_params;

typedef struct _ert_data_logger_entry {
  uint32_t entry_id;
  uint8_t entry_type;
  struct timespec timestamp;

  const char *device_name;
  const char *device_model;

  ert_data_logger_entry_params *params;
} ert_data_logger_entry;

struct _ert_data_logger_entry_related {
  ert_data_logger_entry *entry;

  double distance_meters;
  double altitude_diff_meters;
  double azimuth_degrees;
  double elevation_degrees;
};

typedef struct _ert_data_logger {
  uint32_t current_entry_id;

  const char *device_name;
  const char *device_model;

  ert_data_logger_serializer *serializer;
  ert_data_logger_writer *writer;
} ert_data_logger;

int ert_data_logger_create(const char *device_name, const char *device_model,
    ert_data_logger_serializer *serializer, ert_data_logger_writer *writer,
    ert_data_logger **data_logger_rcv);
int ert_data_logger_log(ert_data_logger *data_logger, ert_data_logger_entry *entry);
int ert_data_logger_populate_entry(ert_data_logger *data_logger, ert_data_logger_entry_params *params,
    ert_data_logger_entry *entry, uint8_t entry_type);
int ert_data_logger_collect_entry_params(ert_data_logger *data_logger,
    ert_comm_transceiver *comm_transceiver,
    ert_comm_protocol *comm_protocol,
    ert_driver_gsm_modem *gsm_modem,
    ert_data_logger_entry_params *params);
#ifdef ERTLIB_SUPPORT_GPSD
int ert_data_logger_collect_entry_params_with_gps(ert_data_logger *data_logger,
    ert_comm_transceiver *comm_transceiver,
    ert_comm_protocol *comm_protocol,
    ert_driver_gsm_modem *gsm_modem,
    ert_gps_listener *gps_listener,
    ert_data_logger_entry_params *params);
#endif
int ert_data_logger_init_entry_params(ert_data_logger *data_logger,
    ert_data_logger_entry_params *params);
int ert_data_logger_uninit_entry_params(ert_data_logger *data_logger,
    ert_data_logger_entry_params *params);
int ert_data_logger_destroy(ert_data_logger *data_logger);

int ert_data_logger_clone_entry(ert_data_logger_entry *source_entry, ert_data_logger_entry **dest_entry_rcv);
int ert_data_logger_destroy_entry(ert_data_logger_entry *entry);

#endif
