/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-color.h"

ert_color_rgb ert_color_value_to_rgb_brightness(uint32_t rgb_value, float brightness)
{
  ert_color_rgb rgb;

  rgb.red = (uint8_t) (((rgb_value >> 16) & 0xFF) * brightness);
  rgb.green = (uint8_t) (((rgb_value >> 8) & 0xFF) * brightness);
  rgb.blue = (uint8_t) ((rgb_value & 0xFF) * brightness);

  return rgb;
}

ert_color_rgb ert_color_value_to_rgb(uint32_t rgb_value)
{
  return ert_color_value_to_rgb_brightness(rgb_value, 1.0f);
}

uint32_t ert_color_rgb_to_value_brightness(ert_color_rgb rgb, float brightness)
{
  return (((uint32_t) (rgb.red * brightness) << 16)
      | ((uint32_t) (rgb.green * brightness) << 8)
        | (uint32_t) (rgb.blue * brightness));
}

uint32_t ert_color_rgb_to_value(ert_color_rgb rgb)
{
  return ert_color_rgb_to_value_brightness(rgb, 1.0f);
}
