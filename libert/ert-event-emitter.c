/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "ert-event-emitter.h"
#include "ert-log.h"

int ert_event_emitter_create(size_t event_count, size_t listeners_per_event_count,
    ert_event_emitter **event_emitter_rcv)
{
  int result;

  ert_event_emitter *event_emitter = calloc(1, sizeof(ert_event_emitter));
  if (event_emitter == NULL) {
    ert_log_fatal("Error allocating memory for event emitter struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_first;
  }

  event_emitter->listener_group_count = event_count;
  event_emitter->listener_groups = calloc(event_count, sizeof(ert_event_listener_group));
  if (event_emitter->listener_groups == NULL) {
    ert_log_fatal("Error allocating memory for event emitter group struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_malloc_event_emitter;
  }

  for (size_t i = 0; i < event_count; i++) {
    ert_event_listener_group *group = &event_emitter->listener_groups[i];

    group->event = NULL;
    group->listener_count = listeners_per_event_count;

    group->listeners = calloc(listeners_per_event_count, sizeof(ert_event_listener));
    if (event_emitter->listener_groups == NULL) {
      ert_log_fatal("Error allocating memory for event emitter listener struct: %s", strerror(errno));
      result = -ENOMEM;
      goto error_malloc_event_listeners;
    }
  }

  *event_emitter_rcv = event_emitter;

  return 0;

  error_malloc_event_listeners:
  for (size_t i = 0; i < listeners_per_event_count; i++) {
    ert_event_listener_group *group = &event_emitter->listener_groups[i];
    if (group->listeners != NULL) {
      free(group->listeners);
    }
  }

  free(event_emitter->listener_groups);

  error_malloc_event_emitter:
  free(event_emitter);

  error_first:

  return result;
}

static ert_event_listener_group *ert_event_emitter_find_group(ert_event_emitter *event_emitter, char *event)
{
  for (size_t i = 0; i < event_emitter->listener_group_count; i++) {
    ert_event_listener_group *group = &event_emitter->listener_groups[i];
    if (group->event != NULL && strcmp(group->event, event) == 0) {
      return group;
    }
  }

  return NULL;
}

static ert_event_listener_group *ert_event_listener_find_or_create_group(ert_event_emitter *event_emitter, char *event)
{
  ert_event_listener_group *group = ert_event_emitter_find_group(event_emitter, event);
  if (group != NULL) {
    return group;
  }

  for (size_t i = 0; i < event_emitter->listener_group_count; i++) {
    group = &event_emitter->listener_groups[i];

    if (group->event == NULL) {
      group->event = strdup(event);
      return group;
    }
  }

  return NULL;
}

static bool ert_event_listener_remove_group(ert_event_emitter *event_emitter, char *event)
{
  ert_event_listener_group *group = ert_event_emitter_find_group(event_emitter, event);
  if (group == NULL) {
    return false;
  }

  free(group->event);
  group->event = NULL;

  return true;
}

int ert_event_emitter_add_listener(ert_event_emitter *event_emitter,
    char *event, ert_event_listener_func listener_func, void *context)
{
  ert_event_listener_group *group = ert_event_listener_find_or_create_group(event_emitter, event);
  if (group == NULL) {
    return -ENOBUFS;
  }

  for (size_t i = 0; i < group->listener_count; i++) {
    ert_event_listener *listener = &group->listeners[i];
    if (listener->listener_func == listener_func) {
      return -EINVAL;
    }
  }

  for (size_t i = 0; i < group->listener_count; i++) {
    ert_event_listener *listener = &group->listeners[i];
    if (listener->listener_func == NULL) {
      listener->listener_func = listener_func;
      listener->context = context;
      return 0;
    }
  }


  return -ENOBUFS;
}

bool ert_event_emitter_remove_listener(ert_event_emitter *event_emitter,
    char *event, ert_event_listener_func listener_func)
{
  ert_event_listener_group *group = ert_event_emitter_find_group(event_emitter, event);
  if (group == NULL) {
    return false;
  }

  bool found = false;
  for (size_t i = 0; i < group->listener_count; i++) {
    ert_event_listener *listener = &group->listeners[i];
    if (listener->listener_func == listener_func) {
      listener->context = NULL;
      listener->listener_func = NULL;
      found = true;
    }
  }

  bool empty = true;
  for (size_t i = 0; i < group->listener_count; i++) {
    ert_event_listener *listener = &group->listeners[i];
    if (listener->listener_func != NULL) {
      empty = false;
      break;
    }
  }

  if (empty) {
    ert_event_listener_remove_group(event_emitter, event);
  }

  return found;
}

bool ert_event_emitter_emit(ert_event_emitter *event_emitter, char *event, void *data)
{
  ert_event_listener_group *group = ert_event_emitter_find_group(event_emitter, event);
  if (group == NULL) {
    return false;
  }

  bool fired = false;
  for (size_t i = 0; i < group->listener_count; i++) {
    ert_event_listener *listener = &group->listeners[i];
    if (listener->listener_func != NULL) {
      listener->listener_func(group->event, data, listener->context);
      fired = true;
    }
  }

  return fired;
}

int ert_event_emitter_destroy(ert_event_emitter *event_emitter)
{
  for (size_t i = 0; i < event_emitter->listener_group_count; i++) {
    ert_event_listener_group *group = &event_emitter->listener_groups[i];
    if (group->event != NULL) {
      free(group->event);
    }
    if (group->listeners != NULL) {
      free(group->listeners);
    }
  }

  free(event_emitter->listener_groups);
  free(event_emitter);

  return 0;
}