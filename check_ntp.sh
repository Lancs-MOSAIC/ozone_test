#!/bin/sh
#
# Check if NTP has synchronised or not

NTPQ=$(/usr/bin/ntpq -c rv)
#echo ntpq said: $NTPQ

if [ -z "$NTPQ" ]; then
  echo Error checking NTP status
  exit 1
fi

echo $NTPQ | grep sync_unspec > /dev/null
RET=$?
if [ $RET = "0" ]; then
  echo NTP has not synchronised
  exit 1
else
  echo NTP has synchronised
  exit 0
fi

