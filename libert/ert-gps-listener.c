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
#include <pthread.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-gps-listener.h"

static void *ert_gps_listener_routine(void *context)
{
  ert_gps_listener *listener = (ert_gps_listener *) context;
  int result;

  result = ert_gps_start_receive(listener->gps);
  if (result < 0) {
    ert_log_error("Error starting reception of GPS data");
    return NULL;
  }

  listener->running = true;

  while (listener->running) {
    result = ert_gps_receive(listener->gps, &listener->gps_data, listener->poll_timeout_milliseconds);

    if (result > 0 && listener->callback != NULL) {
      listener->callback(listener, &listener->gps_data, listener->callback_context);
    }
  }

  result = ert_gps_stop_receive(listener->gps);
  if (result < 0) {
    ert_log_error("Error stopping reception of GPS data");
    return NULL;
  }

  return NULL;
}

int ert_gps_listener_start(ert_gps *gps, ert_gps_listener_callback callback, void *callback_context,
    int poll_timeout_milliseconds, ert_gps_listener **listener_rcv)
{
  ert_gps_listener *listener;
  int result;

  listener = malloc(sizeof(ert_gps_listener));
  if (listener == NULL) {
    ert_log_fatal("Error allocating memory for GPS listener struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(listener, 0, sizeof(ert_gps_listener));

  listener->gps = gps;
  listener->callback = callback;
  listener->callback_context = callback_context;
  listener->poll_timeout_milliseconds = poll_timeout_milliseconds;
  listener->running = false;

  result = pthread_create(&listener->thread, NULL, ert_gps_listener_routine, listener);
  if (result != 0) {
    free(listener);
    ert_log_error("Error starting thread for GPS listener: %s", strerror(errno));
    return -EIO;
  }

  *listener_rcv = listener;

  return result;
}

int ert_gps_get_current_data(ert_gps_listener *listener, ert_gps_data *gps_data)
{
  memcpy(gps_data, &listener->gps_data, sizeof(ert_gps_data));

  return 0;
}

int ert_gps_listener_stop(ert_gps_listener *listener)
{
  listener->running = false;

  pthread_join(listener->thread, NULL);

  free(listener);

  return 0;
}
