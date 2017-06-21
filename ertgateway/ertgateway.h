/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTGATEWAY_H
#define __ERTGATEWAY_H

#define ERT_GATEWAY_MAX_COMM_THREAD_COUNT 16

#include "ert.h"
#include "ert-pipe.h"
#include "ert-data-logger-serializer-jansson.h"
#include "ert-data-logger-serializer-msgpack.h"
#include "ert-data-logger-writer-zlog.h"
#include "ert-data-logger-utils.h"
#include "ert-driver-rfm9xw.h"
#include "ert-driver-st7036.h"
#include "ert-server.h"
#include "ertgateway-common.h"

typedef struct _ert_gateway_handler_display_config {
  uint32_t display_update_interval_seconds;
  uint32_t display_mode_cycle_length_in_update_intervals;
} ert_gateway_handler_display_config;

typedef struct _ert_gateway_ui_dothat_config {
  bool backlight_enabled;
  bool touch_enabled;
  bool leds_enabled;
  uint8_t brightness;
  uint32_t backlight_color;
  uint32_t backlight_error_color;
} ert_gateway_ui_dothat_config;

typedef struct _ert_gateway_handler_telemetry_config {
  uint32_t gateway_telemetry_collect_interval_seconds;
} ert_gateway_handler_telemetry_config;

typedef struct _ert_gateway_handler_image_config {
  char image_path[1024];
  char image_format[16];
} ert_gateway_handler_image_config;

typedef struct _ert_gateway_gps_config {
  bool enabled;

  char gpsd_host[256];
  uint32_t gpsd_port;
} ert_gateway_gps_config;

typedef struct _ert_gateway_config {
  char device_name[128];
  char device_model[128];

  ert_gateway_gps_config gps_config;

  uint32_t thread_count_per_comm_handler;

  ert_gateway_handler_telemetry_config handler_telemetry_config;

  ert_gateway_handler_display_config handler_display_config;
  ert_gateway_handler_image_config handler_image_config;

  ert_driver_rfm9xw_static_config rfm9xw_static_config;
  ert_driver_rfm9xw_config rfm9xw_config;

  ert_server_config server_config;

  bool st7036_enabled;
  ert_driver_st7036_static_config st7036_static_config;

  ert_gateway_ui_dothat_config dothat_config;

  ert_comm_transceiver_config comm_transceiver_config;
  ert_comm_protocol_config comm_protocol_config;
} ert_gateway_config;

typedef struct _ert_gateway {
  volatile bool running;

  ert_gateway_config config;

  ert_server *server;

  ert_event_emitter *event_emitter;

  ert_gps *gps;
  ert_gps_listener *gps_listener;

  pthread_t node_telemetry_handler_threads[ERT_GATEWAY_MAX_COMM_THREAD_COUNT];
  pthread_t image_handler_threads[ERT_GATEWAY_MAX_COMM_THREAD_COUNT];
  pthread_t gateway_telemetry_handler_thread;
  pthread_t display_handler_thread;

  ert_comm_device *comm_device;
  ert_comm_transceiver *comm_transceiver;
  ert_comm_protocol_device *comm_protocol_device;
  ert_comm_protocol *comm_protocol;

  ert_data_logger_serializer *jansson_serializer;

  ert_data_logger_writer *zlog_writer_node;
  ert_data_logger *data_logger_node;
  ert_data_logger_entry_params data_logger_entry_params_node;

  ert_data_logger_writer *zlog_writer_gateway;
  ert_data_logger *data_logger_gateway;
  ert_data_logger_entry_params data_logger_entry_params_gateway;

  ert_pipe *telemetry_stream_queue;
  ert_pipe *image_stream_queue;

  ert_data_logger_serializer *msgpack_serializer;

  uint32_t image_index;

  ert_log_logger *display_logger;

  pthread_mutex_t related_entry_mutex;
  ert_data_logger_entry *related_entry;
} ert_gateway;

#endif
