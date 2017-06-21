/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_GPS_LISTENER_H
#define __ERT_GPS_LISTENER_H

#include "ert-gps-driver.h"

struct _ert_gps_listener;

typedef void (*ert_gps_listener_callback)(struct _ert_gps_listener *listener, ert_gps_data *gps_data, void *callback_context);

typedef struct _ert_gps_listener {
  ert_gps *gps;
  int poll_timeout_milliseconds;

  pthread_t thread;
  volatile bool running;

  ert_gps_listener_callback callback;
  void *callback_context;

  ert_gps_data gps_data;
} ert_gps_listener;

int ert_gps_listener_start(ert_gps *gps, ert_gps_listener_callback callback, void *callback_context,
    int poll_timeout_milliseconds, ert_gps_listener **listener_rcv);
int ert_gps_get_current_data(ert_gps_listener *listener, ert_gps_data *gps_data);
int ert_gps_listener_stop(ert_gps_listener *listener);

#endif
