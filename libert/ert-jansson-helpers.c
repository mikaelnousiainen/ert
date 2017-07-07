#include <string.h>
#include <jansson.h>

#include "ert-jansson-helpers.h"
#include "ert-time.h"

int ert_jansson_serialize_timestamp_millis(json_t *json_obj, char *key, struct timespec *ts)
{
  int64_t timestamp_millis =
      (int64_t) (((int64_t) ts->tv_sec) * 1000LL
                 + (((int64_t) ts->tv_nsec) / 1000000LL));
  return json_object_set_new(json_obj, key, json_integer(timestamp_millis));
}

int ert_jansson_serialize_timestamp_iso8601(json_t *json_obj, char *key, struct timespec *ts)
{
  if (ert_timespec_is_zero(ts)) {
    return json_object_set_new(json_obj, key, json_null());
  }

  uint8_t timestamp_string[64];
  ert_format_iso8601_timestamp(ts, 64, timestamp_string);
  return json_object_set_new(json_obj, key, json_string((const char *) timestamp_string));
}

int ert_jansson_serialize_timestamp_iso8601_and_millis(json_t *json_obj, char *key_prefix, struct timespec *ts)
{
  size_t key_millis_length = 128;
  char key_millis[key_millis_length];
  strncpy(key_millis, key_prefix, key_millis_length);
  strncat(key_millis, "_millis", key_millis_length - strlen(key_millis));

  int result = ert_jansson_serialize_timestamp_iso8601(json_obj, key_prefix, ts);
  if (result < 0) {
    return result;
  }
  result = ert_jansson_serialize_timestamp_millis(json_obj, key_millis, ts);
  if (result < 0) {
    return result;
  }

  return 0;
}
