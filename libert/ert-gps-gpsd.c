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
#include <gps.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-gps-gpsd.h"

typedef struct _ert_gps_gpsd {
  struct gps_data_t gps_data;
} ert_gps_gpsd;

int ert_gps_gpsd_open(char *server, char *port, ert_gps **gps_rcv)
{
  int result;
  ert_gps *gps;
  ert_gps_gpsd *driver;

  gps = malloc(sizeof(ert_gps));
  if (gps == NULL) {
    ert_log_fatal("Error allocating memory for GPS struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(gps, 0, sizeof(ert_gps));

  driver = malloc(sizeof(ert_gps_gpsd));
  if (driver == NULL) {
    free(gps);
    ert_log_fatal("Error allocating memory for GPSD driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(driver, 0, sizeof(ert_gps_gpsd));

  result = gps_open(server, port, &driver->gps_data);
  if (result != 0) {
    free(driver);
    free(gps);
    ert_log_fatal("Error connecting to GPSD: %s", server);
    return -EINVAL;
  }

  gps->priv = driver;

  *gps_rcv = gps;

  return 0;
}

int ert_gps_gpsd_start_receive(ert_gps *gps)
{
  ert_gps_gpsd *driver = (ert_gps_gpsd *) gps->priv;

  return gps_stream(&driver->gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
}

static ert_gps_fix_mode ert_gps_gpsd_convert_mode(int gpsd_mode) {
  switch (gpsd_mode) {
    case MODE_NOT_SEEN:
      return ERT_GPS_MODE_UNKNOWN;
    case MODE_NO_FIX:
      return ERT_GPS_MODE_NO_FIX;
    case MODE_2D:
      return ERT_GPS_MODE_FIX_2D;
    case MODE_3D:
      return ERT_GPS_MODE_FIX_3D;
    default:
      return ERT_GPS_MODE_UNKNOWN;
  }
}

int ert_gps_gpsd_receive(ert_gps *gps, ert_gps_data *data, int timeout_millis)
{
  ert_gps_gpsd *driver = (ert_gps_gpsd *) gps->priv;
  int result;

  result = gps_waiting(&driver->gps_data, timeout_millis * 1000);
  if (result == 0) {
    return -ETIMEDOUT;
  }

  int read_status = gps_read(&driver->gps_data);
  if (read_status < 0) {
    return -EIO;
  }
  if (read_status == 0) {
    return -ETIMEDOUT;
  }

  result = ert_get_current_timestamp(&data->local_timestamp);
  if (result != 0) {
    return -EIO;
  }

  data->has_fix = (driver->gps_data.status == STATUS_FIX);
  data->satellites_used = driver->gps_data.satellites_used;
  data->satellites_visible = driver->gps_data.satellites_visible;
  data->skyview_time_seconds = driver->gps_data.skyview_time;

  data->mode = ert_gps_gpsd_convert_mode(driver->gps_data.fix.mode);

  data->time_seconds = driver->gps_data.fix.time;
  data->time_uncertainty_seconds = driver->gps_data.fix.ept;

  data->latitude_degrees = driver->gps_data.fix.latitude;
  data->latitude_uncertainty_meters = driver->gps_data.fix.epy;
  data->longitude_degrees = driver->gps_data.fix.longitude;
  data->longitude_uncertainty_meters = driver->gps_data.fix.epx;
  data->altitude_meters = driver->gps_data.fix.altitude;
  data->altitude_uncertainty_meters = driver->gps_data.fix.epv;

  data->track_degrees = driver->gps_data.fix.track;
  data->track_uncertainty_degrees = driver->gps_data.fix.epd;
  data->speed_meters_per_sec = driver->gps_data.fix.speed;
  data->speed_uncertainty_meters_per_sec = driver->gps_data.fix.eps;
  data->climb_meters_per_sec = driver->gps_data.fix.climb;
  data->climb_uncertainty_meters_per_sec = driver->gps_data.fix.epc;

  return 0;
}

int ert_gps_gpsd_stop_receive(ert_gps *gps)
{
  ert_gps_gpsd *driver = (ert_gps_gpsd *) gps->priv;

  return gps_stream(&driver->gps_data, WATCH_DISABLE, NULL);
}

int ert_gps_gpsd_close(ert_gps *gps)
{
  ert_gps_gpsd *driver = (ert_gps_gpsd *) gps->priv;

  ert_gps_gpsd_stop_receive(gps);

  gps_close(&driver->gps_data);
  free(driver);
  free(gps);

  return 0;
}

ert_gps_driver ert_gps_driver_gpsd = {
    .open = ert_gps_gpsd_open,
    .start_receive = ert_gps_gpsd_start_receive,
    .receive = ert_gps_gpsd_receive,
    .stop_receive = ert_gps_gpsd_stop_receive,
    .close = ert_gps_gpsd_close
};
