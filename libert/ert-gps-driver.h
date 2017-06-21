/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_GPS_DRIVER_H
#define __ERT_GPS_DRIVER_H

#include "ert-common.h"
#include "ert-gps.h"

typedef struct _ert_gps {
  void *priv;
} ert_gps;

typedef struct _ert_gps_driver {
  int (*open)(char *server, char *port, ert_gps **gps_rcv);
  int (*start_receive)(ert_gps *gps);
  int (*receive)(ert_gps *gps, ert_gps_data *data, int timeout_millis);
  int (*stop_receive)(ert_gps *gps);
  int (*close)(ert_gps *gps);
} ert_gps_driver;

int ert_gps_open(char *server, char *port, ert_gps **gps_rcv);
int ert_gps_start_receive(ert_gps *gps);
int ert_gps_receive(ert_gps *gps, ert_gps_data *data, int timeout_millis);
int ert_gps_stop_receive(ert_gps *gps);
int ert_gps_close(ert_gps *gps);

#endif
