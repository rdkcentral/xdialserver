#!/bin/bash
set -e
set -x

##############################
GITHUB_WORKSPACE="${PWD}"
INSTALL_DIR="$GITHUB_WORKSPACE/install"

export PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig:$INSTALL_DIR/lib64/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$INSTALL_DIR/lib64:$LD_LIBRARY_PATH"
export PATH="$INSTALL_DIR/bin:$INSTALL_DIR/sbin:$PATH"

ls -la "${GITHUB_WORKSPACE}"

############################
# Build xdialserver
echo "building xdialserver"

cd "${GITHUB_WORKSPACE}"

make

echo "======================================================================================"
