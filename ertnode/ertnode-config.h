/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTNODE_CONFIG_H
#define __ERTNODE_CONFIG_H

#include "ertnode.h"
#include "ert-mapper.h"

ert_mapper_entry *ert_node_configuration_mapper_create(ert_node_config *config);
int ert_node_read_configuration(ert_node_config *config, char *config_file_name);

#endif
