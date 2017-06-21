/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_SENSOR_H
#define __ERT_SENSOR_H

#include "ert-common.h"
#include <time.h>

#define ERT_SENSOR_TYPE_FLAG_VECTOR3 0x80

typedef enum _ert_sensor_data_type {
  ERT_SENSOR_TYPE_UNKNOWN = 0x00,
  ERT_SENSOR_TYPE_GENERIC = 0x01,
  ERT_SENSOR_TYPE_TEMPERATURE = 0x11,
  ERT_SENSOR_TYPE_HUMIDITY,
  ERT_SENSOR_TYPE_PRESSURE,
  ERT_SENSOR_TYPE_ACCELEROMETER_MAGNITUDE,
  ERT_SENSOR_TYPE_MAGNETOMETER_MAGNITUDE,
  ERT_SENSOR_TYPE_ALTITUDE,
  ERT_SENSOR_TYPE_ACCELEROMETER = 0x21 | ERT_SENSOR_TYPE_FLAG_VECTOR3,
  ERT_SENSOR_TYPE_GYROSCOPE,
  ERT_SENSOR_TYPE_MAGNETOMETER,
  ERT_SENSOR_TYPE_ACCELEROMETER_RESIDUALS,
  ERT_SENSOR_TYPE_ORIENTATION = 0x31 | ERT_SENSOR_TYPE_FLAG_VECTOR3,
  ERT_SENSOR_TYPE_SYSTEM = 0x41,
  ERT_SENSOR_TYPE_SYSTEM_UPTIME,
  ERT_SENSOR_TYPE_SYSTEM_LOAD_AVG,
  ERT_SENSOR_TYPE_SYSTEM_MEM_USED,
  ERT_SENSOR_TYPE_SYSTEM_SWAP_USED,
  ERT_SENSOR_TYPE_SYSTEM_PROCESS_COUNT
} ert_sensor_data_type;

typedef struct _ert_vector3 {
  double x;
  double y;
  double z;
} ert_vector3;

typedef struct _ert_sensor_data {
  bool available;

  union {
    double value;
    ert_vector3 vector3;
  } value;

  const char *label;
  const char *unit;
} ert_sensor_data;

typedef struct _ert_sensor {
  uint32_t id;
  const char *name;

  const char *model;
  const char *manufacturer;

  int data_type_count;
  ert_sensor_data_type *data_types;
} ert_sensor;

typedef struct _ert_sensor_module {
  const char *name;

  int sensor_count;
  ert_sensor **sensors;

  uint32_t poll_interval_milliseconds;

  void *priv;
} ert_sensor_module;

typedef struct _ert_sensor_module_driver {
  int (*open)(ert_sensor_module **module_rcv);
  int (*read)(ert_sensor_module *module, uint32_t sensor_id, ert_sensor_data *data_array);
  int (*close)(ert_sensor_module *module);
} ert_sensor_module_driver;

typedef struct _ert_sensor_with_data {
  bool available;
  ert_sensor *sensor;
  ert_sensor_data *data_array;
} ert_sensor_with_data;

typedef struct _ert_sensor_module_data {
  struct timespec timestamp;
  ert_sensor_module *module;
  ert_sensor_with_data *sensor_data_array;
} ert_sensor_module_data;

typedef struct _ert_sensor_registry {
  int module_count;
  ert_sensor_module **modules;
} ert_sensor_registry;

//  Convering pressure to altitude
//  Implementation from RTIMULib RTMath.cpp
//
//  h = (T0 / L0) * ((p / P0)**(-(R* * L0) / (g0 * M)) - 1)
//
//  where:
//  h  = height above sea level
//  T0 = standard temperature at sea level = 288.15
//  L0 = standard temperature elapse rate = -0.0065
//  p  = measured pressure
//  P0 = static pressure = 1013.25 (but can be overridden)
//  g0 = gravitational acceleration = 9.80665
//  M  = molecular mass of earth's air = 0.0289644
//  R* = universal gas constant = 8.31432
//
//  Given the constants, this works out to:
//
//  h = 44330.8 * (1 - (p / P0)**0.190263)
#define ert_sensor_convert_pressure_to_altitude(pressure, static_pressure) (44330.8d * (1.0d - pow(pressure / static_pressure, 0.190263d)))

#ifdef __cplusplus
extern "C" {
#endif

ert_sensor *ert_sensor_create_sensor(uint32_t id, const char *name, const char *model, const char *manufacturer,
    int data_type_count, ert_sensor_data_type *data_types);

#ifdef __cplusplus
}
#endif

int ert_sensor_registry_init();
int ert_sensor_registry_read_sensor(int module_index, uint32_t sensor_id, ert_sensor_data *data_array);
int ert_sensor_registry_create_module_data(int module_index, ert_sensor_module_data **module_data_rcv);
int ert_sensor_registry_read_module(int module_index, ert_sensor_module_data *module_data);
int ert_sensor_registry_destroy_module_data(ert_sensor_module_data *module_data);
int ert_sensor_registry_create_module_data_all(int module_count, ert_sensor_module_data **module_data_rcv);
int ert_sensor_registry_read_module_all(int module_count, ert_sensor_module_data **module_data);
int ert_sensor_registry_destroy_module_data_all(int module_count, ert_sensor_module_data **module_data);
int ert_sensor_registry_uninit();

#endif
