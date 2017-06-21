/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-comm-protocol-device-adapter.h"

static void ert_comm_protocol_device_adapter_transceiver_receive_callback(
    uint32_t length, uint8_t *data, void *callback_context)
{
  ert_comm_protocol_device *comm_protocol_device = (ert_comm_protocol_device *) callback_context;
  ert_comm_protocol_device_adapter *adapter = (ert_comm_protocol_device_adapter *) comm_protocol_device->priv;

  if (adapter->receive_callback != NULL) {
    adapter->receive_callback(length, data, adapter->receive_callback_context);
  }
}

static int ert_comm_protocol_device_adapter_set_receive_callback(ert_comm_protocol_device *comm_protocol_device,
    ert_comm_protocol_device_receive_callback receive_callback, void *receive_callback_context)
{
  ert_comm_protocol_device_adapter *adapter = (ert_comm_protocol_device_adapter *) comm_protocol_device->priv;

  adapter->receive_callback = receive_callback;
  adapter->receive_callback_context = receive_callback_context;

  return 0;
}

static int ert_comm_protocol_device_adapter_set_receive_active(ert_comm_protocol_device *comm_protocol_device,
    bool receive_active)
{
  ert_comm_protocol_device_adapter *adapter = (ert_comm_protocol_device_adapter *) comm_protocol_device->priv;

  return ert_comm_transceiver_set_receive_active(adapter->comm_transceiver, receive_active);
}

static uint32_t ert_comm_protocol_device_adapter_get_max_packet_length(ert_comm_protocol_device *comm_protocol_device)
{
  ert_comm_protocol_device_adapter *adapter = (ert_comm_protocol_device_adapter *) comm_protocol_device->priv;

  return ert_comm_transceiver_get_max_packet_length(adapter->comm_transceiver);
}

static int ert_comm_protocol_device_adapter_write_packet(ert_comm_protocol_device *comm_protocol_device,
    uint32_t length, uint8_t *data, uint32_t flags, uint32_t *bytes_written)
{
  ert_comm_protocol_device_adapter *adapter = (ert_comm_protocol_device_adapter *) comm_protocol_device->priv;

  // TODO: atomic increment
  uint32_t id = adapter->transmit_packet_id++;

  uint32_t transmit_flags = (uint32_t) ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_BLOCK
                            | ((flags & ERT_COMM_PROTOCOL_DEVICE_WRITE_PACKET_FLAG_SET_RECEIVE_ACTIVE) ? ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_SET_RECEIVE_ACTIVE : 0);

  return ert_comm_transceiver_transmit(adapter->comm_transceiver, id, length, data, transmit_flags, bytes_written);
}

int ert_comm_protocol_device_adapter_create(ert_comm_transceiver *comm_transceiver,
    ert_comm_protocol_device **comm_protocol_device_rcv)
{
  ert_comm_protocol_device *comm_protocol_device = calloc(1, sizeof(ert_comm_protocol_device));
  if (comm_protocol_device == NULL) {
    ert_log_fatal("Error allocating memory for comm protocol device struct: %s", strerror(errno));
    return -ENOMEM;
  }

  ert_comm_protocol_device_adapter *adapter = calloc(1, sizeof(ert_comm_protocol_device_adapter));
  if (adapter == NULL) {
    free(comm_protocol_device);
    ert_log_fatal("Error allocating memory for comm protocol device adapter struct: %s", strerror(errno));
    return -ENOMEM;
  }

  adapter->comm_transceiver = comm_transceiver;
  adapter->transmit_packet_id = 1;
  adapter->receive_callback = NULL;
  adapter->receive_callback_context = NULL;

  comm_protocol_device->priv = adapter;
  comm_protocol_device->get_max_packet_length = ert_comm_protocol_device_adapter_get_max_packet_length;
  comm_protocol_device->set_receive_callback = ert_comm_protocol_device_adapter_set_receive_callback;
  comm_protocol_device->set_receive_active = ert_comm_protocol_device_adapter_set_receive_active;
  comm_protocol_device->write_packet = ert_comm_protocol_device_adapter_write_packet;
  comm_protocol_device->close = ert_comm_protocol_device_adapter_destroy;

  ert_comm_transceiver_set_receive_callback(comm_transceiver,
      ert_comm_protocol_device_adapter_transceiver_receive_callback, comm_protocol_device);

  *comm_protocol_device_rcv = comm_protocol_device;

  return 0;
}

int ert_comm_protocol_device_adapter_destroy(ert_comm_protocol_device *comm_protocol_device)
{
  ert_comm_protocol_device_adapter *adapter = (ert_comm_protocol_device_adapter *) comm_protocol_device->priv;

  ert_comm_transceiver_set_receive_callback(adapter->comm_transceiver, NULL, NULL);

  free(adapter);
  free(comm_protocol_device);

  return 0;
}
