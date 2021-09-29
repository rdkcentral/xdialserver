#!/bin/bash
HOSTIP=$(ip addr show ${XDIAL_HostIfname} |grep "inet " |cut -d/ -f1 |awk '{ print $2 } ' | tr -d '\n')
echo "Running xDIAL tests from ${HOSTIP} at dir $(pwd) ..."

sh ../ci/scripts/startXdial.sh &
echo "Waiting 2 sec for gdial-server to start..."
sleep 1

source ../ci/scripts/int_test_util.sh

GDIAL_THROTTLE_DELAY_US=$(grep GDIAL_THROTTLE_DELAY_US ../include/gdial-config.h |awk '{ print $3 }' |tr -d '\n')
GDIAL_THROTTLE_DELAY_MS=$(expr $GDIAL_THROTTLE_DELAY_US / 1000)

### ### ### ### ### ### #####
### Test cases start here ###
### ### ### ### ### ### #####

# TEST: Check response time is larger than GDIAL_THROTTLE_DELAY_MS
startTS=$(get_epochmillis)
x=1
while [ $x -le 10 ]
do
  get_appState "Netflix"
  x=$(( $x + 1 ))
done
endTS=$(get_epochmillis)

diffTS=$(expr $endTS - $startTS)
avgTS=$(expr $diffTS / 10)
echo "diffTS=$diffTS avgTS=$avgTS"
[ "${avgTS}" -le  "${GDIAL_THROTTLE_DELAY_MS}" ] && echo "failed: expecting ${GDIAL_THROTTLE_DELAY_MS}, but actual=${avgTS}!" && error_exit;

success_exit
