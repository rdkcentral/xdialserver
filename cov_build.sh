#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
############################
# Build xdialserver
echo "buliding xdialserver"

cd ${GITHUB_WORKSPACE}
cmake -G Ninja -S "$GITHUB_WORKSPACE" -B build/xdialserver \
-DUSE_THUNDER_R4=ON \
-DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
-DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
-DCMAKE_VERBOSE_MAKEFILE=ON \
-DCMAKE_CXX_FLAGS="-DEXCEPTIONS_ENABLE=ON \
-I ${GITHUB_WORKSPACE}/Thunder/Source \
-I ${GITHUB_WORKSPACE}/Thunder/Source/core" \

cmake --build build/xdialserver --target install
echo "======================================================================================"
exit 0
