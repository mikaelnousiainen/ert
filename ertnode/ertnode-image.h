/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERTNODE_IMAGE_H
#define __ERTNODE_IMAGE_H

#include <stdint.h>
#include <stdbool.h>

#include "ert-gps.h"

int ert_node_capture_image(const char *command, const char *image_filename,
    int16_t quality, bool hflip, bool vflip, ert_gps_data *gps_data);
int ert_node_resize_image(const char *command, const char *output_image_filename,
    const char *input_image_filename, int16_t width, int16_t height, int16_t quality);

#endif
