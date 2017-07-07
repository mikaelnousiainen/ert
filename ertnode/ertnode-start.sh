#!/bin/bash

# Stop script if a command fails
set -e

SCRIPT_FILE=`readlink -f "${0}"`
SCRIPT_PATH=`dirname "${SCRIPT_FILE}"`

cd "${SCRIPT_PATH}"

export ERT_LOG_PATH="${SCRIPT_PATH}/log/"

mkdir -p "${ERT_LOG_PATH}"

# Allow commands to fail
set +e

"${SCRIPT_PATH}/ertnode-prepare-raspbian.sh"

systemctl gpsd stop

# Initialize GPS
"${SCRIPT_PATH}/ertnode" --gps-config

systemctl gpsd start

# Run one cycle of hardware initialization to have it in a sane state
"${SCRIPT_PATH}/ertnode" --init-only

nohup bash -c "while true; do ERT_LOG_PATH=\"${ERT_LOG_PATH}\" \"${SCRIPT_PATH}/ertnode\" 1>> \"${ERT_LOG_PATH}ertnode-stdout.log\" 2>> \"${ERT_LOG_PATH}ertnode-stderr.log\"; sleep 5; done" &

nohup bash -c "\"${SCRIPT_PATH}/ertnode-watch.sh\"" &
