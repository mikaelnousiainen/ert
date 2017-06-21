/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COLOR_H
#define __ERT_COLOR_H

#include "ert-common.h"

typedef struct _ert_color_rgb {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} ert_color_rgb;

ert_color_rgb ert_color_value_to_rgb_brightness(uint32_t rgb_value, float brightness);
ert_color_rgb ert_color_value_to_rgb(uint32_t rgb_value);
uint32_t ert_color_rgb_to_value_brightness(ert_color_rgb rgb, float brightness);
uint32_t ert_color_rgb_to_value(ert_color_rgb rgb);

#endif
