/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <memory.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-sensor-module-sysinfo.h"

#define SENSOR_ID_SYSINFO              0x01
#define SENSOR_SYSINFO_DATA_TYPE_COUNT 5

#define SYSINFO_SENSOR_COUNT 1

static ert_sensor_data_type sysinfo_sensor_data_types[] = {
    ERT_SENSOR_TYPE_SYSTEM_UPTIME,
    ERT_SENSOR_TYPE_SYSTEM_LOAD_AVG,
    ERT_SENSOR_TYPE_SYSTEM_MEM_USED,
    ERT_SENSOR_TYPE_SYSTEM_SWAP_USED,
    ERT_SENSOR_TYPE_SYSTEM_PROCESS_COUNT
};

static int ert_sensor_module_sysinfo_enumerate_sensors(ert_sensor_module *module)
{
  ert_sensor *sensors[SYSINFO_SENSOR_COUNT];
  int sensor_count = 0;

  sensors[sensor_count] = ert_sensor_create_sensor(SENSOR_ID_SYSINFO,
      "sysinfo", "sysinfo()", "Linux", SENSOR_SYSINFO_DATA_TYPE_COUNT,
      sysinfo_sensor_data_types);
  sensor_count++;
  module->sensor_count = sensor_count;

  module->sensors = (ert_sensor **) malloc(sensor_count * sizeof(ert_sensor *));
  for (int i = 0; i < sensor_count; i++) {
    module->sensors[i] = sensors[i];
  }

  module->poll_interval_milliseconds = 1000;

  return 0;
}

int ert_sensor_module_sysinfo_open(ert_sensor_module **module_rcv)
{
  ert_sensor_module *module;

  module = (ert_sensor_module *) malloc(sizeof(ert_sensor_module));
  if (module == NULL) {
    ert_log_fatal("Error allocating memory for sensor module struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(module, 0, sizeof(ert_sensor_module));

  module->name = "sysinfo";

  ert_sensor_module_sysinfo_enumerate_sensors(module);

  *module_rcv = module;

  return 0;
}

int ert_sensor_module_sysinfo_read(ert_sensor_module *module, uint32_t sensor_id, ert_sensor_data *data_array) {
  int result;
  int data_index = 0;

  switch (sensor_id) {
    case SENSOR_ID_SYSINFO: {
      struct sysinfo si;
      result = sysinfo(&si);
      if (result < 0) {
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        return result;
      }

      data_array[data_index].available = true;
      data_array[data_index].value.value = si.uptime;
      data_array[data_index].label = "System uptime";
      data_array[data_index].unit = "s";
      data_index++;

      data_array[data_index].available = true;
      data_array[data_index].value.value = (double) si.loads[0] / (double) (1 << SI_LOAD_SHIFT);
      data_array[data_index].label = "Load average (1 minute)";
      data_array[data_index].unit = "CPUs";
      data_index++;

      data_array[data_index].available = true;
      data_array[data_index].value.value =
          (1.0 - ((double) si.freeram + (double) si.bufferram) / (double) si.totalram) * 100.0;
      data_array[data_index].label = "Memory used";
      data_array[data_index].unit = "%";
      data_index++;

      data_array[data_index].available = true;
      data_array[data_index].value.value =
          (1.0 - (double) si.freeswap / (double) si.totalswap) * 100.0;
      data_array[data_index].label = "Swap used";
      data_array[data_index].unit = "%";
      data_index++;

      data_array[data_index].available = true;
      data_array[data_index].value.value = si.procs;
      data_array[data_index].label = "Process count";
      data_array[data_index].unit = "processes";
      break;
    }
    default:
      return -2;
  }

  return 0;
}

int ert_sensor_module_sysinfo_close(ert_sensor_module *module)
{
  for (int i = 0; i < module->sensor_count; i++) {
    free(module->sensors[i]);
  }
  free(module->sensors);

  free(module);

  return 0;
}

ert_sensor_module_driver ert_sensor_module_driver_sysinfo = {
    .open = ert_sensor_module_sysinfo_open,
    .read = ert_sensor_module_sysinfo_read,
    .close = ert_sensor_module_sysinfo_close,
};
