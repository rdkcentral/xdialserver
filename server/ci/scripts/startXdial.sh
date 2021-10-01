#!/bin/bash

# ensure default values
source ../ci/scripts/int_test_env.sh
source ../ci/scripts/int_test_util.sh

echo "env::XDIAL_HostIfname=${XDIAL_HostIfname}"
echo "env::XDIAL_FriendlyName=${XDIAL_FriendlyName}"
echo "env::XDIAL_Manufacturer=${XDIAL_Manufacturer}"
echo "env::XDIAL_ModelName=${XDIAL_ModelName}"
echo "env::XDIAL_AppList=${XDIAL_AppList}"
echo "env::XDIAL_UUID=${XDIAL_UUID}"

# working directory from Action setup is server/build
./gdial-server --enable-server \
        -I ${XDIAL_HostIfname} \
        -F ${XDIAL_FriendlyName} \
        -R ${XDIAL_Manufacturer} \
        -M ${XDIAL_ModelName} \
        -U ${XDIAL_UUID} \
        -A ${XDIAL_AppList}
