#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"

cd ${GITHUB_WORKSPACE}

# # ############################# 
#1. Install Dependencies and packages

apt update
apt install -y ninja-build meson curl libsoup2.4-dev libxml2-dev libglib2.0-dev gobject-introspection libgirepository1.0-dev libgtk-3-dev valac pandoc
pip install jsonref

############################
# Build trevor-base64
if [ ! -d "trower-base64" ]; then
git clone https://github.com/xmidt-org/trower-base64.git
fi
cd trower-base64
meson setup --warnlevel 3 --werror build
ninja -C build
ninja -C build install
cd ..
###########################################
# Clone the required repositories

rm -rf iarmbus
git clone https://github.com/rdkcentral/iarmbus.git
export IARMBUS_PATH=$GITHUB_WORKSPACE/iarmbus

rm -rf ThunderTools
git clone --branch  R4.4.3 https://github.com/rdkcentral/ThunderTools.git

rm -rf Thunder
git clone --branch R4.4.1 https://github.com/rdkcentral/Thunder.git

rm -rf entservices-apis
git clone --branch main https://github.com/rdkcentral/entservices-apis.git

#git clone https://$GITHUB_TOKEN@github.com/rdkcentral/entservices-testframework.git

rm -rf gssdp
git clone --branch gssdp-1.2.3 https://gitlab.gnome.org/GNOME/gssdp.git

############################
# Build gssdp-1.2
echo "======================================================================================"
echo "buliding gssdp-1.2"
cd gssdp

rm -rf build
meson setup build

ninja -C build
ninja -C build install
cd -

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "buliding thunderTools"
cd ThunderTools
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/00010-R4.4-Add-support-for-project-dir.patch
cd -


cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/ThunderTools --target install


############################
# Build Thunder
echo "======================================================================================"
echo "buliding thunder"

cd Thunder
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/error_code_R4_4.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/1004-Add-support-for-project-dir.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-733-Add-ENTOS-IDS.patch
cd -

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON \

cmake --build build/Thunder --target install

############################
# Build entservices-apis
echo "======================================================================================"
echo "buliding entservices-apis"
cd entservices-apis
rm -rf jsonrpc/DTV.json
cd ..

cmake -G Ninja -S entservices-apis  -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/entservices-apis --target install

############################

############################
# Build and deploy stubs for IARMBus and WPEFramework securityagent
mkdir -p /usr/include/WPEFramework/securityagent
cp $GITHUB_WORKSPACE/stubs/securityagent/* /usr/include/WPEFramework/securityagent/ -v

echo "======================================================================================"
echo "Building IARMBus and WPEFramework securityagent stubs"
cd $GITHUB_WORKSPACE
cd ./stubs
g++ -fPIC -shared -o libIARMBus.so iarm_stubs.cpp -I$GITHUB_WORKSPACE/stubs -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I$IARMBUS_PATH/core -I$IARMBUS_PATH/core/include -fpermissive
g++ -fPIC -shared -o libWPEFrameworkSecurityUtil.so securityagent/SecurityTokenUtil.cpp  -I$GITHUB_WORKSPACE/stubs -fpermissive

cp libIARMBus.so /usr/local/lib/
cp libWPEFrameworkSecurityUtil.so /usr/local/lib/
