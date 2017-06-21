/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_PIPE_H
#define __ERT_PIPE_H

#include "ert-common.h"
#include "pipe.h"

typedef struct _ert_pipe {
  pipe_producer_t *producer;
  pipe_consumer_t *consumer;
} ert_pipe;

int ert_pipe_create(size_t element_size, size_t limit, ert_pipe **pipe_rcv);
int ert_pipe_push(ert_pipe *pipe, void *elements, size_t count);
size_t ert_pipe_pop(ert_pipe *pipe, void *target, size_t count);
ssize_t ert_pipe_pop_timed(ert_pipe *pipe, void *target, size_t count, uint32_t timeout_milliseconds);
int ert_pipe_close(ert_pipe *pipe);
int ert_pipe_destroy(ert_pipe *pipe);

#endif
