/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_IMAGE_METADATA_H
#define __ERT_IMAGE_METADATA_H

#include "ert-common.h"
#include <time.h>
#include <limits.h>

#define ERT_IMAGE_METADATA_HEADER_ID "EIM1"
#define ERT_IMAGE_METADATA_SEARCH_BYTE_RANGE 50

typedef struct _ert_image_metadata {
  uint32_t id;
  struct timespec timestamp;
  char format[9];

  char filename[PATH_MAX];
  char full_path_filename[PATH_MAX];
} ert_image_metadata;

int ert_image_metadata_msgpack_serialize(ert_image_metadata *metadata, uint32_t *length, uint8_t **data_rcv);
int ert_image_metadata_msgpack_serialize_with_header(ert_image_metadata *metadata, uint32_t *length, uint8_t **data_rcv);
int ert_image_metadata_msgpack_deserialize(uint32_t length, uint8_t *data, ert_image_metadata *metadata);
int ert_image_metadata_msgpack_deserialize_with_header(uint32_t length, uint8_t *data, ert_image_metadata *metadata);
int ert_image_metadata_json_serialize(ert_image_metadata *metadata, uint8_t entry_type,
    uint32_t *length, uint8_t **data_rcv);

#endif
