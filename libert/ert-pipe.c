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
#include <string.h>
#include <errno.h>
#include "ert-pipe.h"
#include "ert-log.h"

int ert_pipe_create(size_t element_size, size_t limit, ert_pipe **pipe_rcv)
{
  ert_pipe *pipe = (ert_pipe *) malloc(sizeof(ert_pipe));
  if (pipe == NULL) {
    ert_log_fatal("Error allocating memory for pipe struct: %s", strerror(errno));
    return -ENOMEM;
  }

  pipe_t *p = pipe_new(element_size, limit);
  if (p == NULL) {
    free(pipe);
    ert_log_fatal("Error creating new pipe: %s", strerror(errno));
    return -ENOMEM;
  }

  pipe->producer = pipe_producer_new(p);
  pipe->consumer = pipe_consumer_new(p);

  pipe_free(p);

  *pipe_rcv = pipe;

  return 0;
}

int ert_pipe_push(ert_pipe *pipe, void *elements, size_t count)
{
  pipe_push(pipe->producer, elements, count);
  return 0;
}

size_t ert_pipe_pop(ert_pipe *pipe, void *target, size_t count)
{
  return (size_t) pipe_pop(pipe->consumer, target, count);
}

ssize_t ert_pipe_pop_timed(ert_pipe *pipe, void *target, size_t count, uint32_t timeout_milliseconds)
{
  return pipe_pop_timed(pipe->consumer, target, count, (size_t) timeout_milliseconds);
}

int ert_pipe_close(ert_pipe *pipe)
{
  pipe_producer_free(pipe->producer);

  return 0;
}

int ert_pipe_destroy(ert_pipe *pipe)
{
  pipe_consumer_free(pipe->consumer);
  free(pipe);

  return 0;
}
