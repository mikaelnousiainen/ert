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
#include <errno.h>
#include <yaml.h>

#include "ert-log.h"
#include "ert-yaml.h"

typedef enum _ert_yaml_parser_mode {
  MODE_ROOT = 0,
  MODE_MAPPING = 1,
  MODE_SEQUENCE = 2,
} ert_yaml_parser_mode;

typedef enum _ert_yaml_parser_scalar_type {
  SCALAR_TYPE_NONE = 0,
  SCALAR_TYPE_KEY = 1,
  SCALAR_TYPE_VALUE = 2,
} ert_yaml_parser_scalar_type;

struct _ert_yaml_parser {
  yaml_parser_t parser;
};

int ert_yaml_parse_file(FILE *file, ert_mapper_entry *root_entry)
{
  ert_yaml_parser *yaml_parser;
  int result = ert_yaml_parser_open_file(file, &yaml_parser);
  if (result < 0) {
    return result;
  }

  result = ert_yaml_parse(yaml_parser, root_entry);

  ert_yaml_parser_close(yaml_parser);

  return result;
}

int ert_yaml_parser_open_file(FILE *file, ert_yaml_parser **yaml_parser_rcv)
{
  int result;

  ert_yaml_parser *yaml_parser = calloc(1, sizeof(ert_yaml_parser));
  if (yaml_parser == NULL) {
    ert_log_fatal("Error allocating memory for YAML parser struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_first;
  }

  result = yaml_parser_initialize(&yaml_parser->parser);
  if (!result) {
    ert_log_error("Error initializing YAML parser");
    result = -EIO;
    goto error_yaml_parser_malloc;
  }

  yaml_parser_set_input_file(&yaml_parser->parser, file);

  *yaml_parser_rcv = yaml_parser;

  return 0;

  error_yaml_parser_malloc:
  free(yaml_parser);

  error_first:

  return result;
}

static int ert_yaml_parse_internal(ert_yaml_parser *yaml_parser, ert_mapper_entry *entry,
    ert_yaml_parser_mode mode, yaml_event_type_t end_event_type)
{
  yaml_parser_t *parser = &yaml_parser->parser;
  yaml_event_t event;
  bool error = false;
  int result;

  ert_log_debug("YAML scope mode: %d", mode);

  ert_yaml_parser_scalar_type scalar_type =
      (mode == MODE_MAPPING) ? SCALAR_TYPE_KEY : SCALAR_TYPE_NONE;
  ert_mapper_entry *mapping_entry = NULL;

  do {
    result = yaml_parser_parse(parser, &event);
    if (!result) {
      ert_log_error("Error parsing YAML: error code %d", parser->error);
      return -EBADMSG;
    }

    switch (event.type) {
      case YAML_NO_EVENT:
        ert_log_debug("YAML_NO_EVENT");
        break;

      case YAML_STREAM_START_EVENT:
        ert_log_debug("YAML_STREAM_START_EVENT");
        break;
      case YAML_STREAM_END_EVENT:
        ert_log_debug("YAML_STREAM_END_EVENT");
        break;

      case YAML_DOCUMENT_START_EVENT:
        // TODO: report error if another document begins
        ert_log_debug("YAML_DOCUMENT_START_EVENT");
        break;
      case YAML_DOCUMENT_END_EVENT:
        ert_log_debug("YAML_DOCUMENT_END_EVENT");
        break;
      case YAML_SEQUENCE_START_EVENT:
        ert_log_debug("YAML_SEQUENCE_START_EVENT");

        switch (mode) {
          case MODE_MAPPING:
            switch (scalar_type) {
              case SCALAR_TYPE_VALUE:
                result = ert_yaml_parse_internal(yaml_parser, mapping_entry, MODE_SEQUENCE, YAML_SEQUENCE_END_EVENT);
                if (result < 0) {
                  error = true;
                }
                scalar_type = SCALAR_TYPE_NONE;
                break;
              default:
                ert_log_error("YAML sequence only accepted as mapping value");
                error = true;
                break;
            }
            break;
          case MODE_SEQUENCE:
            ert_log_error("Cannot have YAML sequence inside sequence");
            error = true;
            break;
          case MODE_ROOT:
            ert_log_error("Cannot have YAML sequence in document root");
            error = true;
            break;
          default:
            ert_log_error("Unknown parser mode: %d", mode);
            error = true;
        }
        break;
      case YAML_SEQUENCE_END_EVENT:
        ert_log_debug("YAML_SEQUENCE_END_EVENT");
        break;
      case YAML_MAPPING_START_EVENT:
        ert_log_debug("YAML_MAPPING_START_EVENT");

        switch (mode) {
          case MODE_MAPPING:
          case MODE_SEQUENCE:
            switch (scalar_type) {
              case SCALAR_TYPE_VALUE:
                result = ert_yaml_parse_internal(yaml_parser, mapping_entry, MODE_MAPPING, YAML_MAPPING_END_EVENT);
                if (result < 0) {
                  error = true;
                }
                scalar_type = SCALAR_TYPE_KEY;
                break;
              default:
                ert_log_error("YAML mapping only accepted as mapping value");
                error = true;
                break;
            }
            break;
          case MODE_ROOT:
            switch (scalar_type) {
              case SCALAR_TYPE_NONE: {
                if (entry == NULL) {
                  ert_log_error("No mapper entry for document root");
                  error = true;
                  break;
                }
                if (entry->type != ERT_MAPPER_ENTRY_TYPE_MAPPING) {
                  ert_log_error("Mapper entry type for document root is not: mapping");
                  error = true;
                  break;
                }

                result = ert_yaml_parse_internal(yaml_parser, entry, MODE_MAPPING, YAML_MAPPING_END_EVENT);
                if (result < 0) {
                  error = true;
                }
                break;
              }
              default:
                ert_log_error("YAML mapping only accepted in root without scalar values");
                error = true;
                break;
            }
            break;
          default:
            ert_log_error("Unknown parser mode: %d", mode);
            error = true;
        }
        break;
      case YAML_MAPPING_END_EVENT:
        ert_log_debug("YAML_MAPPING_END_EVENT");
        scalar_type = SCALAR_TYPE_NONE;
        break;

      case YAML_ALIAS_EVENT:
        ert_log_debug("YAML_ALIAS_EVENT: anchor %s", event.data.alias.anchor);
        break;
      case YAML_SCALAR_EVENT: {
        yaml_char_t *scalar_value = event.data.scalar.value;
        ert_log_debug("YAML_SCALAR_EVENT: value=%s tag=%s anchor)%s)",
            scalar_value, event.data.scalar.tag, event.data.scalar.anchor);

        switch (mode) {
          case MODE_MAPPING:
            switch (scalar_type) {
              case SCALAR_TYPE_KEY:
                ert_log_debug("YAML_SCALAR_EVENT: key=%s", scalar_value);
                if (entry != NULL) {
                  mapping_entry = ert_mapper_find_entry(entry->children, (char *) scalar_value);
                  if (mapping_entry == NULL) {
                    ert_log_warn("Unknown mapping key: '%s' for parent '%s'", scalar_value, entry->name);
                  }
                }

                scalar_type = SCALAR_TYPE_VALUE;
                break;
              case SCALAR_TYPE_VALUE:
                ert_log_debug("YAML_SCALAR_EVENT: value=%s", scalar_value);

                if (mapping_entry != NULL) {
                  ert_log_debug("Parsed YAML value: '%s' = '%s'", mapping_entry->name, scalar_value);

                  result = ert_mapper_set_value_from_string(mapping_entry, (char *) scalar_value);
                  if (result < 0) {
                    ert_log_error("Invalid scalar value for entry '%s': %s", mapping_entry->name, scalar_value);
                    error = true;
                  }
                }

                scalar_type = SCALAR_TYPE_KEY;
                mapping_entry = NULL;
                break;
              default:
                ert_log_error("Invalid parser state for scalar value in mapping");
                error = true;
            }
            break;
          case MODE_SEQUENCE:
            // TODO: add support for sequences
            ert_log_error("No scalar values allowed in sequence");
            error = true;
            break;
          case MODE_ROOT:
            ert_log_error("No scalar values allowed in root");
            error = true;
            break;
          default:
            ert_log_error("Unknown parser mode for scalar event: %d", mode);
            error = true;
        }
        break;
      }
    }

    if (event.type != end_event_type) {
      yaml_event_delete(&event);
    }
  } while (!error && event.type != end_event_type);

  if (event.type == end_event_type) {
    yaml_event_delete(&event);
  }

  return error ? -EBADMSG : 0;
}

int ert_yaml_parse(ert_yaml_parser *yaml_parser, ert_mapper_entry *root_children_mapping)
{
  // The parser needs a dummy root entry to work correctly
  ert_mapper_entry root_entry[] = {
      {
          .name = "root",
          .type = ERT_MAPPER_ENTRY_TYPE_MAPPING,
          .children = root_children_mapping,
      },
      {
          .type = ERT_MAPPER_ENTRY_TYPE_NONE,
      },
  };

  return ert_yaml_parse_internal(yaml_parser, root_entry, MODE_ROOT, YAML_STREAM_END_EVENT);
}

int ert_yaml_parser_close(ert_yaml_parser *yaml_parser)
{
  yaml_parser_delete(&yaml_parser->parser);
  free(yaml_parser);

  return 0;
}
