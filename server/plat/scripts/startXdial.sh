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

getCmdlineOption()
{
    if [ -z "$2" ]; then
        echo=""; #do not speicify XDIAL_IFNAME when not available
    else
        echo "$1 $2"
    fi
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

if [ $RDK_PROFILE = "TV" ]; then
  Manufacturer=$MFG_NAME
else
  Manufacturer=$PartnerId
fi
echo "Manufacturer: $Manufacturer"

#Construct Friendly Name for Netflix
FriendlyName=`echo "${Manufacturer}_${ModelName}"`
echo "Friendly Name: $FriendlyName"

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

if [[ $DEVICE_TYPE != *"hybrid"* ]] && [[ $DEVICE_NAME != *"XI3"* ]] && [[ $DEVICE_NAME != *"XID"* ]]; then
    XDIAL_IFNAME="${XDIAL_IFNAME}:0"
fi

echo ${XDIAL_IFNAME} > /tmp/dial_interface

if [ "$BUILD_TYPE" != "prod" ] && [ -f /opt/enableXdialNetflixStop ]; then
    export ENABLE_NETFLIX_STOP="true"
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
    export LD_LIBRARY_PATH=/usr/share/xdial/:/lib/:/usr/lib/:/usr/local/lib/

    #wait for udhcpc to complete, do not use default ip
    retry_count=0
    inet_addr_str=`ifconfig ${XDIAL_IFNAME} |grep "inet addr"`
    if [ "${XDIAL_IFNAME:0:4}" == "eth0" ]; then
        curr_ip_addr=`echo $inet_addr_str |egrep "192.168.18.10|192.0.2.10"`
    else
        curr_ip_addr=`echo $inet_addr_str |egrep "192.168.28.10|192.0.2.11"`
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
          curr_ip_addr=`echo $inet_addr_str |egrep "192.168.18.10|192.0.2.10"`
      else
          curr_ip_addr=`echo $inet_addr_str |egrep "192.168.28.10|192.0.2.11"`
      fi
      retry_count=`expr $retry_count + 1`
    done

    xcast_ip_addr_file_prefix=`echo "/tmp/XDIAL_ipv4_$XDIAL_IFNAME" | sed 's/:/_/g'`
    ipAddress=$(echo "$inet_addr_str" | awk -F'inet addr:' '{print $2}' | cut -d ' ' -f 1)
    new_xcast_ip_addr_file=$xcast_ip_addr_file_prefix"_"$ipAddress
    echo "new file name: $new_xcast_ip_addr_file"
    rm -f /tmp/XDIAL_$2*
    touch $new_xcast_ip_addr_file

    XDIAL_IFNAME_OPTION=$(getCmdlineOption "-I" "$XDIAL_IFNAME")
    FriendlyName_OPTION=$(getCmdlineOption "-F" "$FriendlyName")
    Manufacturer_OPTION=$(getCmdlineOption "-R" "$Manufacturer")
    ModelName_OPTION=$(getCmdlineOption "-M" "$ModelName")
    UUID_OPTION=$(getCmdlineOption "-U" "$UUID")
    AppList_OPTION=$(getCmdlineOption "-A" "$AppList")

    echo "gdial-server args: ${XDIAL_IFNAME_OPTION} ${FriendlyName_OPTION} ${Manufacturer_OPTION} ${ModelName_OPTION} ${UUID_OPTION} ${AppList_OPTION}"
    exec /usr/share/xdial/gdial-server ${XDIAL_IFNAME_OPTION} ${FriendlyName_OPTION} ${Manufacturer_OPTION} ${ModelName_OPTION} ${UUID_OPTION} ${AppList_OPTION} 
