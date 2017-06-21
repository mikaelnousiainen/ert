#!/bin/bash

SCRIPT_FILE=`readlink -f "${0}"`
SCRIPT_PATH=`dirname "${SCRIPT_FILE}"`

cd "${SCRIPT_PATH}"

export ERT_LOG_PATH="${SCRIPT_PATH}/log/"

mkdir -p "${ERT_LOG_PATH}"

sudo bash -c "ERT_LOG_PATH=\"${ERT_LOG_PATH}\" \"${SCRIPT_PATH}/ertgateway\" 1>> \"${ERT_LOG_PATH}ertgateway-stdout.log\" 2>> \"${ERT_LOG_PATH}ertgateway-stderr.log\""
