/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_HANDLER_IMAGE_HELPERS_H
#define __ERT_HANDLER_IMAGE_HELPERS_H

#include "ert-common.h"

int ert_image_format_filename(const char *image_format, uint32_t image_index,
    struct timespec *timestamp, char *specifier, char *image_filename_rcv);
void ert_image_add_path(const char *image_path, const char *image_filename, char *image_full_path_filename_rcv);

#endif
