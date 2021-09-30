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

echo "Test starts..."

echo "TEST: discovery"
USN=$(get_ssdp_usn)
USN_EXPECTED="urn:dial-multiscreen-org:service:dial:1"

[ -z "${USN}" ] && echo "fail to discover xdialserver!" && error_exit;
[ "${USN_EXPECTED}" != "${USN}" ] && echo "fail to find correct service!" && error_exit;

echo "TEST: Check manufacturer"
ddxml_manufacturer=$(get_ddxml_manufacturer)
[ "$ddxml_manufacturer" != "CI-Manufacturer" ] && echo "ddxml_manufacturer=$ddxml_manufacturer failed!" && error_exit;

success_exit
