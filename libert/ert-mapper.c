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
#include <errno.h>
#include <inttypes.h>

#include "ert-mapper.h"
#include "ert-log.h"

int ert_mapper_get_value_as_string(ert_mapper_entry *entry, size_t length, char *string)
{
  ert_mapper_entry_type type = entry->type;
  void *value = entry->value;

  if (entry->enum_entries != NULL) {
    return ert_mapper_get_enum_name_for_value(entry, length, string);
  }

  switch (type) {
    case ERT_MAPPER_ENTRY_TYPE_STRING:
      snprintf(string, length, "%s", (char *) value);
      break;
    case ERT_MAPPER_ENTRY_TYPE_BOOLEAN:
      snprintf(string, length, "%d", *((bool *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT8:
      snprintf(string, length, "%d", *((int8_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT16:
      snprintf(string, length, "%d", *((int16_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT32:
      snprintf(string, length, "%d", *((int32_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT64:
      snprintf(string, length, "%" PRId64, *((int64_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT8:
      snprintf(string, length, "%d", *((uint8_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT16:
      snprintf(string, length, "%d", *((uint16_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT32:
      snprintf(string, length, "%d", *((uint32_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT64:
      snprintf(string, length, "%" PRIu64, *((int64_t *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_FLOAT:
      snprintf(string, length, "%f", *((float *) value));
      break;
    case ERT_MAPPER_ENTRY_TYPE_DOUBLE:
      snprintf(string, length, "%f", *((double *) value));
      break;
    default:
      ert_log_error("Invalid mapper entry type %d for scalar value", type);
      return -EINVAL;
  }

  return 0;
}

ert_mapper_enum_entry *ert_mapper_find_enum_entry_for_value(ert_mapper_enum_entry *entries, size_t size, uint8_t *value)
{
  ert_mapper_enum_entry *entry;
  size_t index = 0;

  do {
    entry = &entries[index];

    if (entry->name != NULL && memcmp(entry->value, value, size) == 0) {
      return entry;
    }

    index++;
  } while (entry->name != NULL);

  return NULL;
}

int ert_mapper_get_enum_name_for_value(ert_mapper_entry *entry, size_t name_length, char *name)
{
  if (entry->enum_entries == NULL) {
    ert_log_error("Entry is not an enum entry: %s", entry->name);
    return -EINVAL;
  }

  ert_mapper_entry_type type = entry->type;
  size_t field_size = 0;

  switch (type) {
    case ERT_MAPPER_ENTRY_TYPE_STRING:
      field_size = strlen(entry->value);
      break;
    case ERT_MAPPER_ENTRY_TYPE_BOOLEAN:
      field_size = sizeof(bool);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT8:
      field_size = sizeof(int8_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT16:
      field_size = sizeof(int16_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT32:
      field_size = sizeof(int32_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT64:
      field_size = sizeof(int64_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT8:
      field_size = sizeof(uint8_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT16:
      field_size = sizeof(uint16_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT32:
      field_size = sizeof(uint32_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT64:
      field_size = sizeof(uint64_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_FLOAT:
      field_size = sizeof(float);
      break;
    case ERT_MAPPER_ENTRY_TYPE_DOUBLE:
      field_size = sizeof(double);
      break;
    default:
      ert_log_error("Invalid mapper entry type %d for scalar value", type);
      return -EINVAL;
  }

  ert_mapper_enum_entry *enum_entry = ert_mapper_find_enum_entry_for_value(entry->enum_entries,
      field_size, entry->value);
  if (enum_entry == NULL) {
    ert_log_error("Unknown value for mapper entry '%s'", entry->name);
    return -EINVAL;
  }

  strncpy(name, enum_entry->name, name_length);

  // Ensure string is NULL-terminated
  name[name_length - 1] = '\0';

  return 0;
}

ert_mapper_enum_entry *ert_mapper_find_enum_entry_for_name(ert_mapper_enum_entry *entries, char *name)
{
  ert_mapper_enum_entry *entry;
  size_t index = 0;

  do {
    entry = &entries[index];

    if (entry->name != NULL && strcmp(name, entry->name) == 0) {
      return entry;
    }

    index++;
  } while (entry->name != NULL);

  return NULL;
}

int ert_mapper_set_value_by_enum_name(ert_mapper_entry *entry, char *value)
{
  if (entry->enum_entries == NULL) {
    return 0;
  }

  ert_mapper_enum_entry *enum_entry = ert_mapper_find_enum_entry_for_name(entry->enum_entries, value);
  if (enum_entry == NULL) {
    ert_log_error("Unknown enum value for mapper entry '%s': '%s'", entry->name, value);
    return -EINVAL;
  }

  ert_mapper_entry_type type = entry->type;
  size_t field_size = 0;

  switch (type) {
    case ERT_MAPPER_ENTRY_TYPE_STRING:
      if (entry->maximum_length == 0) {
        ert_log_warn("Maximum length of mapper entry '%s' is zero", entry->name);
        break;
      }

      strncpy(entry->value, enum_entry->value, entry->maximum_length);

      // Ensure string is NULL-terminated
      ((char *) entry->value)[entry->maximum_length - 1] = '\0';

      return 0;
    case ERT_MAPPER_ENTRY_TYPE_BOOLEAN:
      field_size = sizeof(bool);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT8:
      field_size = sizeof(int8_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT16:
      field_size = sizeof(int16_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT32:
      field_size = sizeof(int32_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_INT64:
      field_size = sizeof(int64_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT8:
      field_size = sizeof(uint8_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT16:
      field_size = sizeof(uint16_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT32:
      field_size = sizeof(uint32_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_UINT64:
      field_size = sizeof(uint64_t);
      break;
    case ERT_MAPPER_ENTRY_TYPE_FLOAT:
      field_size = sizeof(float);
      break;
    case ERT_MAPPER_ENTRY_TYPE_DOUBLE:
      field_size = sizeof(double);
      break;
    default:
      ert_log_error("Invalid mapper entry type %d for scalar value", type);
      return -EINVAL;
  }

  memcpy(entry->value, enum_entry->value, field_size);

  return 0;
}

ert_mapper_entry *ert_mapper_allocate_single(ert_mapper_entry *entries)
{
  ert_mapper_entry *entry;
  size_t index = 0;

  do {
    entry = &entries[index];
    index++;
  } while (entry->type != ERT_MAPPER_ENTRY_TYPE_NONE);

  size_t count = index;
  size_t allocation_size = count * sizeof(ert_mapper_entry);

  ert_mapper_entry *allocation = malloc(allocation_size);
  if (allocation == NULL) {
    ert_log_fatal("Error allocating memory for mapper entries: %s", strerror(errno));
    return NULL;
  }

  memcpy(allocation, entries, allocation_size);

  return allocation;
}

ert_mapper_entry *ert_mapper_allocate(ert_mapper_entry *entries)
{
  ert_mapper_entry *allocated_entries = ert_mapper_allocate_single(entries);
  if (allocated_entries == NULL) {
    return NULL;
  }

  ert_mapper_entry *allocated_entry;
  size_t index = 0;

  do {
    allocated_entry = &allocated_entries[index];

    if ((allocated_entry->type == ERT_MAPPER_ENTRY_TYPE_MAPPING
        || allocated_entry->type == ERT_MAPPER_ENTRY_TYPE_SEQUENCE)
        && allocated_entry->children != NULL && !allocated_entry->children_allocated) {
      allocated_entry->children = ert_mapper_allocate(allocated_entry->children);

      if (allocated_entry->children != NULL) {
        allocated_entry->children_allocated = true;
      }
    }

    index++;
  } while (allocated_entry->type != ERT_MAPPER_ENTRY_TYPE_NONE);

  return allocated_entries;
}

bool ert_mapper_foreach(ert_mapper_entry *entries, void *context, uint16_t level, ert_mapper_visit_func visit)
{
  ert_mapper_entry *entry;
  size_t index = 0;

  do {
    entry = &entries[index];

    if ((entry->type == ERT_MAPPER_ENTRY_TYPE_MAPPING || entry->type == ERT_MAPPER_ENTRY_TYPE_SEQUENCE)) {
      if (entry->children != NULL) {
        bool cont = visit(entry, level, context);
        if (!cont) {
          return false;
        }

        cont = ert_mapper_foreach(entry->children, context, level + 1, visit);
        if (!cont) {
          return false;
        }
      }
    } else if (entry->type != ERT_MAPPER_ENTRY_TYPE_NONE) {
      bool cont = visit(entry, level, context);
      if (!cont) {
        return false;
      }
    }

    index++;
  } while (entry->type != ERT_MAPPER_ENTRY_TYPE_NONE);

  return true;
}

void ert_mapper_deallocate(ert_mapper_entry *entries)
{
  ert_mapper_entry *entry;
  size_t index = 0;

  do {
    entry = &entries[index];

    if ((entry->type == ERT_MAPPER_ENTRY_TYPE_MAPPING || entry->type == ERT_MAPPER_ENTRY_TYPE_SEQUENCE)
        && entry->children != NULL) {

      ert_mapper_deallocate(entry->children);
      entry->children = NULL;
    }

    index++;
  } while (entry->type != ERT_MAPPER_ENTRY_TYPE_NONE);

  free(entries);

  return;
}

ert_mapper_entry *ert_mapper_find_entry(ert_mapper_entry *entries, char *name)
{
  ert_mapper_entry *entry;
  ert_mapper_entry_type type;
  size_t index = 0;

  do {
    entry = &entries[index];
    type = entry->type;

    if (entry->name != NULL && strcmp(name, entry->name) == 0) {
      return entry;
    }

    index++;
  } while (type != ERT_MAPPER_ENTRY_TYPE_NONE);

  return NULL;
}

ert_mapper_entry *ert_mapper_find_entry_recursive(ert_mapper_entry *entries, char *names[])
{
  if (entries == NULL || names == NULL || names[0] == NULL) {
    return NULL;
  }

  ert_mapper_entry *entry = ert_mapper_find_entry(entries, names[0]);
  if (entry == NULL) {
    ert_log_error("Mapper entry '%s' does not exist", names[0]);
    return NULL;
  }

  if (names[1] == NULL) {
    return entry;
  }

  if (entry->type != ERT_MAPPER_ENTRY_TYPE_MAPPING) {
    ert_log_error("Mapper entry '%s' type is not mapping", entry->name);
    return NULL;
  }
  if (entry->children == NULL) {
    ert_log_error("Mapper entry '%s' type is mapping, but does not have children", entry->name);
    return NULL;
  }

  return ert_mapper_find_entry_recursive(entry->children, &names[1]);
}

int ert_mapper_convert_to_int32(char *string, int32_t *value_rcv)
{
  char *endptr = NULL;

  errno = 0;
  int32_t value = (int32_t) strtol(string, &endptr, 0);
  if (errno != 0) {
    return -EINVAL;
  }
  if (endptr != NULL && (*endptr) != '\0') {
    return -EINVAL;
  }

  *value_rcv = value;

  return 0;
}

int ert_mapper_convert_to_uint32(char *string, uint32_t *value_rcv)
{
  char *endptr = NULL;

  errno = 0;
  uint32_t value = (uint32_t) strtoul(string, &endptr, 0);
  if (errno != 0) {
    return -EINVAL;
  }
  if (endptr != NULL && (*endptr) != '\0') {
    return -EINVAL;
  }

  *value_rcv = value;

  return 0;
}

int ert_mapper_convert_to_int64(char *string, int64_t *value_rcv)
{
  char *endptr = NULL;

  errno = 0;
  int64_t value = (int64_t) strtoll(string, &endptr, 0);
  if (errno != 0) {
    return -EINVAL;
  }
  if (endptr != NULL && (*endptr) != '\0') {
    return -EINVAL;
  }

  *value_rcv = value;

  return 0;
}

int ert_mapper_convert_to_uint64(char *string, uint64_t *value_rcv)
{
  char *endptr = NULL;

  errno = 0;
  uint64_t value = (uint64_t) strtoull(string, &endptr, 0);
  if (errno != 0) {
    return -EINVAL;
  }
  if (endptr != NULL && (*endptr) != '\0') {
    return -EINVAL;
  }

  *value_rcv = value;

  return 0;
}

int ert_mapper_convert_to_float(char *string, float *value_rcv)
{
  char *endptr = NULL;

  errno = 0;
  float value = strtof(string, &endptr);
  if (errno != 0) {
    return -EINVAL;
  }
  if (endptr != NULL && (*endptr) != '\0') {
    return -EINVAL;
  }

  *value_rcv = value;

  return 0;
}

int ert_mapper_convert_to_double(char *string, double *value_rcv)
{
  char *endptr = NULL;

  errno = 0;
  double value = strtod(string, &endptr);
  if (errno != 0) {
    return -EINVAL;
  }
  if (endptr != NULL && (*endptr) != '\0') {
    return -EINVAL;
  }

  *value_rcv = value;

  return 0;
}

int ert_mapper_set_value_from_string(ert_mapper_entry *entry, char *value)
{
  ert_mapper_entry_type type = entry->type;

  if (entry->value == NULL) {
    ert_log_warn("Mapper entry '%s' value pointer is NULL, skipping conversion", entry->name);
    return 0;
  }

  if (entry->enum_entries != NULL) {
    return ert_mapper_set_value_by_enum_name(entry, value);
  }

  int result = 0;

  switch (type) {
    case ERT_MAPPER_ENTRY_TYPE_STRING:
      if (entry->maximum_length == 0) {
        ert_log_warn("Maximum length of mapper entry '%s' is zero", entry->name);
        break;
      }

      strncpy(entry->value, value, entry->maximum_length);

      // Ensure string is NULL-terminated
      ((char *) entry->value)[entry->maximum_length - 1] = '\0';

      break;
    case ERT_MAPPER_ENTRY_TYPE_BOOLEAN: {
      bool boolean_value;

      if (strcasecmp(value, "true") == 0
          || strcasecmp(value, "on") == 0
          || strcasecmp(value, "yes") == 0
          || strcasecmp(value, "y") == 0) {
        boolean_value = true;
      } else if (strcasecmp(value, "false") == 0
                 || strcasecmp(value, "off") == 0
                 || strcasecmp(value, "no") == 0
                 || strcasecmp(value, "n") == 0) {
        boolean_value = false;
      } else {
        ert_log_error("Invalid boolean value for mapper entry '%s': %s", entry->name, value);
        return -EINVAL;
      }

      *((bool *) entry->value) = boolean_value;
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_INT8: {
      int32_t number;
      result = ert_mapper_convert_to_int32(value, &number);
      if (result == 0) {
        *((int8_t *) entry->value) = (int8_t) number;
      }
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_INT16: {
      int32_t number;
      result = ert_mapper_convert_to_int32(value, &number);
      if (result == 0) {
        *((int16_t *) entry->value) = (int16_t) number;
      }
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_INT32: {
      result = ert_mapper_convert_to_int32(value, (int32_t *) entry->value);
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_INT64: {
      result = ert_mapper_convert_to_int64(value, (int64_t *) entry->value);
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_UINT8: {
      uint32_t number;
      result = ert_mapper_convert_to_uint32(value, &number);
      if (result == 0) {
        *((uint8_t *) entry->value) = (uint8_t) number;
      }
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_UINT16: {
      uint32_t number;
      result = ert_mapper_convert_to_uint32(value, &number);
      if (result == 0) {
        *((uint16_t *) entry->value) = (uint16_t) number;
      }
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_UINT32: {
      result = ert_mapper_convert_to_uint32(value, (uint32_t *) entry->value);
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_UINT64: {
      result = ert_mapper_convert_to_uint64(value, (uint64_t *) entry->value);
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_FLOAT: {
      result = ert_mapper_convert_to_float(value, (float *) entry->value);
      break;
    }
    case ERT_MAPPER_ENTRY_TYPE_DOUBLE: {
      result = ert_mapper_convert_to_double(value, (double *) entry->value);
      break;
    }
    default:
      ert_log_error("Invalid mapper entry type %d for scalar value", type);
      return -EINVAL;
  }

  return result;
}

int ert_mapper_update_value(ert_mapper_entry *entry, char *value, void *update_context)
{
  if (value != NULL) {
    int result = ert_mapper_set_value_from_string(entry, value);
    if (result < 0) {
      ert_log_error("Error updating value of entry '%s', result %d", entry->name, result);
      return result;
    }
  }

  if (entry->updated != NULL) {
    int result = entry->updated(entry, update_context);
    if (result < 0) {
      ert_log_error("Error running update callback for entry '%s', result %d", entry->name, result);
      return result;
    }

    return 1;
  }

  return 0;
}

int ert_mapper_update_value_by_name(ert_mapper_entry *root_entry, char *entry_names[], char *value, void *update_context)
{
  ert_mapper_entry *entry = ert_mapper_find_entry_recursive(root_entry, entry_names);
  if (entry == NULL) {
    return -EINVAL;
  }

  return ert_mapper_update_value(entry, value, update_context);
}

bool ert_mapper_log_entry_visit(ert_mapper_entry *entry, uint16_t level, void *context)
{
  if (entry->type == ERT_MAPPER_ENTRY_TYPE_MAPPING || entry->type == ERT_MAPPER_ENTRY_TYPE_SEQUENCE) {
    ert_log_info("%*c%s:", level * 2, ' ', entry->name);
  } else {
    char value[1024];
    ert_mapper_get_value_as_string(entry, 1024, value);
    ert_log_info("%*c%s: %s", level * 2, ' ', entry->name, value);
  }

  return true;
}

void ert_mapper_log_entries(ert_mapper_entry *root_entry)
{
  ert_mapper_foreach(root_entry, NULL, 0, ert_mapper_log_entry_visit);
}
