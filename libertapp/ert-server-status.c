#include <string.h>
#include <errno.h>
#include <jansson.h>

#include "ert-server-status.h"
#include "ert-jansson-helpers.h"
#include "ert-time.h"
#include "ert-log.h"

static int ert_server_status_update_last_transferred_telemetry_entry(ert_data_logger_entry *data_logger_entry,
  ert_server_status_telemetry *status_telemetry)
{
  int result = ert_get_current_timestamp(&status_telemetry->last_transferred_telemetry_entry_timestamp);
  if (result < 0) {
    return result;
  }

  memcpy(&status_telemetry->last_transferred_telemetry_entry.timestamp, &data_logger_entry->timestamp, sizeof(struct timespec));
  status_telemetry->last_transferred_telemetry_entry.id = data_logger_entry->entry_id;
  status_telemetry->last_transferred_telemetry_entry.type = data_logger_entry->entry_type;

  return 0;
}

int ert_server_status_update_last_transmitted_telemetry(ert_data_logger_entry *data_logger_entry,
    ert_server_status *status)
{
  status->telemetry_transmitted.transfer_failure_count = 0;
  return ert_server_status_update_last_transferred_telemetry_entry(data_logger_entry, &status->telemetry_transmitted);
}

int ert_server_status_record_telemetry_transmission_failure(ert_server_status *status)
{
  status->telemetry_transmitted.transfer_failure_count++;
  return 0;
}

int ert_server_status_update_last_received_telemetry(ert_data_logger_entry *data_logger_entry,
    ert_server_status *status)
{
  status->telemetry_received.transfer_failure_count = 0;
  return ert_server_status_update_last_transferred_telemetry_entry(data_logger_entry, &status->telemetry_received);
}

int ert_server_status_record_telemetry_reception_failure(ert_server_status *status)
{
  status->telemetry_received.transfer_failure_count++;
  return 0;
}

static int ert_server_status_data_logger_entry_json_serialize(json_t *json_obj, ert_server_status_data_logger_entry *entry)
{
  jansson_check_result(json_object_set_new(json_obj, "id", json_integer(entry->id)));
  jansson_check_result(json_object_set_new(json_obj, "type", json_integer(entry->type)));
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601_and_millis(json_obj, "timestamp", &entry->timestamp));

  return 0;
}

static int ert_server_status_telemetry_json_serialize(json_t *json_obj, ert_server_status_telemetry *status_telemetry)
{
  jansson_check_result(ert_jansson_serialize_timestamp_iso8601_and_millis(json_obj,
      "last_transferred_telemetry_entry_timestamp", &status_telemetry->last_transferred_telemetry_entry_timestamp));

  struct json_t *last_transferred_telemetry_entry_obj = json_object();
  jansson_check_result(json_object_set_new(json_obj, "last_transferred_telemetry_entry", last_transferred_telemetry_entry_obj));

  jansson_check_result(ert_server_status_data_logger_entry_json_serialize(
      last_transferred_telemetry_entry_obj, &status_telemetry->last_transferred_telemetry_entry));

  jansson_check_result(json_object_set_new(json_obj, "transfer_failure_count", json_integer(status_telemetry->transfer_failure_count)));

  return 0;
}

static int ert_server_status_telemetry_object_json_serialize(json_t *json_obj, char *key, ert_server_status_telemetry *status_telemetry)
{
  struct json_t *telemetry_status_obj = json_object();
  jansson_check_result(json_object_set_new(json_obj, key, telemetry_status_obj));

  return ert_server_status_telemetry_json_serialize(telemetry_status_obj, status_telemetry);
}

int ert_server_status_json_serialize(ert_server_status *status, uint32_t *length, uint8_t **data_rcv)
{
  int result;

  struct json_t *root_obj = json_object();

  result = ert_server_status_telemetry_object_json_serialize(root_obj, "telemetry_transmitted", &status->telemetry_transmitted);
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error serializing transmitted telemetry status to JSON string");
    return -EIO;
  }

  result = ert_server_status_telemetry_object_json_serialize(root_obj, "telemetry_received", &status->telemetry_received);
  if (result < 0) {
    json_decref(root_obj);
    ert_log_error("Error serializing received telemetry status to JSON string");
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
