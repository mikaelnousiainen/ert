/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-gps-driver.h"
#include "ert-gps-gpsd.h"

ert_gps_driver *gps_driver = &ert_gps_driver_gpsd;

int ert_gps_open(char *server, char *port, ert_gps **gps_rcv)
{
  return gps_driver->open(server, port, gps_rcv);
}

int ert_gps_start_receive(ert_gps *gps)
{
  return gps_driver->start_receive(gps);
}

int ert_gps_receive(ert_gps *gps, ert_gps_data *data, int timeout_millis)
{
  return gps_driver->receive(gps, data, timeout_millis);
}

int ert_gps_stop_receive(ert_gps *gps)
{
  return gps_driver->stop_receive(gps);
}

int ert_gps_close(ert_gps *gps)
{
  return gps_driver->close(gps);
}
