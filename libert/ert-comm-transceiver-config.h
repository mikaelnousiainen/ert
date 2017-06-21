/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_TRANSCEIVER_CONFIG_H
#define __ERT_COMM_TRANSCEIVER_CONFIG_H

#include "ert-mapper.h"
#include "ert-comm-transceiver.h"

ert_mapper_entry *ert_comm_transceiver_create_mappings(ert_comm_transceiver_config *config);

#endif
