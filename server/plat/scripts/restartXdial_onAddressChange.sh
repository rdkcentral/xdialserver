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


LOG_INPUT=restartXdial.txt
if [ ! "$LOG_PATH" ];then LOG_PATH=/opt/logs/ ; fi
LOG_FILE=$LOG_PATH/$LOG_INPUT


# Input Arguments - $1 event - $2 ipaddress type - $3 interface name - $4 ipaddress - $5 ipaddress scope
printf "$(date) $0: Input Parameters : event $1, ipaddress type $2, interface name $3, ipaddress $4, ipaddress scope $5  \n" >> $LOG_FILE


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

restartXdialService() {
    IsXdialRunning=`systemctl is-active xdial`
    if [ "$IsXdialRunning" == "active" ];then
      printf "Interface $3 IP details obtained : \n $curr_ip_addr \n" >> $LOG_FILE
      printf "xdial service is running \n" >> $LOG_FILE
      printf "$(date)Restarting xdial service to take new interface IP \n" >> $LOG_FILE
      systemctl restart xdial
    else
      printf "Interface $3 IP details obtained : \n $curr_ip_addr \n" >> $LOG_FILE
      printf "xdial service is not  running \n" >> $LOG_FILE
      printf "$(date) start xdial service at new interface IP \n" >> $LOG_FILE
      systemctl start xdial
    fi

}

XDIAL_IFNAME=$(getESTBInterfaceName)


if [[ $DEVICE_TYPE != *"hybrid"* ]] && [[ $DEVICE_NAME != *"XI3"* ]] && [[ $DEVICE_NAME != *"XID"* ]]; then
    XDIAL_IFNAME="${XDIAL_IFNAME}:0"
fi

echo "$XDIAL_IFNAME" >> $LOG_FILE

if [ "$1" == "add" ] && [ "$2" == "ipv4" ] && [ "$3" == "$XDIAL_IFNAME" ];then
  # make new ip name
  xcast_ip_addr_file_prefix=`echo "/tmp/XDIAL_$2_$3_" | sed 's/:/_/g'`
  new_xcast_ip_addr_file=$xcast_ip_addr_file_prefix$4

  # Test if there is a previous ip
  if ls $new_xcast_ip_addr_file 1> /dev/null 2>&1; then
    printf "$(date) Got event for same ip - $2 $3 $4, ignore\n" >> $LOG_FILE
  else
    printf "new_xcast_ip_addr_file is $new_xcast_ip_addr_file \n"
    printf "$(date)Got event for new  ip - $2 $3 $4, react\n" >> $LOG_FILE
    file_count=0
    if [ -f `find /tmp/ -type f -name "XDIAL_*" 2>/dev/null |head -1` ]; then
      file_count=$(ls -l /tmp/XDIAL_* 2>/dev/null | wc -l)
    fi
    echo "file count for ls /tmp/XDIAL_* : $file_count \n " >> $LOG_FILE
    if [ $file_count -gt 0 ]; then
      if [ $4 != "192.168.28.10" ] && [ $4 != "192.168.18.10" ] && [ $4 != "192.0.2.11" ] && [ $4 != "192.0.2.10" ]; then
        printf "IP change to new valid ip: $4 proceed with restart  " >> $LOG_FILE
        restartXdialService &
      else
        printf "IP change to default so ignore the event until next event $4 \n" >> $LOG_FILE
      fi
    else
      printf " gdial server has not started yet on any interface \n" >> $LOG_FILE
    fi
  fi
else
  printf "Ignore irrelvant event \n" >> $LOG_FILE
fi
