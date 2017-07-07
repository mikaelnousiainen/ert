#ifndef __ERT_FLIGHT_MANAGER_H
#define __ERT_FLIGHT_MANAGER_H

#include "ert-gps.h"

#define ERT_FLIGHT_MANAGER_ALTITUDE_THRESHOLD_METERS 100
#define ERT_FLIGHT_MANAGER_CLIMB_RATE_THRESHOLD_METERS_PER_SEC 1.0f
#define ERT_FLIGHT_MANAGER_INVALID_DATA_COUNTER_MAXIMUM 30

typedef enum _ert_flight_state {
  ERT_FLIGHT_STATE_IDLE = 0,
  ERT_FLIGHT_STATE_LAUNCHED = 10,
  ERT_FLIGHT_STATE_ASCENDING = 20,
  ERT_FLIGHT_STATE_FLOATING = 30,
  ERT_FLIGHT_STATE_BURST = 40,
  ERT_FLIGHT_STATE_DESCENDING = 50,
  ERT_FLIGHT_STATE_LANDED = 60,
  ERT_FLIGHT_STATE_UNKNOWN = 100,
} ert_flight_state;

typedef struct _ert_flight_data {
  ert_flight_state flight_state;
  double maximum_altitude_meters;
  double minimum_altitude_meters;
  double climb_rate_meters_per_sec;
} ert_flight_data;

typedef struct _ert_flight_manager ert_flight_manager;

int ert_flight_manager_create(ert_flight_manager **flight_manager_rcv);
int ert_flight_manager_destroy(ert_flight_manager *flight_manager);
int ert_flight_manager_is_known_state(ert_flight_manager *flight_manager);
int ert_flight_manager_is_unknown_state(ert_flight_manager *flight_manager);
ert_flight_state ert_flight_manager_get_state(ert_flight_manager *flight_manager);
void ert_flight_manager_get_flight_data(ert_flight_manager *flight_manager, ert_flight_data *flight_data_rcv);
void ert_flight_manager_process(ert_flight_manager *flight_manager, ert_gps_data *gps_data);

#endif
