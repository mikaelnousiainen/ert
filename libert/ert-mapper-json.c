/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>
#include <jansson.h>

#include "ert-mapper-json.h"
#include "ert-log.h"

#define jansson_check_result(x) \
  if ((x) < 0) { \
    ert_log_error("JSON serialization failed"); \
    return -EIO; \
  }

static json_t *ert_mapper_serialize_to_json_value(ert_mapper_entry *entry, char *string_value)
{
  int result;

  if (entry->enum_entries != NULL) {
    char name[128];
    result = ert_mapper_get_enum_name_for_value(entry, 128, name);
    if (result < 0) {
      ert_log_error("Error serializing enum mapper entry to JSON string: %s", entry->name);
      return json_null();
    }

    return json_string(name);
  }

  switch (entry->type) {
    case ERT_MAPPER_ENTRY_TYPE_BOOLEAN: {
      int32_t value;
      result = ert_mapper_convert_to_int32(string_value, &value);
      if (result < 0) {
        ert_log_error("Error serializing mapper entry to JSON boolean: %s", entry->name);
      }
      return json_boolean(value);
    }
    case ERT_MAPPER_ENTRY_TYPE_INT8:
    case ERT_MAPPER_ENTRY_TYPE_INT16:
    case ERT_MAPPER_ENTRY_TYPE_INT32:
    case ERT_MAPPER_ENTRY_TYPE_INT64:
    case ERT_MAPPER_ENTRY_TYPE_UINT8:
    case ERT_MAPPER_ENTRY_TYPE_UINT16:
    case ERT_MAPPER_ENTRY_TYPE_UINT32:
    case ERT_MAPPER_ENTRY_TYPE_UINT64: {
      int64_t value;
      result = ert_mapper_convert_to_int64(string_value, &value);
      if (result < 0) {
        ert_log_error("Error serializing mapper entry to JSON integer: %s", entry->name);
      }
      return json_integer(value);
    }
    case ERT_MAPPER_ENTRY_TYPE_FLOAT:
    case ERT_MAPPER_ENTRY_TYPE_DOUBLE: {
      double value;
      result = ert_mapper_convert_to_double(string_value, &value);
      if (result < 0) {
        ert_log_error("Error serializing mapper entry to JSON integer: %s", entry->name);
      }

      if (isnan(value) || isinf(value)) {
        return json_null();
      }

      return json_real(value);
    }
    case ERT_MAPPER_ENTRY_TYPE_STRING:
      return json_string(string_value);
    default:
      ert_log_error("Unhandled mapper type %d for JSON serialization: %s", entry->type, entry->name);
  }

  return json_null();
}

static int ert_mapper_serialize_to_json_internal(ert_mapper_entry *current_entry, json_t *current_json_obj)
{
  ert_mapper_entry *child_entry;
  size_t index = 0;
  char value[1024];
  int result;

  do {
    child_entry = &current_entry[index];

    switch (child_entry->type) {
      case ERT_MAPPER_ENTRY_TYPE_MAPPING:
        if (child_entry->children != NULL) {
          json_t *child_json_obj = json_object();

          result = ert_mapper_serialize_to_json_internal(child_entry->children, child_json_obj);
          if (result < 0) {
            json_decref(child_json_obj);
            ert_log_error("Config JSON serialization failed for entry '%s'", child_entry->name);
            return -EIO;
          }

          jansson_check_result(json_object_set_new(current_json_obj, child_entry->name, child_json_obj));
        }
        break;
      case ERT_MAPPER_ENTRY_TYPE_SEQUENCE:
        ert_log_error("Sequences not supported yet for config JSON serialization");
        break;
      case ERT_MAPPER_ENTRY_TYPE_NONE:
        break;
      default:
        result = ert_mapper_get_value_as_string(child_entry, 1024, value);
        if (result < 0) {
          ert_log_error("Config value serialization failed for entry '%s'", child_entry->name);
          return -EIO;
        }

        json_t* json_value = ert_mapper_serialize_to_json_value(child_entry, value);

        jansson_check_result(json_object_set_new(current_json_obj, child_entry->name, json_value));
        break;
    }

    index++;
  } while (child_entry->type != ERT_MAPPER_ENTRY_TYPE_NONE);

  return 0;
}

int ert_mapper_serialize_to_json(ert_mapper_entry *root_mapping_entry, char **data_rcv)
{
  json_t *json_obj = json_object();

  int result = ert_mapper_serialize_to_json_internal(root_mapping_entry, json_obj);
  if (result < 0) {
    json_decref(json_obj);
    ert_log_error("Error creating JSON tree from mapper values");
    return -EIO;
  }

  char *data = json_dumps(json_obj, JSON_COMPACT | JSON_PRESERVE_ORDER);
  json_decref(json_obj);

  if (data == NULL) {
    ert_log_error("Error serializing JSON tree to string");
    return -EIO;
  }

  *data_rcv = data;

  return 0;
}

static int ert_mapper_update_from_json_internal(ert_mapper_entry *entries, json_t *json_obj, void *update_context)
{
  const char *key;
  json_t *value;
  char string_value[1024];
  int result = 0;

  bool no_update_callback_found = false;

  json_object_foreach(json_obj, key, value) {
    ert_mapper_entry *current_entry = ert_mapper_find_entry(entries, (char *) key);
    if (current_entry == NULL) {
      ert_log_error("Cannot find mapper entry name '%s'", key);
      return -EINVAL;
    }

    bool value_json_node = false;

    switch (json_typeof(value)) {
      case JSON_STRING:
        strncpy(string_value, json_string_value(value), 1024);
        value_json_node = true;
        break;
      case JSON_INTEGER:
        snprintf(string_value, 1024, "%" PRId64, (int64_t) json_integer_value(value));
        value_json_node = true;
        break;
      case JSON_REAL:
        snprintf(string_value, 1024, "%f", json_real_value(value));
        value_json_node = true;
        break;
      case JSON_TRUE:
      case JSON_FALSE:
        snprintf(string_value, 1024, "%s", json_boolean_value(value) ? "true" : "false");
        value_json_node = true;
        break;
      case JSON_NULL:
        string_value[0] = '\0';
        value_json_node = true;
        break;
      case JSON_OBJECT:
        if (current_entry->type != ERT_MAPPER_ENTRY_TYPE_MAPPING) {
          ert_log_error("Cannot update mapper entry from JSON: entry '%s' type is not mapping, but %d",
              current_entry->name, current_entry->type);
          return -EINVAL;
        }

        if (current_entry->children == NULL) {
          ert_log_error("Cannot update mapper entry from JSON: entry '%s' does not have children", current_entry->name);
          return -EINVAL;
        }

        result = ert_mapper_update_from_json_internal(current_entry->children, value, update_context);
        if (result < 0) {
          return result;
        }
        if (result == 0) {
          result = ert_mapper_update_value(current_entry, NULL, update_context);
          if (result < 0) {
            ert_log_error("Error calling update callback for entry '%s'", current_entry->name);
            return -EINVAL;
          }
          if (result == 0) {
            no_update_callback_found = true;
          }
        }
        break;
      case JSON_ARRAY:
        ert_log_error("JSON arrays not supported: %s", key);
        return -EINVAL;
      default:
        ert_log_error("Unrecognized JSON value type: %d", json_typeof(value));
    }

    if (value_json_node) {
      result = ert_mapper_update_value(current_entry, string_value, update_context);
      if (result < 0) {
        ert_log_error("Invalid value for mapper entry '%s': %s", current_entry->name, string_value);
        return -EINVAL;
      }

      if (result == 0) {
        no_update_callback_found = true;
      }
    }
  }

  return no_update_callback_found ? 0 : 1;
}

int ert_mapper_update_from_json(ert_mapper_entry *root_entry, char *json, void *update_context)
{
  json_error_t error;

  json_t *json_obj = json_loads((const char *) json, 0, &error);
  if (json_obj == NULL) {
    ert_log_error("Error parsing JSON for mapper entry update at line %d, column %d: %s",
        error.line, error.column, error.text);
    return -EINVAL;
  }

  if (!json_is_object(json_obj)) {
    ert_log_error("Input is not a JSON object: %s", json);
  }

  int result = ert_mapper_update_from_json_internal(root_entry, json_obj, update_context);

  json_decref(json_obj);

  return result;
}
