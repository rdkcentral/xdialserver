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

#TEST: Check manufacturer
ddxml_manufacturer=$(get_ddxml_manufacturer)
[ "$ddxml_manufacturer" != "CI-Manufacturer" ] && echo "ddxml_manufacturer=$ddxml_manufacturer failed!" && error_exit;

success_exit
