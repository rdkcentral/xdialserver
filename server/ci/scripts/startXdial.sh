# ensure default values
[ -z "${XDIAL_HostIfname}" ] && XDIAL_HostIfname="eth0" && echo "env::XDIAL_HostIfname=${XDIAL_HostIfname}"
[ -z "${XDIAL_FriendlyName}" ] && XDIAL_FriendlyName="CI-FriendlyName" && echo "env::XDIAL_FriendlyName=${XDIAL_FriendlyName}"
[ -z "${XDIAL_Manufacturer}" ] && XDIAL_Manufacturer="CI-Manufacturer" && echo "env::XDIAL_Manufacturer=${XDIAL_Manufacturer}"
[ -z "${XDIAL_ModelName}" ] && XDIAL_ModelName="CI-ModelName" && echo "env::XDIAL_ModelName=${XDIAL_ModelName}"
[ -z "${XDIAL_AppList}" ] && XDIAL_AppList="netflix,youtube" && echo "env::XDIAL_AppList=${XDIAL_AppList}"
[ -z "${XDIAL_UUID}" ] && XDIAL_UUID=$(date +%s) && echo "env::XDIAL_UUID=${XDIAL_UUID}"

# working directory from Action setup is server/build
./gdial-server --enable-server \
        -I ${XDIAL_HostIfname} \
        -F ${XDIAL_FriendlyName} \
        -R ${XDIAL_Manufacturer} \
        -M ${XDIAL_ModelName} \
        -U ${XDIAL_UUID} \
        -A ${XDIAL_AppList}
