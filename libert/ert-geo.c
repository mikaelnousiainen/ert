/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-geo.h"
#include "geodesic.h"

static const double WGS84_A = 6378137;
static const double WGS84_F = 1 / 298.257223563;

void ert_geo_inverse_wgs84(double lat1, double lon1, double lat2, double lon2,
    double *distance, double *azimuth1, double *azimuth2)
{
  struct geod_geodesic g;
  geod_init(&g, WGS84_A, WGS84_F);
  geod_inverse(&g, lat1, lon1, lat2, lon2, distance, azimuth1, azimuth2);
}