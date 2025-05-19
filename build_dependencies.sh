#!/bin/bash
set -e
set -x
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

# # ############################# 
#1. Install Dependencies and packages

apt update
apt install -y libsqlite3-dev libcurl4-openssl-dev valgrind lcov clang libsystemd-dev libboost-all-dev libwebsocketpp-dev meson libcunit1 libcunit1-dev curl protobuf-compiler-grpc libgrpc-dev libgrpc++-dev libunwind-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
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


git clone --branch  R4.4.3 https://github.com/rdkcentral/ThunderTools.git

git clone --branch R4.4.1 https://github.com/rdkcentral/Thunder.git

git clone --branch main https://github.com/rdkcentral/entservices-apis.git

git clone https://$GITHUB_TOKEN@github.com/rdkcentral/entservices-testframework.git

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "buliding thunderTools"
cd ThunderTools
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/Tests/L1Tests/patches/00010-R4.4-Add-support-for-project-dir.patch
cd -


cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/ThunderTools --target install


############################
# Build Thunder
echo "======================================================================================"
echo "buliding thunder"

cd Thunder
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/Tests/L2Tests/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/Tests/L2Tests/patches/error_code_R4_4.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/Tests/L1Tests/patches/1004-Add-support-for-project-dir.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/Tests/L1Tests/patches/RDKEMW-733-Add-ENTOS-IDS.patch
cd -

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
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
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/Tests/L1Tests/patches/RDKEMW-1007.patch
cd ..

cmake -G Ninja -S entservices-apis  -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/entservices-apis --target install



############################
# generating extrnal headers
cd $GITHUB_WORKSPACE
cd xdial/Tests
echo " Empty mocks creation to avoid compilation errors"
echo "======================================================================================"
mkdir -p headers
#mkdir -p headers/audiocapturemgr
#mkdir -p headers/rdk/ds
#mkdir -p headers/rdk/iarmbus
#mkdir -p headers/rdk/iarmmgrs-hal
#mkdir -p headers/rdk/halif/
#mkdir -p headers/rdk/halif/deepsleep-manager
#mkdir -p headers/ccec/drivers
#mkdir -p headers/network
#mkdir -p headers/proc
echo "dir created successfully"
echo "======================================================================================"

echo "======================================================================================"
echo "empty headers creation"
cd headers
echo "current working dir: "${PWD}
echo "files created successfully"
echo "======================================================================================"

cd ../../
#cp -r /usr/include/gstreamer-1.0/gst /usr/include/glib-2.0/* /usr/lib/x86_64-linux-gnu/glib-2.0/include/* /usr/local/include/trower-base64/base64.h .

ls -la ${GITHUB_WORKSPACE}