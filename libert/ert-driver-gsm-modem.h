/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_GSM_MODEM_H
#define __ERT_DRIVER_GSM_MODEM_H

#include "ert-common.h"
#include "ert-pipe.h"
#include "ert-hal-serial.h"

#include <pthread.h>

#define ERT_DRIVER_GSM_MODEM_DATA_BUFFER_SIZE 1024

typedef enum _ert_driver_gsm_modem_network_selection_mode {
  ERT_DRIVER_GSM_MODEM_NETWORK_SELECTION_MODE_AUTOMATIC = 0,
  ERT_DRIVER_GSM_MODEM_NETWORK_SELECTION_MODE_MANUAL = 1,
  ERT_DRIVER_GSM_MODEM_NETWORK_SELECTION_MODE_DEREGISTER = 2,
  ERT_DRIVER_GSM_MODEM_NETWORK_SELECTION_MODE_SET_ONLY = 3,
  ERT_DRIVER_GSM_MODEM_NETWORK_SELECTION_MODE_MANUAL_WITH_AUTOMATIC_FALLBACK = 4,
} ert_driver_gsm_modem_network_selection_mode;

typedef enum _ert_driver_gsm_modem_network_technology {
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_GSM = 0,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_GSM_COMPACT = 1,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_UTRAN = 2,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_EGPRS = 3,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_UTRAN_HSDPA = 4,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_UTRAN_HSUPA = 5,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_UTRAN_HSDPA_HSUPA= 6,
  ERT_DRIVER_GSM_MODEM_NETWORK_TECHNOLOGY_E_UTRAN = 7,
} ert_driver_gsm_modem_network_technology;

typedef enum _ert_driver_gsm_modem_network_operator_format {
  ERT_DRIVER_GSM_MODEM_NETWORK_OPERATOR_FORMAT_LONG_ALPHANUMERIC = 0,
  ERT_DRIVER_GSM_MODEM_NETWORK_OPERATOR_FORMAT_SHORT_ALPHANUMERIC = 1,
  ERT_DRIVER_GSM_MODEM_NETWORK_OPERATOR_FORMAT_NUMERIC = 2,
} ert_driver_gsm_modem_network_operator_format;

typedef enum _ert_driver_gsm_modem_network_registration_report_mode {
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_MODE_DISABLED = 0,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_MODE_ENABLED = 1,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_MODE_ENABLED_WITH_CID = 2,
} ert_driver_gsm_modem_network_registration_report_mode;

typedef enum _ert_driver_gsm_modem_network_registration_report_status {
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_STATUS_NOT_REGISTERED = 0,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_STATUS_REGISTERED_HOME = 1,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_STATUS_NOT_REGISTERED_SEARCHING = 2,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_STATUS_REGISTRATION_DENIED = 3,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_STATUS_UNKNOWN = 4,
  ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_STATUS_REGISTERED_ROAMING = 5,
} ert_driver_gsm_modem_network_registration_report_status;

typedef enum _ert_driver_gsm_modem_command_status {
  ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_PENDING = 0,
  ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_OK = 1,
  ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_ERROR = 2,
  ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_TIMEOUT = 3,
} ert_driver_gsm_modem_command_status;

typedef struct _ert_driver_gsm_modem_command {
  uint8_t command_data[ERT_DRIVER_GSM_MODEM_DATA_BUFFER_SIZE];
  uint8_t expected_response[ERT_DRIVER_GSM_MODEM_DATA_BUFFER_SIZE];
  bool expect_response_without_delimiter;
  bool ignore_invalid_response;
  uint32_t timeout_milliseconds;

  uint32_t response_data_bytes_used;
  uint8_t response_data[ERT_DRIVER_GSM_MODEM_DATA_BUFFER_SIZE];

  ert_driver_gsm_modem_command_status status;
} ert_driver_gsm_modem_command;

typedef struct _ert_driver_gsm_modem_command_queue_entry {
  ert_driver_gsm_modem_command *command;

  pthread_mutex_t executed_mutex;
  pthread_cond_t executed_cond;
  volatile bool executed_signal;
} ert_driver_gsm_modem_command_queue_entry;

typedef struct _ert_driver_gsm_modem_config {
  char pin[8];

  uint32_t command_queue_size;
  uint32_t poll_interval_millis;
  uint32_t response_timeout_millis;
  uint32_t retry_count;

  hal_serial_device_config serial_device_config;
} ert_driver_gsm_modem_config;

typedef struct _ert_driver_gsm_modem_info {
  char manufacturer[128];
  char model[128];
  char revision[128];
  char product_serial_number[128];
  char subscriber_number[128];
  char international_mobile_subscriber_identity[128];
  char sms_service_center_number[128];

  int32_t rssi;
  int32_t bit_error_rate;
  char operator_name[64];
  ert_driver_gsm_modem_network_selection_mode network_selection_mode;
  ert_driver_gsm_modem_network_registration_report_mode network_registration_mode;
  ert_driver_gsm_modem_network_registration_report_status network_registration_status;
} ert_driver_gsm_modem_status;

typedef struct _ert_driver_gsm_modem {
  ert_driver_gsm_modem_config config;
  hal_serial_device *serial_device;

  ert_driver_gsm_modem_status status;

  volatile bool running;

  ert_pipe *pending_command_queue;
  ert_pipe *executed_command_queue;

  pthread_t transceiver_thread;

  uint32_t buffer_bytes_used;
  uint8_t buffer[ERT_DRIVER_GSM_MODEM_DATA_BUFFER_SIZE];
} ert_driver_gsm_modem;

int ert_driver_gsm_modem_create_command(ert_driver_gsm_modem *modem, ert_driver_gsm_modem_command *command,
    char *command_data, char *expected_response, bool ignore_invalid_response);
int ert_driver_gsm_modem_execute_command(ert_driver_gsm_modem *modem, ert_driver_gsm_modem_command *command);
int ert_driver_gsm_modem_init(ert_driver_gsm_modem *modem);
int ert_driver_gsm_modem_open(ert_driver_gsm_modem_config *config, ert_driver_gsm_modem **modem_rcv);
int ert_driver_gsm_modem_close(ert_driver_gsm_modem *modem);
int ert_driver_gsm_modem_get_status(ert_driver_gsm_modem *modem, ert_driver_gsm_modem_status *status);

int ert_driver_gsm_modem_set_network_registration_report(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_registration_report_mode mode);
int ert_driver_gsm_modem_get_network_registration_report(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_registration_report_mode *mode_rcv,
    ert_driver_gsm_modem_network_registration_report_status *status_rcv,
    char *local_area_code_rcv, char *cell_id_rcv);
int ert_driver_gsm_modem_get_signal_quality(ert_driver_gsm_modem *modem, int32_t *rssi_rcv, int32_t *ber_rcv);
int ert_driver_gsm_modem_set_operator_selection(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_selection_mode selection_mode,
    ert_driver_gsm_modem_network_operator_format format);
int ert_driver_gsm_modem_get_operator_selection(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_selection_mode *selection_mode_rcv,
    ert_driver_gsm_modem_network_operator_format *format_rcv, char *name_rcv);

int ert_driver_gsm_modem_send_sms(ert_driver_gsm_modem *modem, char *number, char *message);

#endif
