/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_GPS_H
#define __ERT_GPS_H

#include "ert-common.h"
#include <time.h>

typedef enum _ert_gps_fix_mode {
  ERT_GPS_MODE_UNKNOWN = 0,
  ERT_GPS_MODE_NO_FIX	= 1,
  ERT_GPS_MODE_FIX_2D = 2,
  ERT_GPS_MODE_FIX_3D = 3,
} ert_gps_fix_mode;

typedef struct _ert_gps_data {
  struct timespec local_timestamp;

  bool has_fix;
  int satellites_visible;
  int satellites_used;
  double skyview_time_seconds;

  ert_gps_fix_mode mode;

  double time_seconds;
  double time_uncertainty_seconds;

  double latitude_degrees;
  double latitude_uncertainty_meters;
  double longitude_degrees;
  double longitude_uncertainty_meters;
  double altitude_meters;
  double altitude_uncertainty_meters;

  double track_degrees;
  double track_uncertainty_degrees;

  double speed_meters_per_sec;
  double speed_uncertainty_meters_per_sec;

  double climb_meters_per_sec;
  double climb_uncertainty_meters_per_sec;
} ert_gps_data;

void ert_gps_time_seconds_to_timespec(double time_seconds, struct timespec *ts);

#endif
