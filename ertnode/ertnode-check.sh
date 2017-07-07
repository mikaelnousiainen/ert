#!/bin/bash

# Allow commands to fail
set +e

MAXIMUM_TELEMETRY_TIMESTAMP_AGE_SECONDS=120
MAXIMUM_TELEMETRY_TRANSFER_FAILURE_COUNT=3

APP_SERVER="localhost:9000"
APP_STATUS_URL="http://${APP_SERVER}/api/status"
APP_EXECUTABLE_NAME="ertnode"

function get_app_pid()
{
    APP_PID=`ps a | grep -E "/${APP_EXECUTABLE_NAME}$" | sed -r "s/[^0-9]*([0-9]+).*/\1/"`
    if [ -z "${APP_PID}" ]; then
        echo "Error finding app PID for executable: ${APP_EXECUTABLE_NAME}"
        return 1
    fi

    echo ${APP_PID}

    return 0
}

function restart_app_gracefully()
{
    APP_PID=`get_app_pid`
    RESULT=$?
    if [ "${RESULT}" -ne "0" ]; then
        return 1
    fi

    echo "`date "+%Y-%m-%d %H:%M:%S"` Sending SIGTERM to PID ${APP_PID} ..."
    kill -TERM ${APP_PID}

    WAIT_COUNT=0
    while [ "${WAIT_COUNT}" -lt 25 ]
    do
        kill -0 ${APP_PID}
        RESULT=$?
        if [ "${RESULT}" -ne "0" ]; then
            echo "`date "+%Y-%m-%d %H:%M:%S"` PID ${APP_PID} terminated gracefully"
            return 0
        fi

        WAIT_COUNT=$((${WAIT_COUNT} + 1))
        echo "`date "+%Y-%m-%d %H:%M:%S"` Waiting for PID ${APP_PID} to terminate (${WAIT_COUNT}) ..."
        sleep 1
    done

    echo "`date "+%Y-%m-%d %H:%M:%S"` Process not terminating, sending SIGKILL to PID ${APP_PID} ..."
    kill -KILL ${APP_PID}
    echo "`date "+%Y-%m-%d %H:%M:%S"` PID ${APP_PID} killed"

    return 0
}

function restart_app_force()
{
    APP_PID=`get_app_pid`
    RESULT=$?
    if [ "${RESULT}" -ne "0" ]; then
        echo "`date "+%Y-%m-%d %H:%M:%S"` Error: ${APP_PID}"
        return 1
    fi

    echo "`date "+%Y-%m-%d %H:%M:%S"` Sending SIGKILL to PID ${APP_PID} ..."
    kill -KILL ${APP_PID}

    return 0
}

echo "`date "+%Y-%m-%d %H:%M:%S"` Checking ERT status: ${APP_STATUS_URL} ..."

APP_STATUS_OUTPUT=`curl -s "${APP_STATUS_URL}"`
RESULT=$?
if [ "${RESULT}" -ne "0" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Error checking app status in URL ${APP_STATUS_URL} -- curl exit code ${RESULT}"
    restart_app_force
    exit 1
fi

LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_MILLIS=`echo "${APP_STATUS_OUTPUT}" | jq ".telemetry_transmitted.last_transferred_telemetry_entry_timestamp_millis"`
LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_SECONDS=$((${LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_MILLIS} / 1000))
CURRENT_TIMESTAMP_SECONDS=`date +%s`

if [ -z "${LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_MILLIS}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Error getting last transferred telemetry timestamp ..."
    restart_app_force
    exit 1
fi
if [ -z "${LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_SECONDS}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Error getting last transferred telemetry timestamp ..."
    restart_app_force
    exit 1
fi
if [ -z "${CURRENT_TIMESTAMP_SECONDS}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Error getting current time"
    restart_app_force
    exit 1
fi

echo "`date "+%Y-%m-%d %H:%M:%S"` Last transferred telemetry timestamp: ${LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_SECONDS} seconds"
echo "`date "+%Y-%m-%d %H:%M:%S"` Current timestamp: ${CURRENT_TIMESTAMP_SECONDS} seconds"

if [ "${LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_SECONDS}" -eq "0" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` No telemetry transferred yet"
    exit 0
fi

LAST_TRANSFERRED_TELEMETRY_AGE_SECONDS=$((${CURRENT_TIMESTAMP_SECONDS} - ${LAST_TRANSFERRED_TELEMETRY_TIMESTAMP_SECONDS}))

if [ -z "${LAST_TRANSFERRED_TELEMETRY_AGE_SECONDS}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Error calculating last transferred telemetry age"
    restart_app_force
    exit 1
fi

echo "`date "+%Y-%m-%d %H:%M:%S"` Last transferred telemetry age: ${LAST_TRANSFERRED_TELEMETRY_AGE_SECONDS} seconds"

if [ "${LAST_TRANSFERRED_TELEMETRY_AGE_SECONDS}" -gt "${MAXIMUM_TELEMETRY_TIMESTAMP_AGE_SECONDS}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Last transferred telemetry older than ${MAXIMUM_TELEMETRY_TIMESTAMP_AGE_SECONDS} seconds, restarting app ..."
    restart_app_gracefully
    exit 0
fi

echo "`date "+%Y-%m-%d %H:%M:%S"` Last transferred telemetry newer than ${MAXIMUM_TELEMETRY_TIMESTAMP_AGE_SECONDS} seconds -- OK"

TELEMETRY_TRANSFER_FAILURE_COUNT=`echo "${APP_STATUS_OUTPUT}" | jq ".telemetry_transmitted.transfer_failure_count"`

if [ -z "${TELEMETRY_TRANSFER_FAILURE_COUNT}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Error getting transfer failure count ..."
    restart_app_force
    exit 1
fi

echo "`date "+%Y-%m-%d %H:%M:%S"` Telemetry transfer failure count: ${TELEMETRY_TRANSFER_FAILURE_COUNT} seconds"

if [ "${TELEMETRY_TRANSFER_FAILURE_COUNT}" -gt "${MAXIMUM_TELEMETRY_TRANSFER_FAILURE_COUNT}" ]; then
    echo "`date "+%Y-%m-%d %H:%M:%S"` Telemetry transfer failure count higher than ${MAXIMUM_TELEMETRY_TRANSFER_FAILURE_COUNT}, restarting app ..."
    restart_app_gracefully
    exit 0
fi

echo "`date "+%Y-%m-%d %H:%M:%S"` Telemetry transfer failure count less than ${MAXIMUM_TELEMETRY_TRANSFER_FAILURE_COUNT} -- OK"

exit 0
