#!/bin/sh
#
# Clean up .ozo files older than some age

if [ -z $OZONE_DATA_DIR ]
then
  echo "OZONE_DATA_DIR is not set, unable to proceed"
  exit 1
fi

# Remove files older than this specification (see "date")
FILE_AGE=-90days

FIND=/usr/bin/find
XARGS=/usr/bin/xargs
RM=/bin/rm

$FIND $OZONE_DATA_DIR \! -newermt $FILE_AGE -name "*.ozo" -print0 | $XARGS -0 -I{} $RM -v {}



