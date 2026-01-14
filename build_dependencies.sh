#!/bin/bash
set -e
set -x

##############################
# Paths
GITHUB_WORKSPACE="${PWD}"
INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr"

mkdir -p "$INSTALL_PREFIX"

##############################
# System dependencies (sudo REQUIRED in GitHub Actions)
sudo apt update
sudo apt install -y \
  ninja-build meson cmake curl \
  libsoup2.4-dev libxml2-dev libglib2.0-dev \
  gobject-introspection libgirepository1.0-dev \
  libgtk-3-dev libcunit1-dev valac pandoc

pip install jsonref

##############################
# Build trower-base64
git clone https://github.com/xmidt-org/trower-base64.git || true
cd trower-base64
meson setup build --prefix="$INSTALL_PREFIX"
ninja -C build
ninja -C build install
cd ..

##############################
# Clone required repositories
git clone https://github.com/rdkcentral/iarmbus.git || true
export IARMBUS_PATH="$GITHUB_WORKSPACE/iarmbus"

git clone --branch R4.4.3 https://github.com/rdkcentral/ThunderTools.git || true
git clone --branch R4.4.1 https://github.com/rdkcentral/Thunder.git || true
git clone --branch main https://github.com/rdkcentral/entservices-apis.git || true
git clone https://$GITHUB_TOKEN@github.com/rdkcentral/entservices-testframework.git || true
git clone --branch gssdp-1.2.3 https://gitlab.gnome.org/GNOME/gssdp.git || true

##############################
# Build gssdp
cd gssdp
meson setup build --prefix="$INSTALL_PREFIX"
ninja -C build
ninja -C build install
cd ..

##############################
# Build ThunderTools
cd ThunderTools
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/00010-R4.4-Add-support-for-project-dir.patch"
cd ..

cmake -G Ninja -S ThunderTools -B build/ThunderTools \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake"

cmake --build build/ThunderTools --target install

##############################
# Build Thunder
cd Thunder
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/error_code_R4_4.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/1004-Add-support-for-project-dir.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-733-Add-ENTOS-IDS.patch"
cd ..

cmake -G Ninja -S Thunder -B build/Thunder \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
  -DMESSAGING=ON \
  -DBUILD_TYPE=Debug \
  -DEXCEPTIONS_ENABLE=ON

cmake --build build/Thunder --target install

##############################
# Build entservices-apis
cmake -G Ninja -S entservices-apis -B build/entservices-apis \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake"

cmake --build build/entservices-apis --target install

##############################
# Build and install IARMBus & SecurityAgent stubs
STUB_INCLUDE="$INSTALL_PREFIX/include"
STUB_LIB="$INSTALL_PREFIX/lib"

mkdir -p "$STUB_INCLUDE/WPEFramework/securityagent"
mkdir -p "$STUB_LIB"

cd "$GITHUB_WORKSPACE/stubs"

g++ -fPIC -shared -o libIARMBus.so iarm_stubs.cpp \
  -I"$GITHUB_WORKSPACE/stubs" \
  -I"$IARMBUS_PATH/core" \
  -I"$IARMBUS_PATH/core/include" \
  -I"$STUB_INCLUDE" \
  -fpermissive

g++ -fPIC -shared -o libWPEFrameworkSecurityUtil.so \
  securityagent/SecurityTokenUtil.cpp \
  -I"$GITHUB_WORKSPACE/stubs" \
  -I"$STUB_INCLUDE" \
  -fpermissive

cp libIARMBus.so "$STUB_LIB/"
cp libWPEFrameworkSecurityUtil.so "$STUB_LIB/"
cp securityagent/* "$STUB_INCLUDE/WPEFramework/securityagent/"

echo "========================================================"
echo "Dependencies built and installed into $INSTALL_PREFIX"
echo "========================================================"
