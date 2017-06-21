/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_GEO_H
#define __ERT_GEO_H

#include "ert-common.h"

void ert_geo_inverse_wgs84(double lat1, double lon1, double lat2, double lon2,
    double *distance, double *azimuth1, double *azimuth2);

#endif
