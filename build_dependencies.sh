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

############################
# generating extrnal headers
cd $GITHUB_WORKSPACE
cd xdialserver/tests
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