/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTGATEWAY_SERVER_H
#define __ERTGATEWAY_SERVER_H

void ert_gateway_server_attach_events(ert_event_emitter *event_emitter, ert_server *server);
void ert_gateway_server_detach_events(ert_event_emitter *event_emitter);

#endif
