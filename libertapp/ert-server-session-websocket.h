/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_SERVER_SESSION_WEBSOCKET_H
#define __ERT_SERVER_SESSION_WEBSOCKET_H

#include "ert-server-session.h"

#define ERT_SERVER_WEBSOCKET_PROTOCOL_DATA_LOGGER "ert-data-logger"

int ws_send_message_init(ert_server_session *session,
    struct lws *wsi, uint32_t data_length, unsigned char *data);
int ws_send_message_continue(ert_server_session *session, struct lws *wsi);

#endif
