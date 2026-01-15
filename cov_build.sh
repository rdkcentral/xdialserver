#!/bin/bash
set -e
set -x

##############################
# Paths
GITHUB_WORKSPACE="${PWD}"
INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr"

##############################
# Environment
export CMAKE_PREFIX_PATH="$INSTALL_PREFIX"
export PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig:$INSTALL_PREFIX/lib/x86_64-linux-gnu/pkgconfig"
export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:$INSTALL_PREFIX/lib/x86_64-linux-gnu"

##############################
# Build xdialserver (out-of-source)
cmake -G Ninja \
  -S "$GITHUB_WORKSPACE/server" \
  -B build/xdialserver \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
  -DCMAKE_CXX_FLAGS="\
-I$INSTALL_PREFIX/include/WPEFramework \
-I$INSTALL_PREFIX/include/WPEFramework/core \
-I$INSTALL_PREFIX/include/WPEFramework/plugins \
-I$INSTALL_PREFIX/include/WPEFramework/interfaces" \
  -DCMAKE_SHARED_LINKER_FLAGS="-L$INSTALL_PREFIX/lib" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$INSTALL_PREFIX/lib" \
  -DCMAKE_BUILD_RPATH="$INSTALL_PREFIX/lib" \
  -DCMAKE_INSTALL_RPATH="$INSTALL_PREFIX/lib"

cmake --build build/xdialserver
