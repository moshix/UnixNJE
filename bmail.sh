#!/bin/sh
#
# Test program..

LOGFILE=/usr/tmp/bmail.log

touch $LOGFILE
echo "---------- bmail ------" >>$LOGFILE
set >>$LOGFILE
echo $* >>$LOGFILE
echo >>$LOGFILE
/usr/local/bin/bmail $* 2>&1 >>$LOGFILE
