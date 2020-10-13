#!/bin/sh

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
printf "$0: Input Parameters : event $1, ipaddress type $2, interface name $3, ipaddress $4, ipaddress scope $5  \n" >> $LOG_FILE


RFC_XDIAL_ENABLED=`tr181 -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XDial.Enable 2>&1`
if [ "x$RFC_XDIAL_ENABLED" == "xfalse" ]; then
    echo "XDIAL is disabled, exit without xdial restart" >> $LOG_FILE
    exit 0
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

restartXdialService() {
    IsXdialRunning=`systemctl is-active xdial`
    if [ "$IsXdialRunning" == "active" ];then
      printf "Interface $3 IP details obtained : \n $curr_ip_addr \n" >> $LOG_FILE
      printf "xdial service is running \n" >> $LOG_FILE
      printf "Restarting xdial service to take new interface IP \n" >> $LOG_FILE
      systemctl restart xdial
    else
      printf "Interface $3 IP details obtained : \n $curr_ip_addr \n" >> $LOG_FILE
      printf "xdial service is not  running \n" >> $LOG_FILE
      printf "start xdial service at new interface IP \n" >> $LOG_FILE
      systemctl start xdial
    fi

}

if [[ $ModelName == *"X061"* ]]; then
    XDIAL_IFNAME=$(getESTBInterfaceName):0
else
    XDIAL_IFNAME=$(getESTBInterfaceName)
fi
echo "$XDIAL_IFNAME"


if [ "$1" == "add" ] && [ "$2" == "ipv4" ] && [ "$3" == "$XDIAL_IFNAME" ];then
  # make new ip name
  xcast_ip_addr_file_prefix=`echo "/tmp/XDIAL_$2_$3_" | sed 's/:/_/g'`
  new_xcast_ip_addr_file=$xcast_ip_addr_file_prefix$4

  # Test if there is a previous ip
  if ls $new_xcast_ip_addr_file 1> /dev/null 2>&1; then
    printf "Got event for same ip - $2 $3 $4, ignore\n" >> $LOG_FILE
  else
    printf "new_xcast_ip_addr_file is $new_xcast_ip_addr_file \n"
    printf "Got event for new  ip - $2 $3 $4, react\n" >> $LOG_FILE
    rm -f $xcast_ip_addr_file_prefix*
    touch $new_xcast_ip_addr_file
    restartXdialService &
  fi
else
  printf "Ignore irrelvant event \n" >> $LOG_FILE
fi
