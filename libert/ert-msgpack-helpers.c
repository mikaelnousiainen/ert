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
#include <errno.h>

#include "ert-msgpack-helpers.h"

#include "ert-log.h"

inline int ert_msgpack_serialize_string(msgpack_packer *pk, const char *value)
{
  size_t length = strlen(value);

  msgpack_pack_str(pk, length);
  msgpack_pack_str_body(pk, value, length);

  return 0;
}

inline int ert_msgpack_serialize_map_key(msgpack_packer *pk, const char *key)
{
  return ert_msgpack_serialize_string(pk, key);
}

inline int ert_msgpack_serialize_map_key_string(msgpack_packer *pk, const char *key, const char *value)
{
  ert_msgpack_serialize_map_key(pk, key);
  if (value == NULL) {
    msgpack_pack_nil(pk);
  } else {
    ert_msgpack_serialize_string(pk, value);
  }

  return 0;
}

inline bool ert_msgpack_string_equals(const char *string, msgpack_object_str *str_obj)
{
  size_t string_length = strlen(string);

  if (string_length != str_obj->size) {
    return false;
  }

  for (uint32_t i = 0; i < str_obj->size; i++) {
    if (string[i] != str_obj->ptr[i]) {
      return false;
    }
  }

  return true;
}

int ert_msgpack_copy_string_alloc(char **string, msgpack_object_str *str_obj)
{
  *string = malloc(str_obj->size + 1);
  if (*string == NULL) {
    ert_log_fatal("Error allocating memory for data logger entry string: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(*string, str_obj->ptr, str_obj->size);

  (*string)[str_obj->size] = '\0';

  return 0;
}

int ert_msgpack_copy_string_object_alloc(char **string, msgpack_object *obj)
{
  if (obj->type == MSGPACK_OBJECT_NIL) {
    *string = NULL;
    return 0;
  }

  if (obj->type != MSGPACK_OBJECT_STR) {
    ert_log_error("Object is not a string");
    return -1;
  }

  return ert_msgpack_copy_string_alloc(string, &obj->via.str);
}

int ert_msgpack_copy_string(char *string, size_t length, msgpack_object_str *str_obj)
{
  size_t bytes_to_copy = (str_obj->size < (length - 1) ? str_obj->size : (length - 1));
  memcpy(string, str_obj->ptr, bytes_to_copy);

  string[bytes_to_copy] = '\0';

  return 0;
}

int ert_msgpack_copy_string_object(char *string, size_t length, msgpack_object *obj)
{
  if (obj->type == MSGPACK_OBJECT_NIL) {
    string[0] = '\0';
    return 0;
  }

  if (obj->type != MSGPACK_OBJECT_STR) {
    ert_log_error("Object is not a string");
    return -1;
  }

  return ert_msgpack_copy_string(string, length, &obj->via.str);
}
