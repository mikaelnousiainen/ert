/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <wait.h>
#include <execinfo.h>
#include <signal.h>

#include "ert-process.h"
#include "ert-log.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

#define RUN_COMMAND_OUTPUT_BUFFER_LENGTH 16384
#define RUN_COMMAND_ARGS_BUFFER_LENGTH 4096

int ert_process_run(const char *command, const char *args[], int *status, size_t output_buffer_length,
    char *output_buffer, size_t *bytes_read)
{
  int result;

  int pipefd[2];
  result = pipe(pipefd);
  if (result < 0) {
    ert_log_error("Error creating pipe for child process");
    return -1;
  }


  pid_t pid = fork();
  if (pid < 0) {
    ert_log_error("Error forking process");
    return -1;
  }

  if (pid == 0) {
    // Child process

    // Redirect stdout and stderr to pipe
    result = dup2(pipefd[PIPE_WRITE], STDOUT_FILENO);
    if (result < 0) {
      fprintf(stderr, "dup2() failed to redirect stdout to pipe with error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    result = dup2(pipefd[PIPE_WRITE], STDERR_FILENO);
    if (result < 0) {
      fprintf(stderr, "dup2() failed to redirect stderr to pipe with error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    // Close remaining file descriptors
    close(pipefd[PIPE_READ]);
    close(pipefd[PIPE_WRITE]);

    result = execv(command, (char * const *) args);
    if (result < 0) {
      fprintf(stderr, "execv() failed with error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  } else {
    // Parent process
    close(pipefd[PIPE_WRITE]);

    if (output_buffer_length > 0 && output_buffer != NULL) {
      size_t block_size = 1024;
      size_t offset = 0;

      ssize_t read_result;
      do {
        size_t bytes_remaining = output_buffer_length - offset;
        size_t bytes_to_read = block_size < bytes_remaining ? block_size : bytes_remaining;
        read_result = read(pipefd[PIPE_READ], output_buffer + offset, bytes_to_read);
        if (read_result > 0) {
          offset += read_result;
        }
      } while (read_result > 0);

      *bytes_read = offset;
    }

    close(pipefd[PIPE_READ]);

    pid_t wait_result = waitpid(pid, status, 0);
    if (wait_result < 1) {
      ert_log_error("waitpid() call failed: %s", strerror(errno));
      return -2;
    }
  }

  return 0;
}

int ert_process_run_command(const char *command, const char *args[])
{
  size_t output_buffer_length = RUN_COMMAND_OUTPUT_BUFFER_LENGTH;
  char output_buffer[output_buffer_length];
  size_t output_buffer_data_length;

  size_t args_string_length = RUN_COMMAND_ARGS_BUFFER_LENGTH;
  char args_string[args_string_length];

  char *args_string_position = args_string;
  for (size_t i = 0 ; args[i] != NULL ; i++) {
    args_string_position += snprintf(args_string_position, args_string_length - ((uintptr_t) args_string_position - (uintptr_t) args_string),
        "%s ", args[i]);
  }

  ert_log_debug("Running command: '%s %s'", command, args_string);

  int status;
  int result = ert_process_run(command, args, &status, output_buffer_length, output_buffer, &output_buffer_data_length);
  if (result < 0) {
    ert_log_error("Failed to run command: %s %s", command, args_string);
    return result;
  }

  output_buffer[output_buffer_data_length] = '\0';

  if (status != 0) {
    ert_log_error("Command '%s %s' finished with error status %d and output: '%s'", command, args_string, status, output_buffer);
  } else {
    ert_log_debug("Command '%s %s' finished with successfully with status %d and output: '%s'", command, args_string, status, output_buffer);
  }

  return status;
}

void ert_process_sleep_with_interrupt(unsigned int seconds, volatile bool *running)
{
  unsigned int current_seconds = 0;

  while (*running && current_seconds < seconds) {
    sleep(1);
    current_seconds++;
  }
}

void ert_process_backtrace_handler(int sig)
{
  void *array[50];

  fprintf(stderr, "Error: signal %d:\n", sig);

  int size = backtrace(array, 50);
  char **strings = backtrace_symbols(array, size);

  fprintf(stderr, "Obtained %d stack frames:\n", size);

  for (int i = 0; i < size; i++) {
    fprintf(stderr, "  %s\n", strings[i]);
  }

  free(strings);

  _exit(1);
}

void ert_process_register_backtrace_handler()
{
  signal(SIGSEGV, ert_process_backtrace_handler);
  signal(SIGBUS, ert_process_backtrace_handler);
}
