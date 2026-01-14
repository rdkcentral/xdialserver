#!/bin/bash
set -e
set -x

##############################
GITHUB_WORKSPACE="${PWD}"
cd "${GITHUB_WORKSPACE}"

##############################
# Install system dependencies
sudo apt update
sudo apt install -y \
    ninja-build meson cmake curl \
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
meson setup build
ninja -C build
sudo ninja -C build install
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
cd gssdp
meson setup build
ninja -C build
sudo ninja -C build install
cd ..

##############################
# Build ThunderTools
cd ThunderTools
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/00010-R4.4-Add-support-for-project-dir.patch"
cd ..

cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DCMAKE_INSTALL_PREFIX=/usr

sudo cmake --build build/ThunderTools --target install

##############################
# Build Thunder
cd Thunder
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/error_code_R4_4.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/1004-Add-support-for-project-dir.patch"
patch -p1 < "$GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-733-Add-ENTOS-IDS.patch"
cd ..

cmake -G Ninja -S Thunder -B build/Thunder \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DMESSAGING=ON \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON

sudo cmake --build build/Thunder --target install

##############################
# Build entservices-apis
cd entservices-apis
rm -rf jsonrpc/DTV.json
cd ..

cmake -G Ninja -S entservices-apis -B build/entservices-apis \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DEXCEPTIONS_ENABLE=ON

sudo cmake --build build/entservices-apis --target install

##############################
# Build and install stubs (system-wide)
sudo mkdir -p /usr/include/WPEFramework/securityagent
sudo cp stubs/securityagent/* /usr/include/WPEFramework/securityagent/

cd stubs

g++ -fPIC -shared -o libIARMBus.so iarm_stubs.cpp \
    -I"$GITHUB_WORKSPACE/stubs" \
    -I/usr/include/glib-2.0 \
    -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
    -I"$IARMBUS_PATH/core" \
    -I"$IARMBUS_PATH/core/include" \
    -fpermissive

g++ -fPIC -shared -o libWPEFrameworkSecurityUtil.so \
    securityagent/SecurityTokenUtil.cpp \
    -I"$GITHUB_WORKSPACE/stubs" \
    -fpermissive

sudo cp libIARMBus.so /usr/lib/
sudo cp libWPEFrameworkSecurityUtil.so /usr/lib/
sudo ldconfig

echo "===== Script 1 completed successfully (Option 1) ====="
