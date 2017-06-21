/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <memory.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-sensor.h"

#ifdef ERTLIB_SUPPORT_RTIMULIB
#include "ert-sensor-module-rtimulib.h"
#endif
#include "ert-sensor-module-sysinfo.h"

#ifdef ERTLIB_SUPPORT_RTIMULIB
static int sensor_module_driver_count = 2;
#else
static int sensor_module_driver_count = 1;
#endif

static ert_sensor_module_driver *sensor_module_drivers[] = {
#ifdef ERTLIB_SUPPORT_RTIMULIB
    &ert_sensor_module_driver_rtimulib,
#endif
    &ert_sensor_module_driver_sysinfo
};

static ert_sensor_registry sensor_registry;

ert_sensor *ert_sensor_create_sensor(uint32_t id, const char *name, const char *model, const char *manufacturer,
    int data_type_count, ert_sensor_data_type *data_types)
{
  ert_sensor *sensor = (ert_sensor *) malloc(sizeof(ert_sensor));
  if (sensor == NULL) {
    ert_log_fatal("Error allocating memory for sensor struct: %s", strerror(errno));
    return NULL;
  }

  sensor->id = id;
  sensor->name = name;
  sensor->model = model;
  sensor->manufacturer = manufacturer;
  sensor->data_type_count = data_type_count;
  sensor->data_types = data_types;

  return sensor;
}

int ert_sensor_registry_init()
{
  int i, result;

  sensor_registry.modules = malloc(sensor_module_driver_count * sizeof(ert_sensor_module *));
  if (sensor_registry.modules == NULL) {
    ert_log_fatal("Error allocating memory for sensor registry: %s", strerror(errno));
    return -ENOMEM;
  }
  memset(sensor_registry.modules, 0, sensor_module_driver_count * sizeof(ert_sensor_module *));

  sensor_registry.module_count = sensor_module_driver_count;

  for (i = 0; i < sensor_registry.module_count; i++) {
    ert_sensor_module_driver *driver = sensor_module_drivers[i];

    ert_sensor_module *module;
    result = driver->open(&module);

    if (result != 0) {
      sensor_registry.module_count = 0;
      free(sensor_registry.modules);
      return result;
    }

    sensor_registry.modules[i] = module;

    ert_log_info("Initialized sensor module: %s", module->name);
  }

  return 0;
}

int ert_sensor_registry_read_sensor(int module_index, uint32_t sensor_id, ert_sensor_data *data_array)
{
  ert_sensor_module *module;
  ert_sensor_module_driver *driver;

  if (module_index < 0 || module_index >= sensor_registry.module_count) {
    return -EINVAL;
  }

  module = sensor_registry.modules[module_index];
  driver = sensor_module_drivers[module_index];

  return driver->read(module, sensor_id, data_array);
}

int ert_sensor_registry_create_module_data(int module_index, ert_sensor_module_data **module_data_rcv)
{
  ert_sensor_module_data *module_data;
  ert_sensor_with_data *sensor_with_data;
  ert_sensor_module *module;
  ert_sensor *sensor;
  int i;

  if (module_index < 0 || module_index >= sensor_registry.module_count) {
    return -EINVAL;
  }

  module_data = malloc(sizeof(ert_sensor_module_data));
  if (module_data == NULL) {
    ert_log_fatal("Error allocating memory for sensor module data: %s", strerror(errno));
    return -ENOMEM;
  }
  memset(module_data, 0, sizeof(ert_sensor_module_data));

  module = sensor_registry.modules[module_index];

  module_data->module = module;
  module_data->sensor_data_array = malloc(module->sensor_count * sizeof(ert_sensor_with_data));
  if (module_data->sensor_data_array == NULL) {
    free(module_data);
    ert_log_fatal("Error allocating memory for sensor data: %s", strerror(errno));
    return -ENOMEM;
  }
  memset(module_data->sensor_data_array, 0, module->sensor_count * sizeof(ert_sensor_with_data));

  for (i = 0; i < module->sensor_count; i++) {
    sensor_with_data = &module_data->sensor_data_array[i];
    sensor = module->sensors[i];

    sensor_with_data->sensor = sensor;
    sensor_with_data->data_array = malloc(sensor->data_type_count * sizeof(ert_sensor_data));
    if (sensor_with_data->data_array == NULL) {
      free(module_data->sensor_data_array);
      free(module_data);
      ert_log_fatal("Error allocating memory for sensor data values: %s", strerror(errno));
      return -ENOMEM;
    }

    memset(sensor_with_data->data_array, 0, sensor->data_type_count * sizeof(ert_sensor_data));
  }

  *module_data_rcv = module_data;

  return 0;
}

int ert_sensor_registry_read_module(int module_index, ert_sensor_module_data *module_data)
{
  ert_sensor_module *module;
  ert_sensor_with_data *sensor_with_data;
  ert_sensor *sensor;
  int i, result;

  if (module_index < 0 || module_index >= sensor_registry.module_count) {
    return -EINVAL;
  }

  module = sensor_registry.modules[module_index];

  result = ert_get_current_timestamp(&module_data->timestamp);
  if (result != 0) {
    return -EIO;
  }

  for (i = 0; i < module->sensor_count; i++) {
    sensor = module->sensors[i];
    sensor_with_data = &module_data->sensor_data_array[i];

    result = ert_sensor_registry_read_sensor(module_index, sensor->id, sensor_with_data->data_array);
    sensor_with_data->available = (result == 0);
  }

  return 0;
}

int ert_sensor_registry_destroy_module_data(ert_sensor_module_data *module_data)
{
  ert_sensor_with_data *sensor_with_data;
  int i;

  for (i = 0; i < module_data->module->sensor_count; i++) {
    sensor_with_data = &module_data->sensor_data_array[i];

    free(sensor_with_data->data_array);
  }

  free(module_data->sensor_data_array);
  free(module_data);

  return 0;
}

int ert_sensor_registry_create_module_data_all(int module_count, ert_sensor_module_data **module_data_rcv)
{
  int result;

  int count = (module_count < sensor_registry.module_count) ? module_count : sensor_registry.module_count;

  for (int module_index = 0; module_index < count; module_index++) {
    result = ert_sensor_registry_create_module_data(module_index, &module_data_rcv[module_index]);
    if (result < 0) {
      return result;
    }
  }

  return count;
}

int ert_sensor_registry_read_module_all(int module_count, ert_sensor_module_data **module_data)
{
  int count = (module_count < sensor_registry.module_count) ? module_count : sensor_registry.module_count;

  for (int module_index = 0; module_index < count; module_index++) {
    ert_sensor_registry_read_module(module_index, module_data[module_index]);
  }

  return count;
}

int ert_sensor_registry_destroy_module_data_all(int module_count, ert_sensor_module_data **module_data)
{
  int result = 0;

  int count = (module_count < sensor_registry.module_count) ? module_count : sensor_registry.module_count;

  for (int module_index = 0; module_index < count; module_index++) {
    int r = ert_sensor_registry_destroy_module_data(module_data[module_index]);
    if (r < 0) {
      result = r;
    }
  }

  return result;
}

int ert_sensor_registry_uninit()
{
  ert_sensor_module_driver *driver;
  int i;

  for (i = 0; i < sensor_registry.module_count; i++) {
    driver = sensor_module_drivers[i];
    driver->close(sensor_registry.modules[i]);
  }

  sensor_registry.module_count = 0;
  free(sensor_registry.modules);

  return 0;
}
