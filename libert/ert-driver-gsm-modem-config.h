/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_GSM_MODEM_CONFIG_H
#define __ERT_DRIVER_GSM_MODEM_CONFIG_H

#include "ert-mapper.h"
#include "ert-driver-gsm-modem.h"

ert_mapper_entry *ert_driver_gsm_modem_create_mappings(ert_driver_gsm_modem_config *modem_config);

#endif
