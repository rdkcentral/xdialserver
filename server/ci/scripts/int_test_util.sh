#!/bin/bash

error_exit() {
  pkill -9 gdial-server
  exit 5001
}

success_exit() {
  pkill -9 gdial-server
  exit 0
}

get_ifname() {
  echo -n $(route |grep default |awk '{ print $NF }')
}

get_hostip() {
  echo -n $(ip addr show ${XDIAL_HostIfname} |grep "inet " |cut -d/ -f1 |awk '{ print $2 } ' | tr -d '\n')
}

get_epochmillis() {
  echo -n $(date +%s%N | cut -b1-13 | tr -d '\n')
}

get_ddxml_manufacturer() {
  echo -n $(curl -s ${HOSTIP}:56890/dd.xml | python -c "import xml.etree.ElementTree as ET; import sys; tree=ET.parse(sys.stdin); print(tree.getroot().findall('{urn:schemas-upnp-org:device-1-0}device/{urn:schemas-upnp-org:device-1-0}manufacturer')[0].text)" |tr -d '\n')
}

get_ssdp_usn() {
  echo -n  $(printf 'M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: "ssdp:discover"\r\nMX: 1\r\nST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n' | socat -t3 - udp-datagram:239.255.255.250:1900,ip-add-membership=239.255.255.250:wlp2s0b1 |grep -i USN |grep "12345678-abcd-abcd-1234-123456789abc" | awk '{split($0,a,"::"); print a[2]}' | tr -d '\r\n')
}

get_appState() {
  echo -n $(curl -s  http://${HOSTIP}:56889/apps/$1| python -c "import xml.etree.ElementTree as ET; import sys; tree=ET.parse(sys.stdin); print(tree.getroot().findall('{urn:dial-multiscreen-org:schemas:dial}state')[0].text)" |tr -d '\n')
}

