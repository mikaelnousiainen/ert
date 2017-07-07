/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>

#include "ertnode.h"
#include "ertnode-config.h"
#include "ertnode-collector-image.h"
#include "ertnode-collector-telemetry.h"
#include "ertnode-sender-image-comm.h"
#include "ertnode-sender-telemetry-comm.h"
#include "ertnode-sender-telemetry-gsm.h"
#include "ert-gps-ublox.h"

#include "ert-data-logger-serializer-jansson.h"
#include "ert-data-logger-writer-zlog.h"
#include "ert-data-logger-serializer-msgpack.h"
#include "ertnode-server.h"

#define ERT_DATA_LOGGER_WRITER_ZLOG_CATEGORY_NODE "ertdlnode"

static const char *default_config_file_name = "ertnode.yaml";
static const char *default_config_paths[] = {
    ".",
    "/etc/ert",
    "/usr/local/etc/ert",
    NULL,
};
static const char *default_log_config_file_name = "zlog.conf";

static volatile ert_node *node_instance = NULL;

static void log_mallinfo(void)
{
  struct mallinfo mi;

  mi = mallinfo();

  ert_log_info("Total non-mmapped bytes (arena):       %d", mi.arena);
  ert_log_info("# of free chunks (ordblks):            %d", mi.ordblks);
  ert_log_info("# of free fastbin blocks (smblks):     %d", mi.smblks);
  ert_log_info("# of mapped regions (hblks):           %d", mi.hblks);
  ert_log_info("Bytes in mapped regions (hblkhd):      %d", mi.hblkhd);
  ert_log_info("Max. total allocated space (usmblks):  %d", mi.usmblks);
  ert_log_info("Free bytes held in fastbins (fsmblks): %d", mi.fsmblks);
  ert_log_info("Total allocated space (uordblks):      %d", mi.uordblks);
  ert_log_info("Total free space (fordblks):           %d", mi.fordblks);
  ert_log_info("Topmost releasable block (keepcost):   %d", mi.keepcost);
}

void comm_protocol_stream_listener_callback(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, void *callback_context)
{
  ert_comm_protocol_stream_info stream_info;

  int result = ert_comm_protocol_stream_get_info(stream, &stream_info);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_get_info failed with result %d", result);
    return;
  }

  ert_log_info("New stream ID %d in port %d", stream_info.stream_id, stream_info.port);
}

void signal_callback(int signum)
{
  ert_log_info("Caught signal: %d, exiting...", signum);

  if (node_instance != NULL) {
    node_instance->running = false;
  }
}

int ert_node_initialize_inputs(ert_node *node)
{
  int result;

  ert_log_info("Initializing sensor registry ...");
  result = ert_sensor_registry_init();
  if (result != 0) {
    ert_log_error("ert_sensor_registry_init failed with result: %d", result);
    return result;
  }

  if (node->config.gps_config.enabled) {
    ert_log_info("Initializing GPS ...");
    char gpsd_port[16];
    snprintf(gpsd_port, 16, "%d", node->config.gps_config.gpsd_port);
    result = ert_gps_open(node->config.gps_config.gpsd_host, gpsd_port, &node->gps);
    if (result != 0) {
      ert_log_error("ert_gps_open failed with result: %d", result);
      return result;
    }

    ert_log_info("Initializing GPS listener routine ...");
    result = ert_gps_listener_start(node->gps, NULL, NULL, 500, &node->gps_listener);
    if (result != 0) {
      ert_log_error("ert_gps_listener_start failed with result: %d", result);
      return result;
    }
  }

  return 0;
}

int ert_node_initialize_comm_device(ert_node *node)
{
  int result;

  ert_log_info("Initializing comm device ...");
  result = rfm9xw_open(&node->config.rfm9xw_static_config,
      &node->config.rfm9xw_config, &node->comm_device);
  if (result != 0) {
    ert_log_error("rfm9xw_open failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_node_initialize_comm_protocol(ert_node *node, ert_comm_device *comm_device)
{
  int result;

  ert_log_info("Initializing comm transceiver routine ...");
  result = ert_comm_transceiver_start(comm_device, &node->config.comm_transceiver_config, &node->comm_transceiver);
  if (result != 0) {
    ert_log_error("ert_comm_transceiver_start failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing comm protocol device ...");
  result = ert_comm_protocol_device_adapter_create(node->comm_transceiver, &node->comm_protocol_device);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_device_adapter_create failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing comm protocol ...");
  result = ert_comm_protocol_create(&node->config.comm_protocol_config, comm_protocol_stream_listener_callback, node,
      node->comm_protocol_device, &node->comm_protocol);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_create failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_node_initialize_data_logger(ert_node *node)
{
  int result;

  ert_log_info("Initializing data logger ...");

  result = ert_data_logger_serializer_msgpack_create(
      &msgpack_serializer_settings[ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_ALL_DATA_WITH_TEXT],
      &node->msgpack_serializer);
  if (result != 0) {
    ert_log_error("ert_data_logger_serializer_msgpack_create failed with result: %d", result);
    return result;
  }

  result = ert_data_logger_serializer_jansson_create(&node->jansson_serializer);
  if (result != 0) {
    ert_log_error("ert_data_logger_serializer_jansson_create failed with result: %d", result);
    return result;
  }

  result = ert_data_logger_writer_zlog_create(ERT_DATA_LOGGER_WRITER_ZLOG_CATEGORY_NODE, &node->zlog_writer);
  if (result != 0) {
    ert_log_error("ert_data_logger_writer_zlog_create failed with result: %d", result);
    return result;
  }

  result = ert_data_logger_create(node->config.device_name, node->config.device_model,
      node->jansson_serializer, node->zlog_writer, &node->data_logger);
  if (result != 0) {
    ert_log_error("ert_data_logger_create failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing data logger data structures ...");
  result = ert_data_logger_init_entry_params(node->data_logger, &node->data_logger_entry_params);
  if (result != 0) {
    ert_log_error("ert_data_logger_init_entry_params failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_node_initialize_gsm_modem(ert_node *node)
{
  if (!node->config.gsm_modem_config.enabled) {
    return 0;
  }

  int result = ert_driver_gsm_modem_open(&node->config.gsm_modem_config.modem_config, &node->gsm_modem);
  if (result < 0) {
    ert_log_error("ert_driver_gsm_modem_open() failed with result %d", result);
    return result;
  }

  ert_driver_gsm_modem_set_operator_selection(node->gsm_modem,
      ERT_DRIVER_GSM_MODEM_NETWORK_SELECTION_MODE_AUTOMATIC,
      ERT_DRIVER_GSM_MODEM_NETWORK_OPERATOR_FORMAT_LONG_ALPHANUMERIC);

  ert_driver_gsm_modem_set_network_registration_report(node->gsm_modem,
      ERT_DRIVER_GSM_MODEM_NETWORK_REGISTRATION_REPORT_MODE_ENABLED_WITH_CID);

  // Attempt to disable Huawei proprietary reporting
  ert_driver_gsm_modem_command command;
  ert_driver_gsm_modem_create_command(node->gsm_modem, &command, "AT^CURC=0", NULL, false);
  ert_driver_gsm_modem_execute_command(node->gsm_modem, &command);

  node->gsm_modem_initialized = true;

  return 0;
}

static int comm_transceiver_send_test_messages(ert_comm_transceiver *comm_transceiver)
{
  int result;
  uint32_t id = 1;

  char buffer[512];
  uint32_t bytes_transmitted;
  for (int i = 0; i < 10; i++) {
    sprintf(buffer, "MESSAGE %04d: SHORT MESSAGE END", id);
    ert_log_info("Transmitting message: \"%s\"", buffer);
    result = ert_comm_transceiver_transmit(comm_transceiver, id++, strlen(buffer), buffer,
        ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_BLOCK, &bytes_transmitted);
    if (result < 0) {
      ert_log_error("ert_comm_transceiver_transmit failed with result %d", result);
    }


    sprintf(buffer, "MESSAGE %04d: LONG MESSAGE 1 LONG MESSAGE 2 LONG MESSAGE 3 LONG MESSAGE 4 LONG MESSAGE 5 LONG MESSAGE 6 LONG MESSAGE 7 LONG MESSAGE 8 LONG MESSAGE 9 LONG MESSAGE A LONG MESSAGE B LONG MESSAGE C LONG MESSAGE D LONG MESSAGE E LONG MESSAGE F LONG MESSAGE END", id);
    ert_log_info("Transmitting message: \"%s\"", buffer);
    result = ert_comm_transceiver_transmit(comm_transceiver, id++, strlen(buffer), buffer,
        ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_BLOCK, &bytes_transmitted);
    if (result < 0) {
      ert_log_error("ert_comm_transceiver_transmit failed with result %d", result);
    }
  }

  return 0;
}

static struct option ert_node_long_options[] = {
    {"config-file", required_argument, NULL, 'c' },
    {"log-config-file", required_argument, NULL, 'l' },
    {"gps-config",  no_argument, NULL, 'g' },
    {"init-only",  no_argument, NULL, 'i' },
    {"help",  no_argument, NULL, 'h' },
    {NULL, 0, NULL, 0 }
};

void ert_node_display_usage(struct option *options)
{
  fprintf(stderr, "Usage:\n");
  for (size_t i = 0; options[i].name != NULL; i++) {
    fprintf(stderr, "  -%c --%s %s\n", options[i].val, options[i].name,
        (options[i].has_arg == required_argument)
        ? "arg"
        : ((options[i].has_arg == optional_argument) ? "[arg]" : ""));
  }
}

int ert_node_process_options(int argc, char **argv,
    char *custom_config_file_name, char *custom_log_config_file_name, bool *init_only, bool *gps_config)
{
  int c;

  opterr = 0;

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "c:l:igh", ert_node_long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 0:
        break;
      case 'c':
        strncpy(custom_config_file_name, optarg, PATH_MAX);
        break;
      case 'l':
        strncpy(custom_log_config_file_name, optarg, PATH_MAX);
        break;
      case 'i':
        *init_only = true;
        break;
      case 'g':
        *gps_config = true;
        break;
      case 'h':
        ert_node_display_usage(ert_node_long_options);
        return -EINVAL;
      case ':':
        fprintf(stderr, "Missing argument for command-line option %c\n", optopt);
        ert_node_display_usage(ert_node_long_options);
        return -EINVAL;
      case '?':
        fprintf(stderr, "Invalid command-line option: %c\n", optopt);
        ert_node_display_usage(ert_node_long_options);
        return -EINVAL;
      default:
        fprintf(stderr, "Invalid command-line option: %c\n", c);
        ert_node_display_usage(ert_node_long_options);
        return -EINVAL;
    }
  }

  return 0;
}

int ert_node_configure(ert_node *node, char *custom_file_name)
{
  if (custom_file_name != NULL && strlen(custom_file_name) > 0) {
    int result = ert_node_read_configuration(&node->config, custom_file_name);
    if (result < 0) {
      ert_log_fatal("Error reading application configuration from file: %s", custom_file_name);
      return -EINVAL;
    }

    return 0;
  }

  char file_name[PATH_MAX];

  for (size_t i = 0; default_config_paths[i] != NULL; i++) {
    snprintf(file_name, PATH_MAX, "%s/%s", default_config_paths[i], default_config_file_name);

    if (access(file_name, F_OK | R_OK) != 0) {
      continue;
    }

    int result = ert_node_read_configuration(&node->config, file_name);
    if (result < 0) {
      ert_log_fatal("Error reading application configuration from file: %s", file_name);
      return -EINVAL;
    }

    return 0;
  }

  ert_log_fatal("Cannot find application configuration from default locations, last file checked: %s (%s)", file_name, strerror(errno));

  return -ENOENT;
}

int ert_node_initialize(ert_node *node)
{
  int result;

  node_instance = node;
  node->running = true;

  if (node->config.sender_image_config.enabled) {
    result = mkdir(node->config.sender_image_config.image_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (result != 0 && errno != EEXIST) {
      ert_log_fatal("Error creating image path directory: %s (%s)", node->config.sender_image_config.image_path,
          strerror(errno));
      return -EIO;
    }
  }

  result = hal_init();
  if (result != 0) {
    ert_log_fatal("hal_init failed with result: %d", result);
    return -EIO;
  }

  result = ert_event_emitter_create(32, 32, &node->event_emitter);
  if (result != 0) {
    ert_log_error("ert_event_emitter_create failed with result: %d", result);
    return -EIO;
  }

  result = ert_node_initialize_inputs(node);
  if (result != 0) {
    return -EIO;
  }

  result = ert_node_initialize_comm_device(node);
  if (result != 0) {
    return -EIO;
  }

  result = ert_node_initialize_comm_protocol(node, node->comm_device);
  if (result != 0) {
    return -EIO;
  }

  result = ert_node_initialize_data_logger(node);
  if (result != 0) {
    return -EIO;
  }

  ert_node_initialize_gsm_modem(node);

  ert_node_sender_telemetry_gsm_initialize(node);

  if (node->config.server_config.enabled) {
    ert_log_info("Initializing HTTP/WebSocket server ...");

    node->config.server_config.event_emitter = node->event_emitter;
    node->config.server_config.data_logger_entry_serializer = node->jansson_serializer;
    node->config.server_config.app_comm_protocol = node->comm_protocol;
    node->config.server_config.app_config_root_entry = ert_node_configuration_mapper_create(&node->config);
    node->config.server_config.app_config_update_context = node;

    result = ert_server_create(&node->config.server_config, &node->server);
    if (result != 0) {
      ert_log_error("ert_server_create failed with result: %d", result);
      return -EIO;
    }
  }

  return 0;
}

int ert_node_run(ert_node *node)
{
  int result;

  if (node->config.sender_telemetry_config.enabled) {
    result = pthread_create(&node->telemetry_collector_thread, NULL, ert_node_telemetry_collector, node);
    if (result != 0) {
      ert_log_error("Error starting telemetry collector thread");
      return -EIO;
    }

    result = pthread_create(&node->telemetry_sender_thread, NULL, ert_node_telemetry_sender_comm, node);
    if (result != 0) {
      ert_log_error("Error starting telemetry sender thread");
      return -EIO;
    }
  }

  if (node->config.sender_image_config.enabled) {
    result = pthread_create(&node->image_collector_thread, NULL, ert_node_image_collector, node);
    if (result != 0) {
      ert_log_error("Error starting image collector thread");
      return -EIO;
    }

    result = pthread_create(&node->image_sender_thread, NULL, ert_node_sender_image_comm, node);
    if (result != 0) {
      ert_log_error("Error starting image sender thread");
      return -EIO;
    }
  }

  if (node->config.server_config.enabled) {
    result = ert_server_start(node->server);
    if (result != 0) {
      ert_log_error("ert_server_start failed with result: %d", result);
      return -EIO;
    }

    ert_node_server_attach_events(node->event_emitter, node->server);
  }

  return 0;
}

int ert_node_wait(ert_node *node)
{
  if (node->config.sender_telemetry_config.enabled) {
    pthread_join(node->telemetry_collector_thread, NULL);
    pthread_join(node->telemetry_sender_thread, NULL);
  }
  if (node->config.sender_image_config.enabled) {
    pthread_join(node->image_collector_thread, NULL);
    pthread_join(node->image_sender_thread, NULL);
  }

  return 0;
}

int ert_node_uninitialize(ert_node *node)
{
  if (node->config.server_config.enabled) {
    ert_server_stop(node->server);
    ert_node_server_detach_events(node->event_emitter);
    ert_server_destroy(node->server);
    ert_mapper_deallocate(node->config.server_config.app_config_root_entry);
  }

  ert_node_sender_telemetry_gsm_uninitialize(node);

  if (node->gsm_modem_initialized) {
    ert_driver_gsm_modem_close(node->gsm_modem);
  }

  ert_data_logger_uninit_entry_params(node->data_logger, &node->data_logger_entry_params);
  ert_data_logger_serializer_msgpack_destroy(node->msgpack_serializer);
  ert_data_logger_serializer_jansson_destroy(node->jansson_serializer);
  ert_data_logger_writer_zlog_destroy(node->zlog_writer);
  ert_data_logger_destroy(node->data_logger);

  ert_comm_protocol_destroy(node->comm_protocol);
  ert_comm_protocol_device_adapter_destroy(node->comm_protocol_device);
  ert_comm_transceiver_stop(node->comm_transceiver);
  if (node->config.gps_config.enabled) {
    ert_gps_listener_stop(node->gps_listener);
  }

  rfm9xw_close(node->comm_device);
  if (node->config.gps_config.enabled) {
    ert_gps_close(node->gps);
  }

  ert_sensor_registry_uninit();

  ert_event_emitter_destroy(node->event_emitter);

  hal_uninit();

  free(node);

  log_mallinfo();

  return 0;
}

int main(int argc, char *argv[])
{
  int result;
  char config_file_name[PATH_MAX];
  char log_config_file_name[PATH_MAX];
  bool init_only = false;
  bool gps_config = false;

  config_file_name[0] = '\0';
  log_config_file_name[0] = '\0';

  ert_process_register_backtrace_handler();

  result = ert_node_process_options(argc, argv, config_file_name, log_config_file_name, &init_only, &gps_config);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  if (strlen(log_config_file_name) == 0) {
    strncpy(log_config_file_name, default_log_config_file_name, PATH_MAX);
  }

  if (access(log_config_file_name, F_OK | R_OK) != 0) {
    fprintf(stderr, "Cannot access log config file: %s\n", log_config_file_name);
    return EXIT_FAILURE;
  }

  result = ert_log_init(log_config_file_name);
  if (result != 0) {
    fprintf(stderr, "ert_log_init failed with result: %d\n", result);
    return EXIT_FAILURE;
  }

  ert_log_info("ERTnode initializing");

  log_mallinfo();

  signal(SIGINT, signal_callback);
  signal(SIGQUIT, signal_callback);
  signal(SIGTERM, signal_callback);

  ert_node *node = calloc(1, sizeof(ert_node));
  if (node == NULL) {
    ert_log_fatal("Error allocating memory for node struct: %s", strerror(errno));
    return EXIT_FAILURE;
  }

  // Set config defaults
  ert_comm_protocol_create_default_config(&node->config.comm_protocol_config);
  node->config.comm_transceiver_config.transmit_buffer_length_packets = 16;
  node->config.comm_transceiver_config.receive_buffer_length_packets = 16;
  node->config.comm_transceiver_config.transmit_timeout_milliseconds = 30000;
  node->config.comm_transceiver_config.poll_interval_milliseconds = 1000;
  node->config.comm_transceiver_config.maximum_receive_time_milliseconds =
      node->config.comm_protocol_config.stream_acknowledgement_receive_timeout_millis * 5;

  result = ert_node_configure(node, config_file_name);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  if (gps_config) {
    hal_serial_device_config serial_device_config;
    strncpy(serial_device_config.device, node->config.gps_config.serial_device_file, 256);
    serial_device_config.speed = node->config.gps_config.serial_device_speed;
    serial_device_config.parity = false;
    serial_device_config.stop_bits_2 = false;

    ert_log_info("Attempting to configured GPS in port %s ...", serial_device_config.device);

    result = ert_gps_ublox_configure(&serial_device_config);
    if (result < 0) {
      return EXIT_FAILURE;
    }

    ert_log_info("GPS configured successfully");

    return EXIT_SUCCESS;
  }

  result = ert_node_initialize(node);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  ert_log_info("ERTnode initialized");

  log_mallinfo();

  if (init_only) {
    ert_log_info("Requested only to initialize application, exiting ...");
  } else {
    result = ert_node_run(node);
    if (result < 0) {
      return EXIT_FAILURE;
    }

    ert_log_info("ERTnode running");

    ert_node_wait(node);
  }

  ert_log_info("ERTnode uninitializing");

  log_mallinfo();

  ert_node_uninitialize(node);

  ert_log_info("ERTnode stopped");

  ert_log_uninit();

  return EXIT_SUCCESS;
}
