/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTGATEWAY_CONFIG_H
#define __ERTGATEWAY_CONFIG_H

#include "ertgateway.h"

ert_mapper_entry *ert_gateway_configuration_mapper_create(ert_gateway_config *config);
int ert_gateway_read_configuration(ert_gateway_config *config, char *config_file_name);

#endif