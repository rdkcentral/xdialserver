#!/bin/bash

get_ifname() {
  echo -n $(route |grep default |awk '{ print $NF }')
}

get_hostip() {
  echo -n $(ip addr show $(get_ifname) |grep "inet " |cut -d/ -f1 |awk '{ print $2 } ' | tr -d '\n')
}

[ -z "${XDIAL_HostIfname}" ] && XDIAL_HostIfname=$(get_ifname)
[ -z "${XDIAL_FriendlyName}" ] && XDIAL_FriendlyName="CI-FriendlyName"
[ -z "${XDIAL_Manufacturer}" ] && XDIAL_Manufacturer="CI-Manufacturer"
[ -z "${XDIAL_ModelName}" ] && XDIAL_ModelName="CI-ModelName"
[ -z "${XDIAL_AppList}" ] && XDIAL_AppList="netflix,youtube"
[ -z "${XDIAL_UUID}" ] && XDIAL_UUID=12345678-abcd-abcd-1234-123456789abc
[ -z "${HOSTIP}" ] && HOSTIP=$(get_hostip)
[ -z "${XDIALSERVER_START_DELAY}" ] && XDIALSERVER_START_DELAY=1
