/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_YAML_H
#define __ERT_YAML_H

#include "ert-common.h"
#include "ert-mapper.h"
#include <stdio.h>

typedef struct _ert_yaml_parser ert_yaml_parser;

int ert_yaml_parser_open_file(FILE *file, ert_yaml_parser **yaml_parser_rcv);
int ert_yaml_parse(ert_yaml_parser *yaml_parser, ert_mapper_entry *root_children_mapping);
int ert_yaml_parser_close(ert_yaml_parser *yaml_parser);
int ert_yaml_parse_file(FILE *file, ert_mapper_entry *entries);

#endif
