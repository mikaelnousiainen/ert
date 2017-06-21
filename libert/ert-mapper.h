/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_MAPPER_H
#define __ERT_MAPPER_H

#include "ert-common.h"

typedef enum _ert_mapper_entry_type {
  ERT_MAPPER_ENTRY_TYPE_NONE = 0,
  ERT_MAPPER_ENTRY_TYPE_MAPPING = 1,
  ERT_MAPPER_ENTRY_TYPE_SEQUENCE = 2,
  ERT_MAPPER_ENTRY_TYPE_STRING = 0x11,
  ERT_MAPPER_ENTRY_TYPE_BOOLEAN,
  ERT_MAPPER_ENTRY_TYPE_INT8 = 0x21,
  ERT_MAPPER_ENTRY_TYPE_INT16,
  ERT_MAPPER_ENTRY_TYPE_INT32,
  ERT_MAPPER_ENTRY_TYPE_INT64,
  ERT_MAPPER_ENTRY_TYPE_UINT8,
  ERT_MAPPER_ENTRY_TYPE_UINT16,
  ERT_MAPPER_ENTRY_TYPE_UINT32,
  ERT_MAPPER_ENTRY_TYPE_UINT64,
  ERT_MAPPER_ENTRY_TYPE_FLOAT = 0x31,
  ERT_MAPPER_ENTRY_TYPE_DOUBLE,
} ert_mapper_entry_type;

typedef struct _ert_mapper_enum_entry {
  char *name;
  void *value;
} ert_mapper_enum_entry;

typedef struct _ert_mapper_entry ert_mapper_entry;

typedef int (*ert_mapper_entry_updated_func)(ert_mapper_entry *entry, void *context);

struct _ert_mapper_entry {
  ert_mapper_entry_type type;
  size_t maximum_length;
  char *name;
  void *value;
  ert_mapper_enum_entry *enum_entries;

  bool children_allocated;
  struct _ert_mapper_entry *children;

  ert_mapper_entry_updated_func updated;
};

typedef bool (*ert_mapper_visit_func)(ert_mapper_entry *entry, uint16_t level, void *context);

int ert_mapper_get_value_as_string(ert_mapper_entry *entry, size_t length, char *string);

ert_mapper_enum_entry *ert_mapper_find_enum_entry_for_value(ert_mapper_enum_entry *entries, size_t size, uint8_t *value);
int ert_mapper_get_enum_name_for_value(ert_mapper_entry *entry, size_t name_length, char *name);
ert_mapper_enum_entry *ert_mapper_find_enum_entry_for_name(ert_mapper_enum_entry *entries, char *name);
int ert_mapper_set_value_by_enum_name(ert_mapper_entry *entry, char *value);

ert_mapper_entry *ert_mapper_allocate_single(ert_mapper_entry *entries);
ert_mapper_entry *ert_mapper_allocate(ert_mapper_entry *entries);
bool ert_mapper_foreach(ert_mapper_entry *entries, void *context, uint16_t level, ert_mapper_visit_func visit);
void ert_mapper_deallocate(ert_mapper_entry *entries);

ert_mapper_entry *ert_mapper_find_entry(ert_mapper_entry *entries, char *name);
ert_mapper_entry *ert_mapper_find_entry_recursive(ert_mapper_entry *entries, char *names[]);

int ert_mapper_convert_to_int32(char *string, int32_t *value_rcv);
int ert_mapper_convert_to_uint32(char *string, uint32_t *value_rcv);
int ert_mapper_convert_to_int64(char *string, int64_t *value_rcv);
int ert_mapper_convert_to_uint64(char *string, uint64_t *value_rcv);
int ert_mapper_convert_to_float(char *string, float *value_rcv);
int ert_mapper_convert_to_double(char *string, double *value_rcv);
int ert_mapper_set_value_from_string(ert_mapper_entry *entry, char *value);

int ert_mapper_update_value(ert_mapper_entry *entry, char *value, void *update_context);
int ert_mapper_update_value_by_name(ert_mapper_entry *root_entry, char *entry_names[], char *value, void *update_context);

bool ert_mapper_log_entry_visit(ert_mapper_entry *entry, uint16_t level, void *context);
void ert_mapper_log_entries(ert_mapper_entry *root_entry);

#endif
