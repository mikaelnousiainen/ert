#!/bin/bash

# Allow commands to fail
set +e

# Turn off HDMI output
/opt/vc/bin/tvservice -o

# Turn off LEDs
echo 0 > /sys/class/leds/led0/brightness
echo 0 > /sys/class/leds/led1/brightness

# Turn off any USB device attached after 3 minutes
# nohup bash -c "sleep 180; echo \"1-1\" > /sys/bus/usb/drivers/usb/unbind" &

# Switch Huawei 4G stick to GSM modem mode
# usb_modeswitch -v 0x12d1 -p 0x1f01 -V 0x12d1 -P 0x14dc -M "55534243123456780000000000000011060000000000000000000000000000"
