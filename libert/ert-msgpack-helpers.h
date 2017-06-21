/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_MSGPACK_HELPERS_H
#define __ERT_MSGPACK_HELPERS_H

#include "ert-common.h"
#include <msgpack.h>

int ert_msgpack_serialize_string(msgpack_packer *pk, const char *value);
int ert_msgpack_serialize_map_key(msgpack_packer *pk, const char *key);
int ert_msgpack_serialize_map_key_string(msgpack_packer *pk, const char *key, const char *value);

bool ert_msgpack_string_equals(const char *string, msgpack_object_str *str_obj);
int ert_msgpack_copy_string_alloc(char **string, msgpack_object_str *str_obj);
int ert_msgpack_copy_string_object_alloc(char **string, msgpack_object *obj);
int ert_msgpack_copy_string(char *string, size_t length, msgpack_object_str *str_obj);
int ert_msgpack_copy_string_object(char *string, size_t length, msgpack_object *obj);

#endif
