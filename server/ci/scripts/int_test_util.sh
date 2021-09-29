#!/bin/bash

error_exit() {
  pkill -9 gdial-server
  exit 5001
}

success_exit() {
  pkill -9 gdial-server
  exit 0
}

get_epochmillis() {
  echo $(date +%s%N | cut -b1-13 | tr -d '\n')
}

get_ddxml_manufacturer() {
  echo -n $(curl -s ${HOSTIP}:56890/dd.xml | python -c "import xml.etree.ElementTree as ET; import sys; tree=ET.parse(sys.stdin); print(tree.getroot().findall('{urn:schemas-upnp-org:device-1-0}device/{urn:schemas-upnp-org:device-1-0}manufacturer')[0].text)" |tr -d '\n')
}

get_appState() {
  echo -n $(curl -s  http://${HOSTIP}:56889/apps/$1| python -c "import xml.etree.ElementTree as ET; import sys; tree=ET.parse(sys.stdin); print(tree.getroot().findall('{urn:dial-multiscreen-org:schemas:dial}state')[0].text)" |tr -d '\n')
}

