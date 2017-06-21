/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string.h>
#include <errno.h>
#include <msgpack.h>
#include <jansson.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-image-metadata.h"
#include "ert-msgpack-helpers.h"

const char *ert_image_metadata_header_id = ERT_IMAGE_METADATA_HEADER_ID;

static int ert_image_metadata_msgpack_serialize_root(msgpack_packer *pk, ert_image_metadata *metadata)
{
  msgpack_pack_map(pk, 3);

  ert_msgpack_serialize_map_key(pk, "i");
  msgpack_pack_uint32(pk, metadata->id);

  ert_msgpack_serialize_map_key(pk, "t");
  msgpack_pack_uint64(pk,
      ((uint64_t) metadata->timestamp.tv_sec) * 1000LL + ((uint64_t) metadata->timestamp.tv_nsec) / 1000000LL);

  ert_msgpack_serialize_map_key_string(pk, "f", metadata->format);

  return 0;
}

int ert_image_metadata_msgpack_serialize(ert_image_metadata *metadata, uint32_t *length, uint8_t **data_rcv)
{
  int result;

  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);

  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  result = ert_image_metadata_msgpack_serialize_root(&pk, metadata);
  if (result < 0) {
    msgpack_sbuffer_destroy(&sbuf);
    ert_log_error("Error serializing image metadata to MsgPack, result %d", result);
    return result;
  }

  uint8_t *data = calloc(1, sbuf.size);
  if (data == NULL) {
    msgpack_sbuffer_destroy(&sbuf);
    ert_log_fatal("Error allocating memory for serialized MsgPack data: %s", strerror(errno));
    return -ENOMEM;
  }

  memcpy(data, sbuf.data, sbuf.size);

  *length = (uint32_t) sbuf.size;
  *data_rcv = data;

  msgpack_sbuffer_destroy(&sbuf);

  return 0;
}

int ert_image_metadata_msgpack_serialize_with_header(ert_image_metadata *metadata, uint32_t *length, uint8_t **data_rcv)
{
  uint32_t image_metadata_serialized_length;
  uint8_t *image_metadata_serialized_data;
  int result = ert_image_metadata_msgpack_serialize(metadata, &image_metadata_serialized_length, &image_metadata_serialized_data);
  if (result < 0) {
    ert_log_error("ert_image_metadata_msgpack_serialize failed with result %d", result);
    return result;
  }

  size_t header_length = strlen(ert_image_metadata_header_id);

  uint8_t *image_metadata_serialized_data_with_header = realloc(image_metadata_serialized_data,
      header_length + image_metadata_serialized_length);

  if (image_metadata_serialized_data_with_header == NULL) {
    ert_log_fatal("Error allocating memory for image metadata with header: %s", strerror(errno));
    free(image_metadata_serialized_data);
    return -ENOMEM;
  }

  memmove(image_metadata_serialized_data_with_header + header_length, image_metadata_serialized_data_with_header,
      image_metadata_serialized_length);
  memcpy(image_metadata_serialized_data_with_header, ert_image_metadata_header_id, header_length);

  *data_rcv = image_metadata_serialized_data_with_header;
  *length = (uint32_t) header_length + image_metadata_serialized_length;

  return 0;
}

static int ert_image_metadata_msgpack_deserialize_root(ert_image_metadata *metadata, msgpack_object *obj)
{
  if (obj->type != MSGPACK_OBJECT_MAP) {
    ert_log_error("Image metadata root object is not a map");
    return -EINVAL;
  }

  msgpack_object_map *map_obj = &obj->via.map;

  int result;

  for (uint32_t i = 0; i < map_obj->size; i++) {
    msgpack_object_kv *kv_obj = &map_obj->ptr[i];

    if (ert_msgpack_string_equals("i", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Image metadata 'i' object is not an integer");
        return -EINVAL;
      }

      metadata->id = (uint32_t) kv_obj->val.via.u64;
    }

    if (ert_msgpack_string_equals("t", &kv_obj->key.via.str)) {
      if (kv_obj->val.type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        ert_log_error("Image metadata 't' object is not an integer");
        return -EINVAL;
      }

      metadata->timestamp.tv_sec = kv_obj->val.via.u64 / 1000LL;
      metadata->timestamp.tv_nsec = (kv_obj->val.via.u64 % 1000LL) * 1000000LL;
    }

    if (ert_msgpack_string_equals("f", &kv_obj->key.via.str)) {
      result = ert_msgpack_copy_string_object(metadata->format, 8, &kv_obj->val);
      if (result < 0) {
        ert_log_error("Image metadata 'f' object is invalid");
        return -EINVAL;
      }
    }
  }

  return 0;
}

int ert_image_metadata_msgpack_deserialize(uint32_t length, uint8_t *data, ert_image_metadata *metadata)
{
  int result = 0;

  msgpack_zone mempool;
  msgpack_zone_init(&mempool, 1024);

  msgpack_object root_obj;
  msgpack_unpack_return unpack_result = msgpack_unpack((const char *) data, length, NULL, &mempool, &root_obj);

  if (unpack_result != MSGPACK_UNPACK_SUCCESS && unpack_result != MSGPACK_UNPACK_EXTRA_BYTES) {
    ert_log_error("Invalid MsgPack data, result %d", unpack_result);
    result = -EINVAL;
    goto error_msgpack;
  }

  result = ert_image_metadata_msgpack_deserialize_root(metadata, &root_obj);
  if (result < 0) {
    ert_log_error("Error deserializing image metadata root, result %d", result);
    goto error_msgpack;
  }

  msgpack_zone_destroy(&mempool);

  return 0;

  error_msgpack:
  msgpack_zone_destroy(&mempool);

  return result;
}

static int32_t ert_image_metadata_msgpack_find_header(uint32_t length, uint8_t *data)
{
  size_t header_length = strlen(ert_image_metadata_header_id);

  if (header_length >= length) {
    return -ENODATA;
  }

  for (int32_t i = 0; i < (length - header_length); i++) {
    if (memcmp(ert_image_metadata_header_id, data + i, header_length) == 0) {
      return i;
    }
  }

  return -EINVAL;
}

int ert_image_metadata_msgpack_deserialize_with_header(uint32_t length, uint8_t *data, ert_image_metadata *metadata)
{
  int32_t header_index = ert_image_metadata_msgpack_find_header(length, data);
  if (header_index < 0) {
    ert_log_info("No image metadata header found in %d bytes of data", length);
    return -EINVAL;
  }

  uint32_t header_length = (uint32_t) strlen(ert_image_metadata_header_id);
  uint32_t offset = header_index + header_length;

  return ert_image_metadata_msgpack_deserialize(length - offset, data + offset, metadata);
}

int ert_image_metadata_json_serialize(ert_image_metadata *metadata, uint8_t entry_type,
    uint32_t *length, uint8_t **data_rcv)
{
  int result;

  struct json_t *root_obj = json_object();

  result = json_object_set_new(root_obj, "type", json_integer(entry_type));
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  result = json_object_set_new(root_obj, "id", json_integer(metadata->id));
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  int64_t timestamp_millis =
      (int64_t) (((int64_t) metadata->timestamp.tv_sec) * 1000LL
                 + (((int64_t) metadata->timestamp.tv_nsec) / 1000000LL));
  result = json_object_set_new(root_obj, "timestamp_millis", json_integer(timestamp_millis));
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  uint8_t timestamp_string[64];
  ert_format_iso8601_timestamp(&metadata->timestamp, 64, timestamp_string);
  result = json_object_set_new(root_obj, "timestamp", json_string(timestamp_string));
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  result = json_object_set_new(root_obj, "format", json_string(metadata->format));
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  result = json_object_set_new(root_obj, "file", json_string(metadata->filename));
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error creating JSON object");
    return -EIO;
  }

  char *data = json_dumps(root_obj, JSON_COMPACT | JSON_PRESERVE_ORDER);
  json_decref(root_obj);

  if (data == NULL) {
    ert_log_error("Error serializing JSON to string");
    return -EIO;
  }

  *data_rcv = (uint8_t *) data;
  *length = (uint32_t) strlen(data);

  return 0;
}
