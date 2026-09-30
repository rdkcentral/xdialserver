#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"

cd ${GITHUB_WORKSPACE}

# # ############################# 
#1. Install Dependencies and packages

apt update
apt install -y git python3 python3-pip cmake ninja-build meson curl libsoup2.4-dev libxml2-dev libglib2.0-dev gobject-introspection libgirepository1.0-dev libgtk-3-dev libcurl4-openssl-dev libcunit1-dev valac pandoc
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

rm -rf iarmbus ThunderTools Thunder entservices-apis entservices-testframework gssdp

THUNDER_TOOLS_COMMIT_SHA="d5dd83c7c19c49c7f25c558c126500bd2d64f7a4"
THUNDER_COMMIT_SHA="2c0fcc5529e7da734be558ca6efa05d934dcce31"

git clone https://github.com/rdkcentral/iarmbus.git
export IARMBUS_PATH=$GITHUB_WORKSPACE/iarmbus

git clone --branch R4_4-RDK https://github.com/rdkcentral/ThunderTools.git
cd ThunderTools
git checkout $THUNDER_TOOLS_COMMIT_SHA
cd ..

git clone --branch R4_4-RDK https://github.com/rdkcentral/Thunder.git
cd Thunder
git checkout $THUNDER_COMMIT_SHA
cd ..

git clone --branch develop https://github.com/rdkcentral/entservices-apis.git

git clone --branch 2.0.0 https://github.com/rdkcentral/entservices-testframework.git

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
