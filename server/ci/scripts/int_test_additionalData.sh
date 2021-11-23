#!/bin/bash
source ../ci/scripts/int_test_env.sh
source ../ci/scripts/int_test_util.sh

echo "Running xDIAL tests from ${HOSTIP} at dir $(pwd) ..."

bash ../ci/scripts/startXdial.sh &
echo "Waiting 1 sec for gdial-server to start..."
sleep ${XDIALSERVER_START_DELAY}

error_exit() {
  pkill -9 gdial-server
  rm /tmp/Netflix
  exit 5001
}

success_exit() {
  pkill -9 gdial-server
  rm /tmp/Netflix
  exit 0
}

get_additionalData() {
  echo -n $(curl -s  http://${HOSTIP}:56889/apps/Netflix | python -c "import xml.etree.ElementTree as ET; import sys; tree=ET.parse(sys.stdin); print(tree.getroot().findall('{{urn:dial-multiscreen-org:schemas:dial}}additionalData/{{urn:dial-multiscreen-org:schemas:dial}}{}'.format('$1'))[0].text)" |tr -d '\n')
}
### ### ### ### ### ### #####
### Test cases start here ###
### ### ### ### ### ### #####

# TEST: dial_data and additionalData
rm /tmp/Netflix
state=$(get_appState "Netflix")
[ "${state}" != "stopped" ] && echo "failed: expecting stopped but state=$state!" && error_exit;

curl -s -X POST  http://${HOSTIP}:56889/apps/Netflix
state=$(get_appState "Netflix")
[ "${state}" != "running" ] && echo "failed: expecting running, but state=$state!" && error_exit;

timestamp=$(date +%s)
curl -X POST http://127.0.0.1:56889/apps/Netflix/dial_data -d "data=${timestamp}"
additionoalData=$(get_additionalData "data")
[ "${additionoalData}" != "${timestamp}" ] && echo "failed: expecting ${timestamp}, but additionoalData=${additionoalData}!" && error_exit;

# addittionalData persists after restart
pkill -9 gdial-server
bash ../ci/scripts/startXdial.sh &
sleep 1
additionoalData=$(get_additionalData "data")
[ "${additionoalData}" != "${timestamp}" ] && echo "failed: expecting ${timestamp}, but additionoalData=${additionoalData}!" && error_exit;

success_exit;
