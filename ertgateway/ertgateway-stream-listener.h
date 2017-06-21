/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTGATEWAY_STREAM_LISTENER_H
#define __ERTGATEWAY_STREAM_LISTENER_H

#include "ertgateway.h"

void ert_gateway_stream_listener_callback(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, void *callback_context);

#endif
