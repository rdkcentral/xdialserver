#!/bin/bash
set -e
set -x

##############################
# Workspace setup
GITHUB_WORKSPACE="${PWD}"
INSTALL_DIR="$GITHUB_WORKSPACE/install"

mkdir -p "$INSTALL_DIR"

export PATH="$INSTALL_DIR/bin:$INSTALL_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$INSTALL_DIR/lib64:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig:$INSTALL_DIR/lib64/pkgconfig:$PKG_CONFIG_PATH"

cd "$GITHUB_WORKSPACE"

##############################
# 1. Install system dependencies (still needs sudo)
sudo apt update
sudo apt install -y \
    ninja-build meson curl \
    libsoup2.4-dev libxml2-dev libglib2.0-dev \
    gobject-introspection libgirepository1.0-dev \
    libgtk-3-dev libcunit1-dev \
    valac pandoc

pip install jsonref

##############################
# Build trower-base64
if [ ! -d "trower-base64" ]; then
    git clone https://github.com/xmidt-org/trower-base64.git
fi

cd trower-base64
meson setup build --prefix="$INSTALL_DIR"
ninja -C build
ninja -C build install
cd ..

##############################
# Clone repositories
rm -rf iarmbus ThunderTools Thunder entservices-apis entservices-testframework gssdp

git clone https://github.com/rdkcentral/iarmbus.git
export IARMBUS_PATH="$GITHUB_WORKSPACE/iarmbus"

git clone --branch R4.4.3 https://github.com/rdkcentral/ThunderTools.git
git clone --branch R4.4.1 https://github.com/rdkcentral/Thunder.git
git clone --branch main https://github.com/rdkcentral/entservices-apis.git
git clone https://$GITHUB_TOKEN@github.com/rdkcentral/entservices-testframework.git
git clone --branch gssdp-1.2.3 https://gitlab.gnome.org/GNOME/gssdp.git

##############################
# Build gssdp
echo "======================================================================================"
echo "building gssdp"

cd gssdp
meson setup build --prefix="$INSTALL_DIR"
ninja -C build
ninja -C build install
cd ..

##############################
# Build ThunderTools
echo "======================================================================================"
echo "building ThunderTools"

cd ThunderTools
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/00010-R4.4-Add-support-for-project-dir.patch"
cd ..

cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake"

cmake --build build/ThunderTools --target install

##############################
# Build Thunder
echo "======================================================================================"
echo "building Thunder"

cd Thunder
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/error_code_R4_4.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/1004-Add-support-for-project-dir.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-733-Add-ENTOS-IDS.patch"
cd ..

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON

cmake --build build/Thunder --target install

##############################
# Build entservices-apis
echo "======================================================================================"
echo "building entservices-apis"

cd entservices-apis
rm -rf jsonrpc/DTV.json
cd ..

cmake -G Ninja -S entservices-apis -B build/entservices-apis \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
    -DEXCEPTIONS_ENABLE=ON

cmake --build build/entservices-apis --target install

##############################
# Build IARMBus & SecurityAgent stubs (local)
echo "======================================================================================"
echo "Building stubs"

mkdir -p "$INSTALL_DIR/include/WPEFramework/securityagent"
cp stubs/securityagent/* "$INSTALL_DIR/include/WPEFramework/securityagent/"

cd stubs

g++ -fPIC -shared -o libIARMBus.so iarm_stubs.cpp \
    -I"$GITHUB_WORKSPACE/stubs" \
    -I"$INSTALL_DIR/include" \
    -I"$IARMBUS_PATH/core" \
    -I"$IARMBUS_PATH/core/include" \
    -fpermissive

g++ -fPIC -shared -o libWPEFrameworkSecurityUtil.so \
    securityagent/SecurityTokenUtil.cpp \
    -I"$GITHUB_WORKSPACE/stubs" \
    -I"$INSTALL_DIR/include" \
    -fpermissive

cp libIARMBus.so "$INSTALL_DIR/lib/"
cp libWPEFrameworkSecurityUtil.so "$INSTALL_DIR/lib/"
