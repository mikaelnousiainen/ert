/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_EVENT_EMITTER_H
#define __ERT_EVENT_EMITTER_H

#include "ert-common.h"

typedef void (*ert_event_listener_func)(char *event, void *data, void *context);

typedef struct _ert_event_listener {
  void *context;
  ert_event_listener_func listener_func;
} ert_event_listener;

typedef struct _ert_event_listener_group {
  char *event;
  size_t listener_count;
  ert_event_listener *listeners;
} ert_event_listener_group;

typedef struct _ert_event_emitter {
  size_t listener_group_count;
  ert_event_listener_group *listener_groups;
} ert_event_emitter;

int ert_event_emitter_create(size_t event_count, size_t listeners_per_event_count,
    ert_event_emitter **event_emitter_rcv);
int ert_event_emitter_add_listener(ert_event_emitter *event_emitter,
    char *event, ert_event_listener_func listener_func, void *context);
bool ert_event_emitter_remove_listener(ert_event_emitter *event_emitter,
    char *event, ert_event_listener_func listener_func);
bool ert_event_emitter_emit(ert_event_emitter *event_emitter, char *event, void *data);
int ert_event_emitter_destroy(ert_event_emitter *event_emitter);


#endif
