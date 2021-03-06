#!/bin/bash

SCRIPT_FILE=`readlink -f "${0}"`
SCRIPT_PATH=`dirname "${SCRIPT_FILE}"`

cd "${SCRIPT_PATH}"

export ERT_LOG_PATH="${SCRIPT_PATH}/log/"

mkdir -p "${ERT_LOG_PATH}"

# Run one cycle of hardware initialization to have it in a sane state
"${SCRIPT_PATH}/ertgateway" --init-only

nohup bash -c "while true; do ERT_LOG_PATH=\"${ERT_LOG_PATH}\" \"${SCRIPT_PATH}/ertgateway\" 1>> \"${ERT_LOG_PATH}ertgateway-stdout.log\" 2>> \"${ERT_LOG_PATH}ertgateway-stderr.log\"; sleep 5; done" &
