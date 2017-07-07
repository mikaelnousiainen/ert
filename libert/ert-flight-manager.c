#include <string.h>
#include <errno.h>
#include <math.h>

#include "ert-flight-manager.h"
#include "ert-log.h"

struct _ert_flight_manager {
  ert_flight_data flight_data;
  uint32_t invalid_data_counter;
};

int ert_flight_manager_create(ert_flight_manager **flight_manager_rcv)
{
  ert_flight_manager *flight_manager = calloc(1, sizeof(ert_flight_manager));
  if (flight_manager == NULL) {
    ert_log_fatal("Error allocating memory for flight manager struct: %s", strerror(errno));
    return -ENOMEM;
  }

  flight_manager->flight_data.flight_state = ERT_FLIGHT_STATE_IDLE;

  *flight_manager_rcv = flight_manager;

  return 0;
}

int ert_flight_manager_destroy(ert_flight_manager *flight_manager)
{
  free(flight_manager);

  return 0;
}

int ert_flight_manager_is_known_state(ert_flight_manager *flight_manager)
{
  return flight_manager->invalid_data_counter < ERT_FLIGHT_MANAGER_INVALID_DATA_COUNTER_MAXIMUM;
}

int ert_flight_manager_is_unknown_state(ert_flight_manager *flight_manager)
{
  return !ert_flight_manager_is_known_state(flight_manager);
}

ert_flight_state ert_flight_manager_get_state(ert_flight_manager *flight_manager)
{
  if (ert_flight_manager_is_unknown_state(flight_manager)) {
    return ERT_FLIGHT_STATE_UNKNOWN;
  }

  return flight_manager->flight_data.flight_state;
}

void ert_flight_manager_get_flight_data(ert_flight_manager *flight_manager, ert_flight_data *flight_data_rcv)
{
  memcpy(flight_data_rcv, &flight_manager->flight_data, sizeof(ert_flight_data));
  flight_data_rcv->flight_state = ert_flight_manager_get_state(flight_manager);
}

void ert_flight_manager_process(ert_flight_manager *flight_manager, ert_gps_data *gps_data)
{
  if (!gps_data->has_fix || !isfinite(gps_data->climb_meters_per_sec) || !isfinite(gps_data->altitude_meters)) {
    flight_manager->invalid_data_counter++;
    return;
  }

  flight_manager->invalid_data_counter = 0;

  ert_flight_data *flight_data = &flight_manager->flight_data;

  flight_data->maximum_altitude_meters = fmax(flight_data->maximum_altitude_meters, gps_data->altitude_meters);
  flight_data->minimum_altitude_meters = fmin(flight_data->minimum_altitude_meters, gps_data->altitude_meters);

  flight_data->climb_rate_meters_per_sec =
      (flight_data->climb_rate_meters_per_sec * 0.75f) + (gps_data->climb_meters_per_sec * 0.25f);

  if (flight_data->climb_rate_meters_per_sec >= ERT_FLIGHT_MANAGER_CLIMB_RATE_THRESHOLD_METERS_PER_SEC) {
    switch (flight_data->flight_state) {
      case ERT_FLIGHT_STATE_IDLE:
        flight_data->flight_state = ERT_FLIGHT_STATE_LAUNCHED;
        break;
      case ERT_FLIGHT_STATE_LAUNCHED:
        if (gps_data->altitude_meters >= flight_data->minimum_altitude_meters + ERT_FLIGHT_MANAGER_ALTITUDE_THRESHOLD_METERS) {
          flight_data->flight_state = ERT_FLIGHT_STATE_ASCENDING;
        }
        break;
      case ERT_FLIGHT_STATE_LANDED:
        // Start from idle
        flight_data->flight_state = ERT_FLIGHT_STATE_IDLE;
        break;
      default:
        flight_data->flight_state = ERT_FLIGHT_STATE_ASCENDING;
    }
  } else if (flight_data->climb_rate_meters_per_sec <= -ERT_FLIGHT_MANAGER_CLIMB_RATE_THRESHOLD_METERS_PER_SEC) {
    if (gps_data->altitude_meters >= flight_data->maximum_altitude_meters - ERT_FLIGHT_MANAGER_ALTITUDE_THRESHOLD_METERS) {
      switch (flight_data->flight_state) {
        case ERT_FLIGHT_STATE_IDLE:
        case ERT_FLIGHT_STATE_LANDED:
        case ERT_FLIGHT_STATE_DESCENDING:
          // Do not change state - impossible transition
          break;
        case ERT_FLIGHT_STATE_LAUNCHED:
        case ERT_FLIGHT_STATE_ASCENDING:
        case ERT_FLIGHT_STATE_FLOATING:
        default:
          flight_data->flight_state = ERT_FLIGHT_STATE_BURST;
      }
    } else {
      switch (flight_data->flight_state) {
        case ERT_FLIGHT_STATE_IDLE:
          break;
        case ERT_FLIGHT_STATE_LANDED:
          // Start from idle
          flight_data->flight_state = ERT_FLIGHT_STATE_IDLE;
          break;
        case ERT_FLIGHT_STATE_LAUNCHED:
        case ERT_FLIGHT_STATE_ASCENDING:
        case ERT_FLIGHT_STATE_FLOATING:
        case ERT_FLIGHT_STATE_BURST:
        default:
          flight_data->flight_state = ERT_FLIGHT_STATE_DESCENDING;
      }
    }
  } else {
    switch (flight_data->flight_state) {
      case ERT_FLIGHT_STATE_IDLE:
      case ERT_FLIGHT_STATE_LAUNCHED:
      case ERT_FLIGHT_STATE_LANDED:
        // Do not change state
        break;
      case ERT_FLIGHT_STATE_ASCENDING:
      case ERT_FLIGHT_STATE_FLOATING:
        flight_data->flight_state = ERT_FLIGHT_STATE_FLOATING;
        break;
      case ERT_FLIGHT_STATE_BURST:
        flight_data->flight_state = ERT_FLIGHT_STATE_DESCENDING;
        break;
      case ERT_FLIGHT_STATE_DESCENDING:
        flight_data->flight_state = ERT_FLIGHT_STATE_LANDED;
        break;
      default:
        // Do not change state
        break;
    }
  }
}
