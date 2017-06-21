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
#include <string.h>
#include <time.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-data-logger.h"

int ert_data_logger_create(const char *device_name, const char *device_model,
    ert_data_logger_serializer *serializer, ert_data_logger_writer *writer,
    ert_data_logger **data_logger_rcv)
{
  ert_data_logger *data_logger = calloc(1, sizeof(ert_data_logger));
  if (data_logger == NULL) {
    ert_log_fatal("Error allocating memory for data logger struct: %s", strerror(errno));
    return -ENOMEM;
  }

  data_logger->device_name = device_name;
  data_logger->device_model = device_model;
  data_logger->current_entry_id = 1;
  data_logger->serializer = serializer;
  data_logger->writer = writer;

  *data_logger_rcv = data_logger;

  return 0;
}

int ert_data_logger_log(ert_data_logger *data_logger, ert_data_logger_entry *entry)
{
  int result;

  uint32_t length;
  uint8_t *data;

  result = data_logger->serializer->serialize(data_logger->serializer, entry, &length, &data);
  if (result < 0) {
    ert_log_error("Data logger serializer failed with result %d", result);
    return result;
  }

  result = data_logger->writer->write(data_logger->writer, length, data);
  free(data);

  if (result < 0) {
    ert_log_error("Data logger writer failed with result %d", result);
    return result;
  }

  return 0;
}

int ert_data_logger_populate_entry(ert_data_logger *data_logger, ert_data_logger_entry_params *params,
    ert_data_logger_entry *entry, uint8_t entry_type)
{
  int result;

  result = clock_gettime(CLOCK_REALTIME, &entry->timestamp);
  if (result < 0) {
    ert_log_error("Error getting current time for data logger: %s", strerror(errno));
    return -EIO;
  }

  entry->device_name = data_logger->device_name;
  entry->device_model = data_logger->device_model;

  entry->entry_id = data_logger->current_entry_id;
  data_logger->current_entry_id++;
  entry->entry_type = entry_type;

  entry->params = params;

  return 0;
}

int ert_data_logger_init_entry_params(ert_data_logger *data_logger,
  ert_data_logger_entry_params *params)
{
  int result = ert_sensor_registry_create_module_data_all(
      ERT_DATA_LOGGER_SENSOR_MODULE_COUNT, params->sensor_module_data);
  if (result < 0) {
    return result;
  }

  params->gps_data_present = false;
  params->sensor_module_data_count = (uint8_t) result;
  params->comm_device_status_count = 1;

  return 0;
}

int ert_data_logger_collect_entry_params(ert_data_logger *data_logger,
    ert_comm_transceiver *comm_transceiver,
    ert_comm_protocol *comm_protocol,
    ert_driver_gsm_modem *gsm_modem,
    ert_data_logger_entry_params *params)
{
  int result;

  params->comm_device_status_present = false;
  if (comm_transceiver != NULL) {
    result = ert_comm_transceiver_get_device_status(comm_transceiver, &params->comm_device_status[0]);
    if (result < 0) {
      return result;
    }
    params->comm_device_status_present = true;
  }

  params->comm_protocol_status_present = false;
  if (comm_protocol != NULL) {
    result = ert_comm_protocol_get_status(comm_protocol, &params->comm_protocol_status[0]);
    if (result < 0) {
      return result;
    }
    params->comm_protocol_status_present = true;
  }

  result = ert_sensor_registry_read_module_all(params->sensor_module_data_count, params->sensor_module_data);
  if (result < 0) {
    return result;
  }

  params->gsm_modem_status_present = false;
  if (gsm_modem != NULL) {
    result = ert_driver_gsm_modem_get_status(gsm_modem, &params->gsm_modem_status);
    if (result < 0) {
      return result;
    }
    params->gsm_modem_status_present = true;
  }

  return 0;
}

#ifdef ERTLIB_SUPPORT_GPSD

int ert_data_logger_collect_entry_params_with_gps(ert_data_logger *data_logger,
    ert_comm_transceiver *comm_transceiver,
    ert_comm_protocol *comm_protocol,
    ert_driver_gsm_modem *gsm_modem,
    ert_gps_listener *gps_listener,
    ert_data_logger_entry_params *params)
{
  int result;

  params->gps_data_present = false;
  if (gps_listener != NULL) {
    result = ert_gps_get_current_data(gps_listener, &params->gps_data);
    if (result < 0) {
      return result;
    }
    params->gps_data_present = true;
  }

  result = ert_data_logger_collect_entry_params(data_logger, comm_transceiver, comm_protocol, gsm_modem, params);
  if (result < 0) {
    return result;
  }

  return 0;
}

#endif

int ert_data_logger_uninit_entry_params(ert_data_logger *data_logger,
    ert_data_logger_entry_params *params)
{
  int result = ert_sensor_registry_destroy_module_data_all(
      params->sensor_module_data_count, params->sensor_module_data);
  if (result < 0) {
    return result;
  }

  params->sensor_module_data_count = 0;

  return 0;
}

int ert_data_logger_destroy(ert_data_logger *data_logger)
{
  free(data_logger);
  return 0;
}

static int ert_data_logger_clone_sensor(ert_sensor *source_sensor, ert_sensor **dest_sensor_rcv)
{
  ert_sensor *dest_sensor = malloc(sizeof(ert_sensor));
  if (dest_sensor == NULL) {
    ert_log_fatal("Error allocating memory for ert_sensor: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(dest_sensor, source_sensor, sizeof(ert_sensor));

  if (source_sensor->name != NULL) {
    dest_sensor->name = strdup(source_sensor->name);
    if (dest_sensor->name == NULL) {
      ert_log_fatal("Error allocating memory for sensor name: %s", strerror(errno));
      return -ENOMEM;
    }
  }
  if (source_sensor->manufacturer != NULL) {
    dest_sensor->manufacturer = strdup(source_sensor->manufacturer);
    if (dest_sensor->manufacturer == NULL) {
      ert_log_fatal("Error allocating memory for sensor manufacturer: %s", strerror(errno));
      return -ENOMEM;
    }
  }
  if (source_sensor->model != NULL) {
    dest_sensor->model = strdup(source_sensor->model);
    if (dest_sensor->model == NULL) {
      ert_log_fatal("Error allocating memory for sensor model: %s", strerror(errno));
      return -ENOMEM;
    }
  }

  int data_type_count = source_sensor->data_type_count;

  dest_sensor->data_types = malloc(data_type_count * sizeof(ert_sensor_data_type));
  if (dest_sensor->data_types == NULL) {
    ert_log_fatal("Error allocating memory for ert_sensor_data_type array: %s", strerror(errno));
    return -ENOMEM;
  }
  memcpy(dest_sensor->data_types, source_sensor->data_types, data_type_count * sizeof(ert_sensor_data_type));

  *dest_sensor_rcv = dest_sensor;

  return 0;
}

static int ert_data_logger_clone_sensor_with_data(ert_sensor_with_data *source_sensor_with_data,
    ert_sensor_with_data *dest_sensor_with_data, ert_sensor_module *dest_module)
{
  memcpy(dest_sensor_with_data, source_sensor_with_data, sizeof(ert_sensor_with_data));

  ert_sensor *dest_sensor = NULL;
  for (size_t sensor_index = 0; sensor_index < dest_module->sensor_count; sensor_index++) {
    ert_sensor *current_dest_sensor = dest_module->sensors[sensor_index];
    if (current_dest_sensor->id == source_sensor_with_data->sensor->id) {
      dest_sensor = current_dest_sensor;
    }
  }
  if (dest_sensor == NULL) {
    ert_log_error("Cannot find sensor ID %d for cloning", source_sensor_with_data->sensor->id);
    return -EINVAL;
  }

  dest_sensor_with_data->sensor = dest_sensor;

  int data_type_count = source_sensor_with_data->sensor->data_type_count;

  dest_sensor_with_data->data_array = malloc(data_type_count * sizeof(ert_sensor_data));
  if (dest_sensor_with_data->data_array == NULL) {
    ert_log_fatal("Error allocating memory for ert_sensor_data array: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(dest_sensor_with_data->data_array, source_sensor_with_data->data_array,
      data_type_count * sizeof(ert_sensor_data));

  for (size_t sensor_data_index = 0; sensor_data_index < data_type_count; sensor_data_index++) {
    ert_sensor_data *source_sensor_data = &source_sensor_with_data->data_array[sensor_data_index];
    ert_sensor_data *dest_sensor_data = &dest_sensor_with_data->data_array[sensor_data_index];

    if (source_sensor_data->label != NULL) {
      dest_sensor_data->label = strdup(source_sensor_data->label);
      if (dest_sensor_data->label == NULL) {
        ert_log_fatal("Error allocating memory for sensor label: %s", strerror(errno));
        return -ENOMEM;
      }
    }
    if (source_sensor_data->unit != NULL) {
      dest_sensor_data->unit = strdup(source_sensor_data->unit);
      if (dest_sensor_data->unit == NULL) {
        ert_log_fatal("Error allocating memory for sensor unit: %s", strerror(errno));
        return -ENOMEM;
      }
    }
  }

  return 0;
}

int ert_data_logger_clone_entry(ert_data_logger_entry *source_entry, ert_data_logger_entry **dest_entry_rcv) {
  int result;

  ert_data_logger_entry *dest_entry = malloc(sizeof(ert_data_logger_entry));
  if (dest_entry == NULL) {
    ert_log_fatal("Error allocating memory for ert_data_logger_entry: %s", strerror(errno));
    return -ENOMEM;
  }
  memcpy(dest_entry, source_entry, sizeof(ert_data_logger_entry));

  if (source_entry->device_name != NULL) {
    dest_entry->device_name = strdup(source_entry->device_name);
    if (dest_entry->device_name == NULL) {
      ert_log_fatal("Error allocating memory for device name: %s", strerror(errno));
      return -ENOMEM;
    }
  }
  if (source_entry->device_model != NULL) {
    dest_entry->device_model = strdup(source_entry->device_model);
    if (dest_entry->device_name == NULL) {
      ert_log_fatal("Error allocating memory for device model: %s", strerror(errno));
      return -ENOMEM;
    }
  }

  ert_data_logger_entry_params *source_params = source_entry->params;

  ert_data_logger_entry_params *dest_params = malloc(sizeof(ert_data_logger_entry_params));
  if (dest_params == NULL) {
    ert_log_fatal("Error allocating memory for ert_data_logger_entry_params: %s", strerror(errno));
    return -ENOMEM;
  }
  memcpy(dest_params, source_params, sizeof(ert_data_logger_entry_params));
  dest_entry->params = dest_params;

  for (size_t sensor_module_data_index = 0; sensor_module_data_index < source_params->sensor_module_data_count; sensor_module_data_index++) {
    ert_sensor_module_data *source_sensor_module_data = source_params->sensor_module_data[sensor_module_data_index];
    ert_sensor_module_data *dest_sensor_module_data = malloc(sizeof(ert_sensor_module_data));
    if (dest_sensor_module_data == NULL) {
      ert_log_fatal("Error allocating memory for ert_sensor_module_data: %s", strerror(errno));
      return -ENOMEM;
    }

    memcpy(dest_sensor_module_data, source_sensor_module_data, sizeof(ert_sensor_module_data));
    dest_params->sensor_module_data[sensor_module_data_index] = dest_sensor_module_data;

    ert_sensor_module *source_module = source_sensor_module_data->module;
    ert_sensor_module *dest_module = malloc(sizeof(ert_sensor_module));
    if (dest_module == NULL) {
      ert_log_fatal("Error allocating memory for ert_sensor_module: %s", strerror(errno));
      return -ENOMEM;
    }
    memcpy(dest_module, source_module, sizeof(ert_sensor_module));
    dest_sensor_module_data->module = dest_module;

    if (source_module->name != NULL) {
      dest_module->name = strdup(source_module->name);
      if (dest_module->name == NULL) {
        ert_log_fatal("Error allocating memory for module name: %s", strerror(errno));
        return -ENOMEM;
      }
    }

    dest_module->sensors = malloc(source_module->sensor_count * sizeof(ert_sensor *));
    if (dest_module->sensors == NULL) {
      ert_log_fatal("Error allocating memory for sensor array: %s", strerror(errno));
      return -ENOMEM;
    }

    for (size_t sensor_index = 0; sensor_index < source_module->sensor_count; sensor_index++) {
      result = ert_data_logger_clone_sensor(source_module->sensors[sensor_index], &dest_module->sensors[sensor_index]);
      if (result < 0) {
        ert_log_error("Error cloning sensor, result %d", result);
        ert_data_logger_destroy_entry(dest_entry);
        return result;
      }
    }

    int sensor_count = source_sensor_module_data->module->sensor_count;
    dest_sensor_module_data->sensor_data_array = malloc(sensor_count * sizeof(ert_sensor_with_data));
    if (dest_sensor_module_data->sensor_data_array == NULL) {
      ert_log_fatal("Error allocating memory for sensor data array: %s", strerror(errno));
      return -ENOMEM;
    }

    for (size_t sensor_with_data_array_index = 0; sensor_with_data_array_index < sensor_count; sensor_with_data_array_index++) {
      ert_sensor_with_data *source_sensor_with_data = &source_sensor_module_data->sensor_data_array[sensor_with_data_array_index];
      ert_sensor_with_data *dest_sensor_with_data = &dest_sensor_module_data->sensor_data_array[sensor_with_data_array_index];

      result = ert_data_logger_clone_sensor_with_data(source_sensor_with_data, dest_sensor_with_data, dest_module);
      if (result < 0) {
        ert_log_error("Error cloning sensor with data, result %d", result);
        ert_data_logger_destroy_entry(dest_entry);
        return result;
      }
    }

  }

  *dest_entry_rcv = dest_entry;

  return 0;
}

int ert_data_logger_destroy_entry(ert_data_logger_entry *entry) {
  if (entry->device_name != NULL) {
    free((void *) entry->device_name);
  }
  if (entry->device_model != NULL) {
    free((void *) entry->device_model);
  }

  if (entry->params != NULL) {
    for (size_t module_index = 0; module_index < entry->params->sensor_module_data_count; module_index++) {
      ert_sensor_module_data *module_data = entry->params->sensor_module_data[module_index];
      ert_sensor_module *module = module_data->module;

      for (size_t sensor_index = 0; sensor_index < module->sensor_count; sensor_index++) {
        ert_sensor_with_data *sensor_with_data = &module_data->sensor_data_array[sensor_index];

        if (sensor_with_data->data_array != NULL) {
          for (size_t sensor_data_index = 0; sensor_data_index < sensor_with_data->sensor->data_type_count; sensor_data_index++) {
            ert_sensor_data *sensor_data = &sensor_with_data->data_array[sensor_data_index];
            if (sensor_data->unit != NULL) {
              free((char *) sensor_data->unit);
            }

            if (sensor_data->label != NULL) {
              free((char *) sensor_data->label);
            }
          }

          free(sensor_with_data->data_array);
        }

        ert_sensor *sensor = module->sensors[sensor_index];
        if (sensor->name != NULL) {
          free((void *) sensor->name);
        }
        if (sensor->model != NULL) {
          free((void *) sensor->model);
        }
        if (sensor->manufacturer != NULL) {
          free((void *) sensor->manufacturer);
        }
        if (sensor->data_types != NULL) {
          free(sensor->data_types);
        }
        free(sensor);
      }

      if (module->sensors != NULL) {
        free(module->sensors);
      }

      if (module->name != NULL) {
        free((void *) module->name);
      }

      if (module_data->sensor_data_array != NULL) {
        free(module_data->sensor_data_array);
      }

      free(module);
      free(module_data);
    }

    free(entry->params);
  }

  free(entry);

  return 0;
}
