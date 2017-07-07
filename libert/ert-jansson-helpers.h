#ifndef __ERT_JANSSON_HELPERS_H
#define __ERT_JANSSON_HELPERS_H

#include <time.h>
#include "ert-common.h"

#define jansson_check_result(x) \
  if ((x) < 0) { \
    ert_log_error("JSON serialization failed"); \
    return -EIO; \
  }

int ert_jansson_serialize_timestamp_millis(json_t *json_obj, char *key, struct timespec *ts);
int ert_jansson_serialize_timestamp_iso8601(json_t *json_obj, char *key, struct timespec *ts);
int ert_jansson_serialize_timestamp_iso8601_and_millis(json_t *json_obj, char *key_prefix, struct timespec *ts);

    #endif
