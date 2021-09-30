#!/bin/bash
source ../ci/scripts/int_test_env.sh
source ../ci/scripts/int_test_util.sh

echo "Running xDIAL tests from ${HOSTIP} at dir $(pwd) ..."

bash ../ci/scripts/startXdial.sh &
echo "Waiting 1 sec for gdial-server to start..."
sleep ${XDIALSERVER_START_DELAY}

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
