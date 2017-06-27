/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTNODE_H
#define __ERTNODE_H

#define ERTLIB_SUPPORT_GPSD
#define ERTLIB_SUPPORT_RTIMULIB

#include <pthread.h>

#include "ert.h"
#include "ert-driver-gsm-modem.h"
#include "ert-driver-rfm9xw.h"
#include "ert-server.h"

#include "ertnode-common.h"

typedef struct _ert_node_sender_telemetry_config {
  bool enabled;
  uint32_t telemetry_collect_interval_seconds;
  uint32_t telemetry_send_interval;
  uint32_t minimal_telemetry_data_send_interval;
} ert_node_sender_telemetry_config;

typedef struct _ert_node_sender_image_config {
  bool enabled;
  uint32_t image_capture_interval_seconds;
  uint32_t image_send_interval;

  char raspistill_command[1024];
  char convert_command[1024];

  bool horizontal_flip;
  bool vertical_flip;

  char image_path[1024];
  int16_t original_image_quality;

  char transmitted_image_format[16];
  int16_t transmitted_image_width;
  int16_t transmitted_image_height;
  int16_t transmitted_image_quality;
} ert_node_sender_image_config;

typedef struct _ert_node_gsm_modem_config {
  bool enabled;

  bool sms_telemetry_enabled;
  uint32_t sms_telemetry_send_interval;
  char destination_number[32];

  ert_driver_gsm_modem_config modem_config;
} ert_node_gsm_modem_config;

typedef struct _ert_node_gps_config {
  bool enabled;

  char gpsd_host[256];
  uint32_t gpsd_port;

  char serial_device_file[256];
  uint32_t serial_device_speed;
} ert_node_gps_config;

typedef struct _ert_node_config {
  char device_name[128];
  char device_model[128];

  ert_driver_rfm9xw_static_config rfm9xw_static_config;
  ert_driver_rfm9xw_config rfm9xw_config;

  ert_node_gsm_modem_config gsm_modem_config;

  ert_node_sender_telemetry_config sender_telemetry_config;
  ert_node_sender_image_config sender_image_config;
  ert_node_gps_config gps_config;

  ert_comm_transceiver_config comm_transceiver_config;
  ert_comm_protocol_config comm_protocol_config;

  ert_server_config server_config;
} ert_node_config;

typedef struct _ert_node {
  volatile bool running;

  ert_node_config config;

  ert_server *server;

  ert_event_emitter *event_emitter;

  pthread_t telemetry_collector_thread;
  pthread_t image_collector_thread;
  pthread_t telemetry_sender_thread;
  pthread_t image_sender_thread;

  ert_gps *gps;
  ert_gps_listener *gps_listener;

  ert_comm_device *comm_device;
  ert_comm_transceiver *comm_transceiver;
  ert_comm_protocol_device *comm_protocol_device;
  ert_comm_protocol *comm_protocol;

  bool gsm_modem_initialized;
  ert_driver_gsm_modem *gsm_modem;

  ert_data_logger_serializer *jansson_serializer;

  ert_data_logger_writer *zlog_writer;
  ert_data_logger *data_logger;
  ert_data_logger_entry_params data_logger_entry_params;

  ert_data_logger_serializer *msgpack_serializer;
} ert_node;

#endif
