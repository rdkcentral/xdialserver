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
      interface=`getMoCAInterface`
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

#Construct Friendly Name for Netflix
FriendlyName=`echo "${PartnerId}_${ModelName}"`
echo "Friendly Name: $FriendlyName"

Manufacturer=$MFG_NAME
echo "Manufacturer: $Manufacturer"
#Get UUID
UUID=$(getReceiverId)
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

echo -en '\n'
if tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XDial.Enable 2>&1 1>/dev/null  | grep -q 'true'; then
    echo "rfc enabled :starting gdial-server"
    export LD_LIBRARY_PATH=/usr/share/xdial/:/lib/:/usr/lib/:/usr/local/lib/

    #wait for udhcpc to complete, do not use default ip
    retry_count=0
    if [ "${XDIAL_IFNAME}" == "eth0" ]; then
        curr_ip_addr=`ifconfig ${XDIAL_IFNAME}:0 |grep "inet addr" |grep "192.168.18.10"`
    else
        curr_ip_addr=`ifconfig ${XDIAL_IFNAME}:0 |grep "inet addr" |grep "192.168.28.10"`
    fi

    while [[ ! -z "$curr_ip_addr" ]]
    do
      if [ "$retry_count" -ge 5 ]; then
          break
      fi
      sleep 2
      echo "++++++++++++++++++"
      echo "waiting for ip on ${XDIAL_IFNAME}:0, currently $curr_ip_addr"
      echo "++++++++++++++++++"
      if [ "${XDIAL_IFNAME}" == "eth0" ]; then
          curr_ip_addr=`ifconfig ${XDIAL_IFNAME}:0 |grep "inet addr" |grep "192.168.18.10"`
      else
          curr_ip_addr=`ifconfig ${XDIAL_IFNAME}:0 |grep "inet addr" |grep "192.168.28.10"`
      fi
      retry_count=`expr $retry_count + 1`
    done

    /usr/share/xdial/gdial-server -I ${XDIAL_IFNAME}:0 -F ${FriendlyName} -R ${Manufacturer} -M ${ModelName} -U ${UUID} -A "${AppList} "
else
    echo "rfc disabled: gdial-server not started"
fi
