#!/bin/bash

SCRIPT_FILE=`readlink -f "${0}"`
SCRIPT_PATH=`dirname "${SCRIPT_FILE}"`

cd "${SCRIPT_PATH}"

export ERT_LOG_PATH="${SCRIPT_PATH}/log/"

mkdir -p "${ERT_LOG_PATH}"

"${SCRIPT_PATH}/ertnode-prepare-raspbian.sh"

systemctl gpsd stop

# Initialize GPS
"${SCRIPT_PATH}/ertnode" -g

systemctl gpsd start

# Run one cycle of hardware initialization to have it in a sane state
"${SCRIPT_PATH}/ertnode" -i

nohup bash -c "while true; do ERT_LOG_PATH=\"${ERT_LOG_PATH}\" \"${SCRIPT_PATH}/ertnode\" 1>> \"${ERT_LOG_PATH}ertnode-stdout.log\" 2>> \"${ERT_LOG_PATH}ertnode-stderr.log\"; sleep 5; done" &
