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
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-driver-gsm-modem.h"

static char *modem_init_commands[] = {
    "ATE0",
    "ATZ",
    "ATQ0",
    "ATV1",
    "AT+CMGF=1",
    "AT+CMEE=1",
    NULL
};

static char *modem_init_commands_after_pin[] = {
    "AT+CSCS=\"GSM\"",
    NULL
};

#define ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER "\r\n"
#define ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER_LENGTH 2
#define ERT_DRIVER_GSM_MODEM_CTRL_Z 0x1A

#define ERT_DRIVER_GSM_MODEM_RESPONSE_OK "OK"
#define ERT_DRIVER_GSM_MODEM_RESPONSE_ERROR "ERROR"
#define ERT_DRIVER_GSM_MODEM_RESPONSE_CME_ERROR "+CME ERROR:"
#define ERT_DRIVER_GSM_MODEM_RESPONSE_CMS_ERROR "+CMS ERROR"

static inline bool starts_with(char *prefix, char *string)
{
  return strncmp(prefix, string, strlen(prefix)) == 0;
}

static int ert_driver_gsm_modem_get_time_millis(uint64_t *time_milliseconds)
{
  struct timespec to;

  int result = clock_gettime(CLOCK_REALTIME, &to);
  if (result < 0) {
    return -EIO;
  }

  *time_milliseconds = (uint64_t) (to.tv_sec * 1000L + to.tv_nsec / 1000000L);

  return 0;
}

static int ert_driver_gsm_modem_parse_input(ert_driver_gsm_modem_command *command, char *input_buffer, uint32_t *bytes_left_rcv)
{
  char *delimiter_pointer;
  char *delimiter;
  char strpbrk_delimiter[32];
  size_t delimiter_length;

  if (command->expect_response_without_delimiter) {
    delimiter = (char *) command->expected_response;
    delimiter_length = strlen(delimiter);
    snprintf(strpbrk_delimiter, 32, "%s%s", command->expected_response, ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER);
  } else {
    delimiter = ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER;
    delimiter_length = ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER_LENGTH;
    strcpy(strpbrk_delimiter, ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER);
  }

  while ((delimiter_pointer = strpbrk(input_buffer, strpbrk_delimiter)) != NULL) {
    size_t parsed_length = (size_t) ((uintptr_t) delimiter_pointer - (uintptr_t) input_buffer);
    // Include delimiter in response data
    size_t response_data_length = parsed_length + delimiter_length;

    // TODO: Handle unsolicited responses (e.g. +CMTI) and proprietary (^) notifications

    if (parsed_length > 0) {
      char *current_response_data = (char *) command->response_data + command->response_data_bytes_used;
      strncpy(current_response_data, input_buffer, response_data_length);
      current_response_data[response_data_length] = '\0';
      command->response_data_bytes_used += response_data_length;

      ert_log_debug("Parsed modem response: '%s'", current_response_data);
    }

    if (starts_with(ERT_DRIVER_GSM_MODEM_RESPONSE_OK, input_buffer)) {
      command->status = ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_OK;
    } else if (starts_with(ERT_DRIVER_GSM_MODEM_RESPONSE_ERROR, input_buffer)
        || starts_with(ERT_DRIVER_GSM_MODEM_RESPONSE_CME_ERROR, input_buffer)
        || starts_with(ERT_DRIVER_GSM_MODEM_RESPONSE_CMS_ERROR, input_buffer)) {
      command->status = ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_ERROR;
    } else if (command->expect_response_without_delimiter && starts_with(delimiter, input_buffer)) {
      command->status = ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_OK;
    }

    // Move rest of the input to beginning of the buffer
    strcpy(input_buffer, input_buffer + response_data_length);
  };

  *bytes_left_rcv = (uint32_t) strlen(input_buffer);

  return 0;
}

static int ert_driver_gsm_modem_write_command(ert_driver_gsm_modem *modem, uint8_t *command) {
  ert_log_debug("Writing modem command: '%s'", command);

  uint32_t bytes_written;
  int result = hal_serial_write(modem->serial_device, (uint32_t) strlen((const char *) command), command, &bytes_written);
  if (result < 0) {
    return result;
  }

  result = hal_serial_write(modem->serial_device, ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER_LENGTH,
      (uint8_t *) ERT_DRIVER_GSM_MODEM_COMMAND_DELIMITER, &bytes_written);
  if (result < 0) {
    return result;
  }

  return 0;
}

static void *ert_driver_gsm_modem_transceiver_routine(void *context)
{
  ert_driver_gsm_modem *modem = (ert_driver_gsm_modem *) context;
  ert_driver_gsm_modem_command_queue_entry *current_command_queue_entry = NULL;
  bool command_executing = false;
  uint64_t command_start_millis = 0;
  uint32_t buffer_size = ERT_DRIVER_GSM_MODEM_DATA_BUFFER_SIZE - 1;
  int result;

  ert_log_info("GSM modem transceiver routine running ...");

  while (modem->running) {
    uint32_t buffer_available = buffer_size - modem->buffer_bytes_used;
    uint32_t bytes_read = 0;

    result = hal_serial_read(modem->serial_device, modem->config.poll_interval_millis,
        buffer_available, modem->buffer + modem->buffer_bytes_used, &bytes_read);
    if (result < 0 && result != -ETIMEDOUT) {
      break;
    }

    if (bytes_read > 0) {
      modem->buffer[modem->buffer_bytes_used + bytes_read] = '\0';

      ert_log_debug("Read %d bytes from modem: '%s'", bytes_read, modem->buffer + modem->buffer_bytes_used);

      if (command_executing && current_command_queue_entry != NULL) {
        result = ert_driver_gsm_modem_parse_input(current_command_queue_entry->command, (char *) modem->buffer, &modem->buffer_bytes_used);
        if (result < 0) {
          ert_log_error("Error parsing modem responses, result %d", result);
          continue;
        }
      } else {
        modem->buffer_bytes_used += bytes_read;
      }
    }

    if (modem->buffer_bytes_used >= buffer_size) {
      ert_log_error("Modem receive buffer full");
      modem->buffer_bytes_used = 0;
      modem->buffer[0] = '\0';
      continue;
    }

    if (command_executing) {
      if (current_command_queue_entry->command->status == ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_PENDING) {
        uint64_t current_millis;
        result = ert_driver_gsm_modem_get_time_millis(&current_millis);
        if (result < 0) {
          break;
        }

        if ((current_millis - command_start_millis) < current_command_queue_entry->command->timeout_milliseconds) {
          // Wait for more data
          continue;
        }

        current_command_queue_entry->command->status = ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_TIMEOUT;
      }

      pthread_mutex_lock(&current_command_queue_entry->executed_mutex);
      current_command_queue_entry->executed_signal = true;
      pthread_cond_signal(&current_command_queue_entry->executed_cond);
      pthread_mutex_unlock(&current_command_queue_entry->executed_mutex);

      command_executing = false;
    }

    ert_log_debug("ert_pipe_pop_timed for commands");
    ssize_t pop_count = ert_pipe_pop_timed(modem->pending_command_queue, &current_command_queue_entry, 1, 1);
    ert_log_debug("ert_pipe_pop_timed for commands: %d", pop_count);
    if (pop_count == 0) {
      break;
    } else if (pop_count < 0) {
      continue;
    }

    result = ert_driver_gsm_modem_write_command(modem, current_command_queue_entry->command->command_data);
    if (result < 0) {
      ert_log_error("Error writing command to modem, result %d", result);
      command_executing = false;
    } else {
      // Clear buffer for command response
      modem->buffer_bytes_used = 0;
      modem->buffer[0] = '\0';

      command_executing = true;
      result = ert_driver_gsm_modem_get_time_millis(&command_start_millis);
      if (result < 0) {
        break;
      }
    }
  }

  if (command_executing && current_command_queue_entry != NULL) {
    current_command_queue_entry->command->status = ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_ERROR;

    pthread_mutex_lock(&current_command_queue_entry->executed_mutex);
    current_command_queue_entry->executed_signal = true;
    pthread_cond_signal(&current_command_queue_entry->executed_cond);
    pthread_mutex_unlock(&current_command_queue_entry->executed_mutex);
  }

  modem->running = false;

  ert_log_info("GSM modem transceiver routine stopped");

  return NULL;
}

int ert_driver_gsm_modem_execute_command(ert_driver_gsm_modem *modem, ert_driver_gsm_modem_command *command)
{
  int result;
  ert_log_debug("Enqueuing modem command: '%s'", command->command_data);

  ert_driver_gsm_modem_command_queue_entry command_queue_entry = {0};

  result = pthread_mutex_init(&command_queue_entry.executed_mutex, NULL);
  if (result != 0) {
    ert_log_error("pthread_mutex_init failed with result %d: %s", result, strerror(errno));
    return -EIO;
  }
  result = pthread_cond_init(&command_queue_entry.executed_cond, NULL);
  if (result != 0) {
    pthread_mutex_destroy(&command_queue_entry.executed_mutex);
    ert_log_error("pthread_cond_init failed with result %d: %s", result, strerror(errno));
    return -EIO;
  }

  command_queue_entry.command = command;
  command_queue_entry.executed_signal = false;

  ert_driver_gsm_modem_command_queue_entry *command_queue_entry_ptr = &command_queue_entry;

  ert_pipe_push(modem->pending_command_queue, &command_queue_entry_ptr, 1);

  while (!command_queue_entry.executed_signal) {
    struct timespec to;

    result = ert_get_current_timestamp_offset(&to, command->timeout_milliseconds);
    if (result < 0) {
      pthread_cond_destroy(&command_queue_entry.executed_cond);
      pthread_mutex_destroy(&command_queue_entry.executed_mutex);
      return -EIO;
    }

    pthread_mutex_lock(&command_queue_entry.executed_mutex);
    result = pthread_cond_timedwait(&command_queue_entry.executed_cond, &command_queue_entry.executed_mutex, &to);
    pthread_mutex_unlock(&command_queue_entry.executed_mutex);

    if (result != ETIMEDOUT && result != 0) {
      pthread_cond_destroy(&command_queue_entry.executed_cond);
      pthread_mutex_destroy(&command_queue_entry.executed_mutex);
      ert_log_error("pthread_cond_timedwait failed with result %d", result);
      return -EIO;
    }
  }

  pthread_cond_destroy(&command_queue_entry.executed_cond);
  pthread_mutex_destroy(&command_queue_entry.executed_mutex);

  switch (command->status) {
    case ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_OK:
      return 0;
    case ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_PENDING:
      return -EINTR;
    case ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_TIMEOUT:
      return -ETIMEDOUT;
    case ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_ERROR:
    default:
      return -EIO;
  }
}

int ert_driver_gsm_modem_create_command(ert_driver_gsm_modem *modem, ert_driver_gsm_modem_command *command,
    char *command_data, char *expected_response, bool ignore_invalid_response)
{
  strcpy((char *) command->command_data, command_data);
  if (expected_response != NULL) {
    strcpy((char *) command->expected_response, expected_response);
  } else {
    command->expected_response[0] = '\0';
  }

  command->status = ERT_DRIVER_GSM_MODEM_COMMAND_STATUS_PENDING;
  command->response_data_bytes_used = 0;
  command->response_data[0] = '\0';
  command->timeout_milliseconds = modem->config.response_timeout_millis;
  command->ignore_invalid_response = ignore_invalid_response;
  command->expect_response_without_delimiter = false;

  return 0;
}

int ert_driver_gsm_modem_check_and_send_sim_pin(ert_driver_gsm_modem *modem)
{
  ert_driver_gsm_modem_command command;
  ert_driver_gsm_modem_create_command(modem, &command, "AT+CPIN?", "+CPIN:", false);
  int result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    return result;
  }

  if (starts_with("+CPIN: SIM PIN", (char *) command.response_data)) {
    char data[32];
    snprintf(data, 32, "AT+CPIN=\"%s\"", modem->config.pin);
    ert_driver_gsm_modem_create_command(modem, &command, data, NULL, false);
    result = ert_driver_gsm_modem_execute_command(modem, &command);
    if (result < 0) {
      return result;
    }
    return 0;
  } else if (starts_with("+CPIN: READY", (char *) command.response_data)) {
    return 0;
  }

  ert_log_error("Error querying GSM modem SIM PIN status, received response: '%s'", command.response_data);

  return -EIO;
}

int ert_driver_gsm_modem_send_sms(ert_driver_gsm_modem *modem, char *number, char *message)
{
  ert_log_info("Sending SMS message: number=\"%s\" message=\"%s\" ...", number, message);

  ert_driver_gsm_modem_command command;
  char data[128];
  snprintf(data, 128, "AT+CMGS=\"%s\"", number);

  ert_driver_gsm_modem_create_command(modem, &command, data, ">", false);
  command.expect_response_without_delimiter = true;
  int result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error initiating sending of SMS message to number \"%s\", result %d", number, result);
    return result;
  }

  char msg_data[256];
  snprintf(msg_data, 256, "%s%c", message, ERT_DRIVER_GSM_MODEM_CTRL_Z);

  ert_driver_gsm_modem_create_command(modem, &command, msg_data, NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error sending of SMS message \"%s\" to number \"%s\", result %d", message, number, result);
    return result;
  }

  ert_log_info("SMS message sent successfully: number=\"%s\" message=\"%s\"", number, message);

  return 0;
}

int ert_driver_gsm_modem_set_network_registration_report(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_registration_report_mode mode)
{
  int result;
  char data[16];
  ert_driver_gsm_modem_command command;

  snprintf(data, 16, "AT+CREG=%d", mode);
  ert_driver_gsm_modem_create_command(modem, &command, data, NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error enabling network registration with mode %d, result %d: %s", mode, result, command.response_data);

    return result;
  }

  return 0;
}

int ert_driver_gsm_modem_get_network_registration_report(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_registration_report_mode *mode_rcv,
    ert_driver_gsm_modem_network_registration_report_status *status_rcv,
    char *local_area_code_rcv, char *cell_id_rcv)
{
  int result;
  ert_driver_gsm_modem_command command;

  ert_driver_gsm_modem_create_command(modem, &command, "AT+CREG?", NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error getting network registration report, result %d: %s", result, command.response_data);

    return result;
  }

  ert_driver_gsm_modem_network_registration_report_mode mode;
  ert_driver_gsm_modem_network_registration_report_status status;
  char local_area_code[32], cell_id[32];
  local_area_code[0] = '\0';
  cell_id[0] = '\0';

  sscanf((char *) command.response_data, "+CREG: %d,%d,\"%[^\"]\",\"%[^\"]\",", &mode, &status, local_area_code, cell_id);

  if (mode_rcv != NULL) {
    *mode_rcv = mode;
  }
  if (status_rcv != NULL) {
    *status_rcv = status;
  }
  if (local_area_code_rcv != NULL) {
    strcpy(local_area_code_rcv, local_area_code);
  }
  if (cell_id_rcv != NULL) {
    strcpy(cell_id_rcv, cell_id);
  }

  return 0;
}

int ert_driver_gsm_modem_get_signal_quality(ert_driver_gsm_modem *modem, int32_t *rssi_rcv, int32_t *ber_rcv)
{
  int result;
  ert_driver_gsm_modem_command command;

  ert_driver_gsm_modem_create_command(modem, &command, "AT+CSQ", NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error getting signal quality, result %d: %s", result, command.response_data);

    return result;
  }

  int32_t rssi, ber;
  sscanf((char *) command.response_data, "+CSQ: %d,%d", &rssi, &ber);

  if (rssi_rcv != NULL) {
    *rssi_rcv = rssi;
  }
  if (ber_rcv != NULL) {
    *ber_rcv = ber;
  }

  return 0;
}

int ert_driver_gsm_modem_set_operator_selection(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_selection_mode selection_mode,
    ert_driver_gsm_modem_network_operator_format format)
{
  int result;
  char data[32];
  ert_driver_gsm_modem_command command;

  snprintf(data, 32, "AT+COPS=%d,%d", selection_mode, format);

  ert_driver_gsm_modem_create_command(modem, &command, data, NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error setting operator selection, result %d: %s", result, command.response_data);

    return result;
  }

  return 0;
}

int ert_driver_gsm_modem_get_operator_selection(ert_driver_gsm_modem *modem,
    ert_driver_gsm_modem_network_selection_mode *selection_mode_rcv,
    ert_driver_gsm_modem_network_operator_format *format_rcv, char *name_rcv)
{
  int result;
  ert_driver_gsm_modem_command command;

  ert_driver_gsm_modem_create_command(modem, &command, "AT+COPS?", NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error getting operator selection, result %d: %s", result, command.response_data);

    return result;
  }

  ert_driver_gsm_modem_network_selection_mode selection_mode;
  ert_driver_gsm_modem_network_operator_format format;
  char name[32];
  sscanf((char *) command.response_data, "+COPS: %d,%d,\"%[^\"]\"", &selection_mode, &format, name);

  if (selection_mode_rcv != NULL) {
    *selection_mode_rcv = selection_mode;
  }
  if (format_rcv != NULL) {
    *format_rcv = format;
  }
  if (name_rcv != NULL) {
    strcpy(name_rcv, name);
  }

  return 0;
}

int ert_driver_gsm_modem_get_value(ert_driver_gsm_modem *modem, char *command_name, size_t length, char *value_rcv)
{
  int result;
  ert_driver_gsm_modem_command command;

  ert_driver_gsm_modem_create_command(modem, &command, command_name, NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error getting value for command %s, result %d: %s", command_name, result, command.response_data);

    return result;
  }

  char value[128];
  sscanf((char *) command.response_data, "%s", value);

  if (value_rcv != NULL) {
    strncpy(value_rcv, value, length);
  }

  return 0;
}

int ert_driver_gsm_modem_get_subscriber_number(ert_driver_gsm_modem *modem, size_t length, char *value_rcv)
{
  int result;
  ert_driver_gsm_modem_command command;

  ert_driver_gsm_modem_create_command(modem, &command, "AT+CNUM", NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error getting subscriber number, result %d: %s", result, command.response_data);

    return result;
  }

  char value[128];
  sscanf((char *) command.response_data, "+CNUM: \"\",\"%[^\"]\"", value);

  if (value_rcv != NULL) {
    strncpy(value_rcv, value, length);
  }

  return 0;
}

int ert_driver_gsm_modem_get_sms_service_center_number(ert_driver_gsm_modem *modem, size_t length, char *value_rcv)
{
  int result;
  ert_driver_gsm_modem_command command;

  ert_driver_gsm_modem_create_command(modem, &command, "AT+CSCA?", NULL, false);
  result = ert_driver_gsm_modem_execute_command(modem, &command);
  if (result < 0) {
    ert_log_error("Error getting SMS service center number, result %d: %s", result, command.response_data);

    return result;
  }

  char value[128];
  sscanf((char *) command.response_data, "+CSCA: \"%[^\"]\"", value);

  if (value_rcv != NULL) {
    strncpy(value_rcv, value, length);
  }

  return 0;
}

static int ert_driver_gsm_modem_read_static_status(ert_driver_gsm_modem *modem)
{
  ert_driver_gsm_modem_get_value(modem, "AT+CGMI", 128, modem->status.manufacturer);
  ert_driver_gsm_modem_get_value(modem, "AT+CGMM", 128, modem->status.model);
  ert_driver_gsm_modem_get_value(modem, "AT+CGMR", 128, modem->status.revision);
  ert_driver_gsm_modem_get_value(modem, "AT+CGSN", 128, modem->status.product_serial_number);
  ert_driver_gsm_modem_get_value(modem, "AT+CIMI", 128, modem->status.international_mobile_subscriber_identity);

  ert_driver_gsm_modem_get_subscriber_number(modem, 128, modem->status.subscriber_number);
  ert_driver_gsm_modem_get_sms_service_center_number(modem, 128, modem->status.sms_service_center_number);

  return 0;
}

int ert_driver_gsm_modem_get_status(ert_driver_gsm_modem *modem, ert_driver_gsm_modem_status *status)
{
  int result = ert_driver_gsm_modem_get_signal_quality(modem, &modem->status.rssi, &modem->status.bit_error_rate);
  if (result != 0) {
    ert_log_error("ert_driver_gsm_modem_get_signal_quality failed with result %d", result);
  }

  result = ert_driver_gsm_modem_get_operator_selection(modem, &modem->status.network_selection_mode,
      NULL, modem->status.operator_name);
  if (result != 0) {
    ert_log_error("ert_driver_gsm_modem_get_operator_selection failed with result %d", result);
  }

  result = ert_driver_gsm_modem_get_network_registration_report(modem, &modem->status.network_registration_mode,
      &modem->status.network_registration_status, NULL, NULL);
  if (result != 0) {
    ert_log_error("ert_driver_gsm_modem_get_network_registration_report failed with result %d", result);
  }

  memcpy(status, &modem->status, sizeof(ert_driver_gsm_modem_status));

  return 0;
}

int ert_driver_gsm_modem_init(ert_driver_gsm_modem *modem)
{
  ert_driver_gsm_modem_command command;
  int result;

  ert_log_info("Initializing GSM modem ...");

  for (size_t i = 0; modem_init_commands[i] != NULL; i++) {
    ert_driver_gsm_modem_create_command(modem, &command, modem_init_commands[i], NULL, true);
    result = ert_driver_gsm_modem_execute_command(modem, &command);
    if (result < 0) {
      return result;
    }
  }

  if (modem->config.pin != NULL) {
    result = ert_driver_gsm_modem_check_and_send_sim_pin(modem);
    if (result < 0) {
      return result;
    }
  }

  for (size_t i = 0; modem_init_commands_after_pin[i] != NULL; i++) {
    ert_driver_gsm_modem_create_command(modem, &command, modem_init_commands_after_pin[i], NULL, true);
    result = ert_driver_gsm_modem_execute_command(modem, &command);
    if (result < 0) {
      return result;
    }
  }

  ert_driver_gsm_modem_read_static_status(modem);

  ert_log_info("GSM modem initialization finished");

  return 0;
}

int ert_driver_gsm_modem_open(ert_driver_gsm_modem_config *config, ert_driver_gsm_modem **modem_rcv)
{
  int result;

  ert_driver_gsm_modem *modem;
  modem = calloc(1, sizeof(ert_driver_gsm_modem));
  if (modem == NULL) {
    ert_log_fatal("Error allocating memory for modem struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_first;
  }

  memcpy(&modem->config, config, sizeof(ert_driver_gsm_modem_config));

  result = hal_serial_open(&modem->config.serial_device_config, &modem->serial_device);
  if (result < 0) {
    goto error_malloc_modem;
  }

  result = ert_pipe_create(sizeof(ert_driver_gsm_modem_command *), modem->config.command_queue_size, &modem->pending_command_queue);
  if (result != 0) {
    ert_log_error("Error creating modem pending command queue, result %d", result);
    goto error_serial_open;
  }

  result = ert_pipe_create(sizeof(ert_driver_gsm_modem_command *), modem->config.command_queue_size, &modem->executed_command_queue);
  if (result != 0) {
    ert_log_error("Error creating modem executed command queue, result %d", result);
    goto error_pending_command_queue;
  }

  modem->running = true;

  result = pthread_create(&modem->transceiver_thread, NULL, ert_driver_gsm_modem_transceiver_routine, modem);
  if (result != 0) {
    ert_log_error("Error starting transceiver thread for modem");
    goto error_executed_command_queue;
  }

  result = ert_driver_gsm_modem_init(modem);
  if (result < 0) {
    goto error_thread_create;
  }

  *modem_rcv = modem;

  return 0;

  error_thread_create:
  modem->running = false;

  error_executed_command_queue:
  ert_pipe_destroy(modem->executed_command_queue);

  error_pending_command_queue:
  ert_pipe_destroy(modem->pending_command_queue);

  error_serial_open:
  hal_serial_close(modem->serial_device);

  error_malloc_modem:
  free(modem);

  error_first:

  return result;
}

int ert_driver_gsm_modem_close(ert_driver_gsm_modem *modem)
{
  modem->running = false;

  ert_pipe_destroy(modem->executed_command_queue);
  ert_pipe_destroy(modem->pending_command_queue);

  pthread_join(modem->transceiver_thread, NULL);

  hal_serial_close(modem->serial_device);
  free(modem);

  return 0;
}
