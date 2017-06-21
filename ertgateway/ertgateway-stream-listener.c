/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <unistd.h>

#include "ertgateway-stream-listener.h"

void ert_gateway_stream_listener_callback(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, void *callback_context)
{
  ert_gateway *gateway = (ert_gateway *) callback_context;
  ert_comm_protocol_stream_info stream_info;
  int result;

  result = ert_comm_protocol_stream_get_info(stream, &stream_info);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_get_info failed with result %d", result);
    return;
  }

  ert_log_info("New stream ID %d in port %d", stream_info.stream_id, stream_info.port);

  ert_pipe *stream_queue;
  switch (stream_info.port) {
    case ERT_STREAM_PORT_TELEMETRY_MSGPACK:
      stream_queue = gateway->telemetry_stream_queue;
      break;
    case ERT_STREAM_PORT_IMAGE:
      stream_queue = gateway->image_stream_queue;
      break;
    default:
      ert_log_warn("Unknown port in stream: %d", stream_info.port);
      result = ert_comm_protocol_receive_stream_close(comm_protocol, stream);
      if (result < 0) {
        ert_log_error("ert_comm_protocol_receive_stream_close failed with result %d", result);
      }
      return;
  }

  ert_pipe_push(stream_queue, &stream, 1);
}
