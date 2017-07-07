#!/bin/bash

# Allow commands to fail
set +e

SCRIPT_FILE=`readlink -f "${0}"`
SCRIPT_PATH=`dirname "${SCRIPT_FILE}"`

while true;
do
    "${SCRIPT_PATH}/ertnode-check.sh"
    sleep 10
done
