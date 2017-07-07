#ifndef __ERT_SERVER_STATUS_H
#define __ERT_SERVER_STATUS_H

#include "ert-common.h"
#include "ert-data-logger.h"

typedef struct _ert_server_status_data_logger_entry {
  uint32_t id;
  uint8_t type;
  struct timespec timestamp;
} ert_server_status_data_logger_entry;

typedef struct _ert_server_status_telemetry {
  struct timespec last_transferred_telemetry_entry_timestamp;
  ert_server_status_data_logger_entry last_transferred_telemetry_entry;
  uint32_t transfer_failure_count;
} ert_server_status_telemetry;

typedef struct _ert_server_status {
  ert_server_status_telemetry telemetry_transmitted;
  ert_server_status_telemetry telemetry_received;
} ert_server_status;

int ert_server_status_update_last_transmitted_telemetry(ert_data_logger_entry *data_logger_entry,
    ert_server_status *status);
int ert_server_status_update_last_received_telemetry(ert_data_logger_entry *data_logger_entry,
    ert_server_status *status);
int ert_server_status_record_telemetry_transmission_failure(ert_server_status *status);
int ert_server_status_record_telemetry_reception_failure(ert_server_status *status);
int ert_server_status_json_serialize(ert_server_status *status, uint32_t *length, uint8_t **data_rcv);

#endif
