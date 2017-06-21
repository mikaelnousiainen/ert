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
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <malloc.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>

#include "ertgateway.h"
#include "ertgateway-config.h"
#include "ertgateway-handler-telemetry-node.h"
#include "ertgateway-handler-telemetry-gateway.h"
#include "ertgateway-handler-image.h"
#include "ertgateway-handler-display.h"
#include "ertgateway-server.h"
#include "ertgateway-stream-listener.h"

#define ERT_DATA_LOGGER_WRITER_ZLOG_CATEGORY_NODE "ertdlnode"
#define ERT_DATA_LOGGER_WRITER_ZLOG_CATEGORY_GATEWAY "ertdlgateway"

static const char *default_config_file_name = "ertgateway.yaml";
static const char *default_config_paths[] = {
    ".",
    "/etc/ert",
    "/usr/local/etc/ert",
    NULL,
};
static const char *default_log_config_file_name = "zlog.conf";

static volatile ert_gateway *gateway_instance = NULL;

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

void signal_callback(int signum)
{
  ert_log_info("Caught signal: %d, exiting...", signum);

  if (gateway_instance != NULL) {
    gateway_instance->running = false;

    if (gateway_instance->image_stream_queue != NULL) {
      ert_pipe_close(gateway_instance->image_stream_queue);
    }
    if (gateway_instance->telemetry_stream_queue != NULL) {
      ert_pipe_close(gateway_instance->telemetry_stream_queue);
    }
  }
}

int ert_gateway_initialize_inputs(ert_gateway *gateway)
{
  int result;

  ert_log_info("Initializing sensor registry ...");
  result = ert_sensor_registry_init();
  if (result != 0) {
    ert_log_error("ert_sensor_registry_init failed with result: %d", result);
    return result;
  }

#ifdef ERTGATEWAY_SUPPORT_GPSD
  if (gateway->config.gps_config.enabled) {
    ert_log_info("Initializing GPS ...");
    char gpsd_port[16];
    snprintf(gpsd_port, 16, "%d", gateway->config.gps_config.gpsd_port);
    result = ert_gps_open(gateway->config.gps_config.gpsd_host, gpsd_port, &gateway->gps);
    if (result != 0) {
      ert_log_error("ert_gps_open failed with result: %d", result);
      return result;
    }

    ert_log_info("Initializing GPS listener routine ...");
    result = ert_gps_listener_start(gateway->gps, NULL, NULL, 500, &gateway->gps_listener);
    if (result != 0) {
      ert_log_error("ert_gps_listener_start failed with result: %d", result);
      return result;
    }
  }
#endif

  return 0;
}

int ert_gateway_initialize_comm_device(ert_gateway *gateway)
{
  int result;

  ert_log_info("Initializing comm device ...");
  result = rfm9xw_open(
      &gateway->config.rfm9xw_static_config,
      &gateway->config.rfm9xw_config, &gateway->comm_device);
  if (result != 0) {
    ert_log_error("rfm9xw_open failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_gateway_initialize_comm_protocol(ert_gateway *gateway, ert_comm_device *comm_device)
{
  int result;

  ert_log_info("Initializing comm transceiver routine ...");
  result = ert_comm_transceiver_start(comm_device, &gateway->config.comm_transceiver_config, &gateway->comm_transceiver);
  if (result != 0) {
    ert_log_error("ert_comm_transceiver_start failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing comm protocol device ...");
  result = ert_comm_protocol_device_adapter_create(gateway->comm_transceiver, &gateway->comm_protocol_device);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_device_adapter_create failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing comm protocol ...");
  result = ert_comm_protocol_create(&gateway->config.comm_protocol_config, ert_gateway_stream_listener_callback, gateway,
      gateway->comm_protocol_device, &gateway->comm_protocol);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_create failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_gateway_initialize_data_logger(ert_gateway *gateway)
{
  int result;

  result = pthread_mutex_init(&gateway->related_entry_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing mutex for related entry, result %d", result);
    return -EIO;
  }

  ert_log_info("Initializing Jansson serializer ...");
  result = ert_data_logger_serializer_jansson_create(&gateway->jansson_serializer);
  if (result != 0) {
    ert_log_error("ert_data_logger_serializer_jansson_create failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing zlog writers ...");
  result = ert_data_logger_writer_zlog_create(ERT_DATA_LOGGER_WRITER_ZLOG_CATEGORY_NODE, &gateway->zlog_writer_node);
  if (result != 0) {
    ert_log_error("ert_data_logger_writer_zlog_create failed with result: %d", result);
    return result;
  }
  result = ert_data_logger_writer_zlog_create(ERT_DATA_LOGGER_WRITER_ZLOG_CATEGORY_GATEWAY, &gateway->zlog_writer_gateway);
  if (result != 0) {
    ert_log_error("ert_data_logger_writer_zlog_create failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing data loggers ...");
  result = ert_data_logger_create(gateway->config.device_name, gateway->config.device_model,
      gateway->jansson_serializer, gateway->zlog_writer_node, &gateway->data_logger_node);
  if (result != 0) {
    ert_log_error("ert_data_logger_create failed with result: %d", result);
    return result;
  }
  result = ert_data_logger_create(gateway->config.device_name, gateway->config.device_model,
      gateway->jansson_serializer, gateway->zlog_writer_gateway, &gateway->data_logger_gateway);
  if (result != 0) {
    ert_log_error("ert_data_logger_create failed with result: %d", result);
    return result;
  }

  ert_log_info("Initializing data logger data structures ...");
  result = ert_data_logger_init_entry_params(gateway->data_logger_node, &gateway->data_logger_entry_params_node);
  if (result != 0) {
    ert_log_error("ert_data_logger_init_entry_params failed with result: %d", result);
    return result;
  }
  result = ert_data_logger_init_entry_params(gateway->data_logger_gateway, &gateway->data_logger_entry_params_gateway);
  if (result != 0) {
    ert_log_error("ert_data_logger_init_entry_params failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_gateway_initialize_queues(ert_gateway *gateway)
{
  int result;

  result = ert_pipe_create(sizeof(ert_comm_protocol_stream *), 32, &gateway->telemetry_stream_queue);
  if (result != 0) {
    ert_log_error("ert_pipe_create failed with result: %d", result);
    return result;
  }

  result = ert_pipe_create(sizeof(ert_comm_protocol_stream *), 32, &gateway->image_stream_queue);
  if (result != 0) {
    ert_log_error("ert_pipe_create failed with result: %d", result);
    return result;
  }

  return 0;
}

static struct option ert_gateway_long_options[] = {
    {"config-file", required_argument, NULL, 'c' },
    {"log-config-file", required_argument, NULL, 'l' },
    {"init-only",  no_argument, NULL, 'i' },
    {"help",  no_argument, NULL, 'h' },
    {0, 0, NULL, 0 }
};

void ert_gateway_display_usage(struct option *options)
{
  fprintf(stderr, "Usage:\n");
  for (size_t i = 0; options[i].name != NULL; i++) {
    fprintf(stderr, "  -%c --%s %s\n", options[i].val, options[i].name,
        (options[i].has_arg == required_argument)
        ? "arg"
        : ((options[i].has_arg == optional_argument) ? "[arg]" : ""));
  }
}

int ert_gateway_process_options(int argc, char *argv[],
    char *custom_config_file_name, char *custom_log_config_file_name, bool *init_only)
{
  int c;

  opterr = 0;

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "c:l:ih", ert_gateway_long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 0:
        printf("option %s", ert_gateway_long_options[option_index].name);
        if (optarg)
          printf(" with arg %s", optarg);
        printf("\n");
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
      case 'h':
        ert_gateway_display_usage(ert_gateway_long_options);
        return -EINVAL;
      case ':':
        fprintf(stderr, "Missing argument for command-line option %c\n", optopt);
        ert_gateway_display_usage(ert_gateway_long_options);
        return -EINVAL;
      case '?':
        fprintf(stderr, "Invalid command-line option: %c\n", optopt);
        ert_gateway_display_usage(ert_gateway_long_options);
        return -EINVAL;
      default:
        fprintf(stderr, "Invalid command-line option: %c\n", c);
        ert_gateway_display_usage(ert_gateway_long_options);
        return -EINVAL;
    }
  }

  return 0;
}

int ert_gateway_configure(ert_gateway *gateway, char *custom_file_name)
{
  if (custom_file_name != NULL && strlen(custom_file_name) > 0) {
    int result = ert_gateway_read_configuration(&gateway->config, custom_file_name);
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

    int result = ert_gateway_read_configuration(&gateway->config, file_name);
    if (result < 0) {
      ert_log_fatal("Error reading application configuration from file: %s", file_name);
      return -EINVAL;
    }

    return 0;
  }

  ert_log_fatal("Cannot find application configuration from default locations, last file checked: %s (%s)", file_name, strerror(errno));

  return -ENOENT;
}

int ert_gateway_initialize(ert_gateway *gateway)
{
  int result;

  gateway_instance = gateway;
  gateway->running = true;

  result = mkdir(gateway->config.handler_image_config.image_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (result != 0 && errno != EEXIST) {
    ert_log_fatal("Error creating image path directory: %s (%s)", gateway->config.handler_image_config.image_path,
        strerror(errno));
    return -EIO;
  }

  result = hal_init();
  if (result != 0) {
    ert_log_fatal("hal_init failed with result: %d", result);
    return -EIO;
  }

  result = ert_event_emitter_create(32, 32, &gateway->event_emitter);
  if (result != 0) {
    ert_log_error("ert_event_emitter_create failed with result: %d", result);
    return -EIO;
  }

  result = ert_gateway_initialize_inputs(gateway);
  if (result != 0) {
    return -EIO;
  }

  result = ert_gateway_initialize_queues(gateway);
  if (result != 0) {
    return -EIO;
  }

  result = ert_gateway_initialize_comm_device(gateway);
  if (result != 0) {
    return -EIO;
  }

  result = ert_gateway_initialize_comm_protocol(gateway, gateway->comm_device);
  if (result != 0) {
    return -EIO;
  }

  result = ert_gateway_initialize_data_logger(gateway);
  if (result != 0) {
    return -EIO;
  }

  result = ert_data_logger_serializer_msgpack_create(
      &msgpack_serializer_settings[ERT_DATA_LOGGER_SERIALIZER_MSGPACK_SETTINGS_MINIMAL],
      &gateway->msgpack_serializer);
  if (result != 0) {
    ert_log_error("ert_data_logger_serializer_msgpack_create failed with result: %d", result);
    return -EIO;
  }

  gateway->image_index = 1;

  if (gateway->config.server_config.enabled) {
    ert_log_info("Initializing HTTP/WebSocket server ...");

    gateway->config.server_config.event_emitter = gateway->event_emitter;
    gateway->config.server_config.data_logger_entry_serializer = gateway->jansson_serializer;
    gateway->config.server_config.app_comm_protocol = gateway->comm_protocol;
    gateway->config.server_config.app_config_root_entry = ert_gateway_configuration_mapper_create(&gateway->config);
    gateway->config.server_config.app_config_update_context = gateway;

    result = ert_server_create(&gateway->config.server_config, &gateway->server);
    if (result != 0) {
      ert_log_error("ert_server_create failed with result: %d", result);
      return -EIO;
    }
  }

  ert_comm_transceiver_set_receive_active(gateway->comm_transceiver, true);

  return 0;
}

int ert_gateway_run(ert_gateway *gateway)
{
  int result;
  uint32_t thread_count = gateway->config.thread_count_per_comm_handler;
  uint32_t total_thread_count = 2 * thread_count;

  ert_log_info("Starting %d threads for each comm stream handler, %d in total", thread_count, total_thread_count);

  for (uint32_t i = 0; i < thread_count; i++) {
    result = pthread_create(&gateway->node_telemetry_handler_threads[i], NULL, ert_gateway_handler_telemetry_node, gateway);
    if (result != 0) {
      ert_log_error("Error starting telemetry handler thread");
      return -EIO;
    }

    result = pthread_create(&gateway->image_handler_threads[i], NULL, ert_gateway_image_handler, gateway);
    if (result != 0) {
      ert_log_error("Error starting image handler thread");
      return -EIO;
    }
  }

  ert_log_info("Started %d threads", total_thread_count);

  result = pthread_create(&gateway->gateway_telemetry_handler_thread, NULL, ert_gateway_handler_telemetry_gateway, gateway);
  if (result != 0) {
    ert_log_error("Error starting gateway telemetry handler thread");
    return -EIO;
  }

  result = pthread_create(&gateway->display_handler_thread, NULL, ert_gateway_handler_display, gateway);
  if (result != 0) {
    ert_log_error("Error starting display handler thread");
    return -EIO;
  }

  if (gateway->config.server_config.enabled) {
    result = ert_server_start(gateway->server);
    if (result != 0) {
      ert_log_error("ert_server_start failed with result: %d", result);
      return -EIO;
    }

    ert_gateway_server_attach_events(gateway->event_emitter, gateway->server);
  }

  return 0;
}

int ert_gateway_wait(ert_gateway *gateway)
{
  uint32_t thread_count = gateway->config.thread_count_per_comm_handler;

  for (uint32_t i = 0; i < thread_count; i++) {
    pthread_join(gateway->node_telemetry_handler_threads[i], NULL);
    pthread_join(gateway->image_handler_threads[i], NULL);
  }

  pthread_join(gateway->gateway_telemetry_handler_thread, NULL);
  pthread_join(gateway->display_handler_thread, NULL);

  return 0;
}

int ert_gateway_uninitialize(ert_gateway *gateway)
{
  if (gateway->config.server_config.enabled) {
    ert_server_stop(gateway->server);
    ert_gateway_server_detach_events(gateway->event_emitter);
    ert_server_destroy(gateway->server);
    ert_mapper_deallocate(gateway->config.server_config.app_config_root_entry);
  }

  ert_data_logger_uninit_entry_params(gateway->data_logger_node, &gateway->data_logger_entry_params_node);
  ert_data_logger_serializer_msgpack_destroy(gateway->msgpack_serializer);
  ert_data_logger_serializer_jansson_destroy(gateway->jansson_serializer);
  ert_data_logger_writer_zlog_destroy(gateway->zlog_writer_node);
  ert_data_logger_destroy(gateway->data_logger_node);
  pthread_mutex_destroy(&gateway->related_entry_mutex);

  ert_comm_protocol_destroy(gateway->comm_protocol);
  ert_comm_protocol_device_adapter_destroy(gateway->comm_protocol_device);
  ert_comm_transceiver_stop(gateway->comm_transceiver);
#ifdef ERTGATEWAY_SUPPORT_GPSD
  if (gateway->config.gps_config.enabled) {
    ert_gps_listener_stop(gateway->gps_listener);
  }
#endif

  rfm9xw_close(gateway->comm_device);
#ifdef ERTGATEWAY_SUPPORT_GPSD
  if (gateway->config.gps_config.enabled) {
    ert_gps_close(gateway->gps);
  }
#endif

  ert_pipe_destroy(gateway_instance->image_stream_queue);
  ert_pipe_destroy(gateway_instance->telemetry_stream_queue);

  ert_log_logger_destroy(gateway->display_logger);

  ert_sensor_registry_uninit();

  ert_event_emitter_destroy(gateway->event_emitter);

  hal_uninit();

  free(gateway);

  log_mallinfo();

  return 0;
}

int main(int argc, char *argv[])
{
  int result;
  char config_file_name[PATH_MAX];
  char log_config_file_name[PATH_MAX];
  bool init_only = false;

  config_file_name[0] = '\0';
  log_config_file_name[0] = '\0';

  ert_process_register_backtrace_handler();

  result = ert_gateway_process_options(argc, argv, config_file_name, log_config_file_name, &init_only);
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

  ert_log_info("ERTgateway initializing");

  log_mallinfo();

  signal(SIGINT, signal_callback);
  signal(SIGQUIT, signal_callback);
  signal(SIGTERM, signal_callback);

  ert_gateway *gateway = calloc(1, sizeof(ert_gateway));
  if (gateway == NULL) {
    ert_log_fatal("Error allocating memory for gateway struct: %s", strerror(errno));
    return EXIT_FAILURE;
  }

  result = ert_log_logger_create("ertdisplay", &gateway->display_logger);
  if (result < 0) {
    ert_log_fatal("Error initializing gateway display logger, result %d", result);
    return EXIT_FAILURE;
  }

  // Set config defaults
  ert_comm_protocol_create_default_config(&gateway->config.comm_protocol_config);
  gateway->config.comm_protocol_config.receive_buffer_length_packets = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT * 2;

  gateway->config.comm_transceiver_config.transmit_buffer_length_packets = 16;
  gateway->config.comm_transceiver_config.receive_buffer_length_packets = 64;
  gateway->config.comm_transceiver_config.transmit_timeout_milliseconds = 30000;
  gateway->config.comm_transceiver_config.poll_interval_milliseconds = 1000;

  result = ert_gateway_configure(gateway, config_file_name);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  result = ert_gateway_initialize(gateway);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  ert_log_info("ERTgateway initialized");

  log_mallinfo();

  if (init_only) {
    ert_log_info("Requested only to initialize application, exiting ...");
  } else {
    result = ert_gateway_run(gateway);
    if (result < 0) {
      return EXIT_FAILURE;
    }

    ert_log_info("ERTgateway running");

    ert_gateway_wait(gateway);
  }

  ert_log_info("ERTgateway uninitializing ...");

  log_mallinfo();

  ert_gateway_uninitialize(gateway);

  ert_log_info("ERTgateway stopped");

  ert_log_uninit();

  return EXIT_SUCCESS;
}
