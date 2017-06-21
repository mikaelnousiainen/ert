/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_SERVER_SESSION_HTTP_H
#define __ERT_SERVER_SESSION_HTTP_H

#include "ert-server-session.h"

int http_send_file(struct lws *wsi, char *root_path, char *requested_uri);
int http_send_data_init(ert_server_session *session,
    struct lws *wsi, char *content_type, uint32_t data_length, unsigned char *data);
int http_send_data_continue(ert_server_session *session, struct lws *wsi);
int http_send_data_chunked_init(ert_server_session *session,
    struct lws *wsi, char *content_type,
    http_send_data_chunked_callback chunked_callback,
    http_send_data_chunked_callback_finished chunked_callback_finished,
    void *chunked_callback_context);
int http_send_data_chunked_continue(ert_server_session *session, struct lws *wsi);

int http_receive_body_data_init(ert_server_session *session, struct lws *wsi, uint32_t body_buffer_length, uint32_t session_type);
int http_receive_body_data(ert_server_session *session, struct lws *wsi, size_t length, void *data);
void http_receive_body_data_complete(ert_server_session *session);
void http_receive_body_data_destroy(ert_server_session *session);

#endif
