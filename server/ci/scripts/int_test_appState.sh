#!/bin/bash
HOSTIP=$(ip addr show ${XDIAL_HostIfname} |grep "inet " |cut -d/ -f1 |awk '{ print $2 } ' | tr -d '\n')
echo "Running xDIAL tests from ${HOSTIP} at dir $(pwd) ..."

sh ../ci/scripts/startXdial.sh &
echo "Waiting 2 sec for gdial-server to start..."
sleep 1

source ../ci/scripts/int_test_util.sh

### ### ### ### ### ### #####
### Test cases start here ###
### ### ### ### ### ### #####

# TEST: STOPPED--> RUNNING--> HIDDDEN--> RUNNING -->STOPPED
state=$(get_appState "Netflix")
[ "$state" != "stopped" ] && echo "failed: expecting stopped but state=$state!" && error_exit;

curl -s -X POST  http://${HOSTIP}:56889/apps/Netflix
state=$(get_appState "Netflix")
[ "$state" != "running" ] && echo "failed: expecting running, but state=$state!" && error_exit;

curl -s -X POST  http://${HOSTIP}:56889/apps/Netflix/run/hide
state=$(get_appState "Netflix")
[ "$state" != "hidden" ] && echo "failed: expecting running, but state=$state!" && error_exit;

curl -s -X POST  http://${HOSTIP}:56889/apps/Netflix
state=$(get_appState "Netflix")
[ "$state" != "running" ] && echo "failed: expecting running, but state=$state!" && error_exit;

curl -s -X DELETE http://${HOSTIP}:56889/apps/Netflix/run
state=$(get_appState "Netflix")
[ "$state" != "stopped" ] && echo "failed: expecting stopped but state=$state!" && error_exit;

success_exit;
