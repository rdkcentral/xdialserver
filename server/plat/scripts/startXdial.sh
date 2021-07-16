#!/bin/sh

##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

. /etc/device.properties
. /etc/include.properties
. $RDK_PATH/getPartnerId.sh

if [ -f $RDK_PATH/utils.sh ]; then
   . $RDK_PATH/utils.sh
fi

getESTBInterfaceName()
{
   if [ -f /tmp/wifi-on ]; then
      interface=`getWiFiInterface`
   else
      interface=$MOCA_INTERFACE
      if [ ! "$interface" ]; then
                interface=eth1
      fi
   fi
   echo ${interface}
}

#Get Partner ID
PartnerId=$(getPartnerId)
PartnerId=`echo ${PartnerId:0:1} | tr  '[a-z]' '[A-Z]'`${PartnerId:1}
echo "Partner ID: $PartnerId"

#Get Model Name
ModelName=$MODEL_NUM
echo "Model Name: $ModelName"

getValFromJsonStr () {
    input=$1
    deliminator=$2
    if echo $input | grep -q $deliminator; then
        result=$(echo "$input" | awk -F "${deliminator}\":" '{print $2}')
        echo "$result" | awk -F ",\"" '{print $1}' | sed 's/}//g' |sed 's/"//g'
    else
        echo "ERR";
    fi
}

respStr="$(curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" -d '{"jsonrpc":"2.0","id":3,"method":"org.rdk.AuthService.1.getExperience"}' http://127.0.0.1:9998/jsonrpc 2>/dev/null)"
echo "curl getExperience response: $respStr"
statusValue=$(getValFromJsonStr $respStr "success")
exp_retry=0

while [ $statusValue != "true" ]
do
  sleep 3
  respStr="$(curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" -d '{"jsonrpc":"2.0","id":3,"method":"org.rdk.AuthService.1.getExperience"}' http://127.0.0.1:9998/jsonrpc 2>/dev/null)"
  echo "curl getExperience response: $respStr"
  statusValue=$(getValFromJsonStr $respStr "success")
  if [ "$exp_retry" -ge 15 ]; then
      break
  fi
  exp_retry=`expr $exp_retry + 1`
done

expValue=$(getValFromJsonStr $respStr "experience")
echo "expValue : $expValue"
if [ $expValue = "Flex" ]; then
    ModelName="$ModelName$expValue"
    echo "Flex experience ModelName:$ModelName"
fi

#Construct Friendly Name for Netflix
FriendlyName=`echo "${PartnerId}_${ModelName}"`
echo "Friendly Name: $FriendlyName"

Manufacturer=$MFG_NAME
echo "Manufacturer: $Manufacturer"
#Get UUID
UUID=$(getReceiverId)
if [ -z "$UUID" ]; then
    #Assigning default UUID
    UUID="12345678-abcd-abcd-1234-123456789abc"
fi
echo "UUID: $UUID"

#Opening Required PORTS
XDIAL_IFNAME=$(getESTBInterfaceName)
AppList=$(tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XDial.AppList 2>&1)
echo $AppList

iptables -C INPUT -i ${XDIAL_IFNAME} -p tcp --dport 9081 -j ACCEPT > /dev/null 2>&1
rule_exist="$?"
if [ "$rule_exist" -ne 0 ]; then
    iptables -I INPUT -i ${XDIAL_IFNAME} -p tcp --dport 9081 -j ACCEPT #(Netflix MDX)
fi

iptables -C INPUT -i ${XDIAL_IFNAME} -p tcp --dport 56889 -j ACCEPT > /dev/null 2>&1
rule_exist="$?"
if [ "$rule_exist" -ne 0 ]; then
    iptables -I INPUT -i ${XDIAL_IFNAME} -p tcp --dport 56889 -j ACCEPT #(DIAL_PORT)
fi

iptables -C INPUT -i ${XDIAL_IFNAME} -p tcp --dport 56890 -j ACCEPT > /dev/null 2>&1
rule_exist="$?"
if [ "$rule_exist" -ne 0 ]; then
    iptables -I INPUT -i ${XDIAL_IFNAME} -p tcp --dport 56890 -j ACCEPT #(SSDP PORT)
fi

if [[ $DEVICE_TYPE != *"hybrid"* ]] && [[ $DEVICE_NAME != *"XI3"* ]] && [[ $DEVICE_NAME != *"XID"* ]]; then
    XDIAL_IFNAME="${XDIAL_IFNAME}:0"
fi

echo ${XDIAL_IFNAME} > /tmp/dial_interface

if tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XDial.FriendlyNameEnable 2>&1 1>/dev/null  | grep -q 'true'; then
    XDIAL_FRIENDLYNAME_ENABLED=" --feature-friendlyname "
fi

if [ "$BUILD_TYPE" != "prod" ] && [ -f /opt/enableXdialNetflixStop ]; then
    export ENABLE_NETFLIX_STOP="true"
fi

if tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XDial.WolWakeEnable 2>&1 1>/dev/null  | grep -q 'true'; then
    XDIAL_WOLWAKE_ENABLED=" --feature-wolwake"
fi

retry_logic () {

    if [ -z "$2" ]
    then
        echo "RETRY"
    else
        if [ -z "$1" ]
        then
            echo "NORETRY"
        else
            echo "RETRY"
        fi
    fi
}

echo -en '\n'
if tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XDial.Enable 2>&1 1>/dev/null  | grep -q 'true'; then
    echo "rfc enabled :starting gdial-server"
    export LD_LIBRARY_PATH=/usr/share/xdial/:/lib/:/usr/lib/:/usr/local/lib/

    #wait for udhcpc to complete, do not use default ip
    retry_count=0
    inet_addr_str=`ifconfig ${XDIAL_IFNAME} |grep "inet addr"`
    if [ "${XDIAL_IFNAME:0:4}" == "eth0" ]; then
        curr_ip_addr=`echo $inet_addr_str |grep "192.168.18.10"`
    else
        curr_ip_addr=`echo $inet_addr_str |grep "192.168.28.10"`
    fi
    while [ $(retry_logic "$curr_ip_addr" "$inet_addr_str" ) == "RETRY" ]
    do
      if [ "$retry_count" -ge 20 ]; then
          break
      fi
      sleep 3
      echo "++++++++++++++++++"
      output=`ifconfig ${XDIAL_IFNAME}`
      echo "waiting for ip on ${XDIAL_IFNAME}, currently $inet_addr_str"
      echo "++++++++++++++++++"
      inet_addr_str=`ifconfig ${XDIAL_IFNAME} |grep "inet addr"`
      if [ "${XDIAL_IFNAME:0:4}" == "eth0" ]; then
          curr_ip_addr=`echo $inet_addr_str |grep "192.168.18.10"`
      else
          curr_ip_addr=`echo $inet_addr_str |grep "192.168.28.10"`
      fi
      retry_count=`expr $retry_count + 1`
    done

    /usr/share/xdial/gdial-server -I "${XDIAL_IFNAME}" -F "${FriendlyName}" -R "${Manufacturer}" -M "${ModelName}" -U "${UUID}" -A "${AppList}" ${XDIAL_FRIENDLYNAME_ENABLED} ${XDIAL_WOLWAKE_ENABLED}
else
    echo "rfc disabled: gdial-server not started"
fi
