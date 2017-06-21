/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <malloc.h>
#include <math.h>
#include <RTIMULib.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-sensor-module-rtimulib.h"

#define G_FORCE_METERS_PER_SECOND2 9.80665

#define DEFAULT_POLL_INTERVAL 400

#define SENSOR_ID_PRESSURE 0x01
#define SENSOR_ID_HUMIDITY 0x02
#define SENSOR_ID_IMU      0x03

static ert_sensor_data_type pressure_sensor_data_types[] = {
    ERT_SENSOR_TYPE_PRESSURE, ERT_SENSOR_TYPE_TEMPERATURE, ERT_SENSOR_TYPE_ALTITUDE
};
static ert_sensor_data_type humidity_sensor_data_types[] = {
    ERT_SENSOR_TYPE_HUMIDITY, ERT_SENSOR_TYPE_TEMPERATURE
};
static ert_sensor_data_type imu_sensor_data_types[] = {
    ERT_SENSOR_TYPE_ACCELEROMETER, ERT_SENSOR_TYPE_ACCELEROMETER_RESIDUALS, ERT_SENSOR_TYPE_ACCELEROMETER_MAGNITUDE,
    ERT_SENSOR_TYPE_GYROSCOPE,
    ERT_SENSOR_TYPE_MAGNETOMETER, ERT_SENSOR_TYPE_MAGNETOMETER_MAGNITUDE,
    ERT_SENSOR_TYPE_ORIENTATION
};

typedef struct _ert_sensor_module_rtimulib {
  RTIMUSettings *settings;

  RTPressure *pressure;
  RTHumidity *humidity;
  RTIMU *imu;
} ert_sensor_module_rtimulib;

inline double to_degrees(double radians) {
  return radians * (180.0 / M_PI);
}

inline double to_meters_per_second2(double g)
{
  return g * G_FORCE_METERS_PER_SECOND2;
}

static int ert_sensor_module_rtimulib_enumerate_sensors(ert_sensor_module *module)
{
  ert_sensor_module_rtimulib *driver = (ert_sensor_module_rtimulib *) module->priv;
  ert_sensor *sensors[3] = { NULL, NULL, NULL };
  int sensor_count = 0;
  int i;
  bool success;
  uint32_t poll_interval_milliseconds = DEFAULT_POLL_INTERVAL;

  driver->pressure = RTPressure::createPressure(driver->settings);
  if (driver->pressure != NULL) {
    success = driver->pressure->pressureInit();

    if (success) {
      sensors[sensor_count] = ert_sensor_create_sensor(SENSOR_ID_PRESSURE,
          driver->pressure->pressureName(), driver->pressure->pressureName(), "",
          3, pressure_sensor_data_types);
      sensor_count++;
    } else {
      delete driver->pressure;
      driver->pressure = NULL;
    }
  }

  driver->humidity = RTHumidity::createHumidity(driver->settings);
  if (driver->humidity != NULL) {
    success = driver->humidity->humidityInit();

    if (success) {
      sensors[sensor_count] = ert_sensor_create_sensor(SENSOR_ID_HUMIDITY,
          driver->humidity->humidityName(), driver->humidity->humidityName(), "",
          2, humidity_sensor_data_types);
      sensor_count++;
    } else {
      delete driver->humidity;
      driver->humidity = NULL;
    }
  }

  driver->imu = RTIMU::createIMU(driver->settings);
  if (driver->imu != NULL && driver->imu->IMUType() != RTIMU_TYPE_NULL) {
    success = driver->imu->IMUInit();

    if (success) {
      driver->imu->setSlerpPower(0.02);
      driver->imu->setGyroEnable(true);
      driver->imu->setAccelEnable(true);
      driver->imu->setCompassEnable(true);

      sensors[sensor_count] = ert_sensor_create_sensor(SENSOR_ID_IMU,
          driver->imu->IMUName(), driver->imu->IMUName(), "",
          7, imu_sensor_data_types);
      sensor_count++;

      poll_interval_milliseconds = driver->imu->IMUGetPollInterval();
    } else {
      delete driver->imu;
      driver->imu = NULL;
    }
  }

  module->sensor_count = sensor_count;

  module->sensors = (ert_sensor **) malloc(sensor_count * sizeof(ert_sensor *));
  for (i = 0; i < sensor_count; i++) {
    module->sensors[i] = sensors[i];
  }

  module->poll_interval_milliseconds = poll_interval_milliseconds;

  return 0;
}

int ert_sensor_module_rtimulib_open(ert_sensor_module **module_rcv)
{
  ert_sensor_module *module;
  ert_sensor_module_rtimulib *driver;

  module = (ert_sensor_module *) malloc(sizeof(ert_sensor_module));
  if (module == NULL) {
    ert_log_fatal("Error allocating memory for sensor module struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(module, 0, sizeof(ert_sensor_module));

  driver = (ert_sensor_module_rtimulib *) malloc(sizeof(ert_sensor_module_rtimulib));
  if (driver == NULL) {
    free(module);
    ert_log_fatal("Error allocating memory for RTIMULib driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(driver, 0, sizeof(ert_sensor_module_rtimulib));

  driver->settings = new RTIMUSettings("RTIMULib");

  module->name = "RTIMULib";
  module->priv = driver;

  ert_sensor_module_rtimulib_enumerate_sensors(module);

  *module_rcv = module;

  return 0;
}

int ert_sensor_module_rtimulib_read(ert_sensor_module *module, uint32_t sensor_id, ert_sensor_data *data_array)
{
  ert_sensor_module_rtimulib *driver = (ert_sensor_module_rtimulib *) module->priv;
  RTIMU_DATA rtimu_data;
  bool success;
  int data_index = 0;

  switch (sensor_id) {
    case SENSOR_ID_PRESSURE:
      if (driver->pressure == NULL) {
        return -2;
      }

      success = driver->pressure->pressureRead(rtimu_data);
      if (!success) {
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        return -1;
      }

      data_array[data_index].available = rtimu_data.pressureValid;
      data_array[data_index].value.value = rtimu_data.pressure;
      data_array[data_index].label = "Barometric pressure";
      data_array[data_index].unit = "hPa";
      data_index++;

      data_array[data_index].available = rtimu_data.temperatureValid;
      data_array[data_index].value.value = rtimu_data.temperature;
      data_array[data_index].label = "Temperature";
      data_array[data_index].unit = "C";
      data_index++;

      data_array[data_index].available = rtimu_data.pressureValid;
      data_array[data_index].value.value = ert_sensor_convert_pressure_to_altitude(rtimu_data.pressure, 1013.25d);
      data_array[data_index].label = "Altitude estimate";
      data_array[data_index].unit = "m";

      break;
    case SENSOR_ID_HUMIDITY:
      if (driver->humidity == NULL) {
        return -2;
      }

      success = driver->humidity->humidityRead(rtimu_data);
      if (!success) {
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        return -1;
      }

      data_array[data_index].available = rtimu_data.humidityValid;
      data_array[data_index].value.value = rtimu_data.humidity;
      data_array[data_index].label = "Relative humidity";
      data_array[data_index].unit = "%";
      data_index++;

      data_array[data_index].available = rtimu_data.temperatureValid;
      data_array[data_index].value.value = rtimu_data.temperature;
      data_array[data_index].label = "Temperature";
      data_array[data_index].unit = "C";
      break;
    case SENSOR_ID_IMU: {
      if (driver->imu == NULL) {
        return -2;
      }

      success = driver->imu->IMURead();
      if (!success) {
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        data_index++;
        data_array[data_index].available = false;
        return -1;
      }

      RTIMU_DATA imudata = driver->imu->getIMUData();

      data_array[data_index].available = imudata.accelValid;
      data_array[data_index].value.vector3.x = to_meters_per_second2(imudata.accel.x());
      data_array[data_index].value.vector3.y = to_meters_per_second2(imudata.accel.y());
      data_array[data_index].value.vector3.z = to_meters_per_second2(imudata.accel.z());
      data_array[data_index].label = "Accelerometer";
      data_array[data_index].unit = "m/s^2";
      data_index++;

      RTVector3 accel_residuals = driver->imu->getAccelResiduals();
      data_array[data_index].available = imudata.accelValid;
      data_array[data_index].value.vector3.x = to_meters_per_second2(accel_residuals.x());
      data_array[data_index].value.vector3.y = to_meters_per_second2(accel_residuals.y());
      data_array[data_index].value.vector3.z = to_meters_per_second2(accel_residuals.z());
      data_array[data_index].label = "Accelerometer residuals";
      data_array[data_index].unit = "m/s^2";
      data_index++;

      data_array[data_index].available = imudata.accelValid;
      data_array[data_index].value.value = to_meters_per_second2(imudata.accel.length());
      data_array[data_index].label = "Accelerometer magnitude";
      data_array[data_index].unit = "m/s^2";
      data_index++;

      data_array[data_index].available = imudata.gyroValid;
      data_array[data_index].value.vector3.x = to_degrees(imudata.gyro.x());
      data_array[data_index].value.vector3.y = to_degrees(imudata.gyro.y());
      data_array[data_index].value.vector3.z = to_degrees(imudata.gyro.z());
      data_array[data_index].label = "Gyroscope";
      data_array[data_index].unit = "deg/s";
      data_index++;

      data_array[data_index].available = imudata.compassValid;
      data_array[data_index].value.vector3.x = imudata.compass.x();
      data_array[data_index].value.vector3.y = imudata.compass.y();
      data_array[data_index].value.vector3.z = imudata.compass.z();
      data_array[data_index].label = "Magnetometer";
      data_array[data_index].unit = "uT";
      data_index++;

      data_array[data_index].available = imudata.compassValid;
      data_array[data_index].value.value = imudata.compass.length();
      data_array[data_index].label = "Magnetometer magnitude";
      data_array[data_index].unit = "uT";
      data_index++;

      data_array[data_index].available = imudata.fusionPoseValid;
      data_array[data_index].value.vector3.x = to_degrees(imudata.fusionPose.x());
      data_array[data_index].value.vector3.y = to_degrees(imudata.fusionPose.y());
      data_array[data_index].value.vector3.z = to_degrees(imudata.fusionPose.z());
      data_array[data_index].label = "Orientation";
      data_array[data_index].unit = "deg";
      break;
    }
    default:
      return -2;
  }

  return 0;
}

int ert_sensor_module_rtimulib_close(ert_sensor_module *module)
{
  ert_sensor_module_rtimulib *driver = (ert_sensor_module_rtimulib *) module->priv;

  delete driver->pressure;
  delete driver->humidity;
  delete driver->imu;
  delete driver->settings;
  free(driver);

  for (int i = 0; i < module->sensor_count; i++) {
    free(module->sensors[i]);
  }
  free(module->sensors);

  free(module);

  return 0;
}

ert_sensor_module_driver ert_sensor_module_driver_rtimulib = {
    .open = ert_sensor_module_rtimulib_open,
    .read = ert_sensor_module_rtimulib_read,
    .close = ert_sensor_module_rtimulib_close,
};
