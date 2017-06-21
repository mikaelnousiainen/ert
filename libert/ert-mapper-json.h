/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_MAPPER_JSON_H
#define __ERT_MAPPER_JSON_H

#include "ert-mapper.h"

int ert_mapper_serialize_to_json(ert_mapper_entry *root_mapping_entry, char **data_rcv);
int ert_mapper_update_from_json(ert_mapper_entry *entry, char *json, void *update_context);

#endif
