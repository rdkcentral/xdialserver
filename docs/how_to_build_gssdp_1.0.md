# Build `xdialServer` with custom `gssdp-1.0`

### step 1: checkout `gssdp` source code
Goto workspace where you can checkout `gssdp` source code on branch `gssdp-1.0`
```
git clone https://gitlab.gnome.org/GNOME/gssdp.git
git checkout gssdp-1.0
```

### step 2: export propert path environment variable
```
export GSSDP_WORKSPACE=<full path to parent of gssdp checkout directory>
export XDIAL_WORKSPACE=<full path to parent of xdialserver checkout directory>

cd $GSSDP_WORKSPACE/gssdp
mkdir -p $GSSDP_WORKSPACE/gssdp/builddir
mkdir -p $GSSDP_WORKSPACE/gssdp/install
```

### step 3: build `gssdp`
You may need to intall `gtk-doc-tools` if it is not yet installed
```
sudo apt-get install gtk-doc-tools
```

```
cd $GSSDP_WORKSPACE/gssdp
sh autogen.sh --prefix=`pwd`/install

./configure \
  --without-gtk \
  --enable-debug=no \
  --enable-compile-warnings=no \
  --prefix=`pwd`/install
  
make
```
### step 4: install custom `gssdp`
It is important you specify `DESTDIR=/` in the install command
```
# install
DESTDIR=/ make install
```


### step 5: (Re)build `xdialserver` with the custom `gssdp 1.0`

Now the `gssdp` header and libs should be available under `$GSSDP_WORKSPACE/gssdp/install` directory
Goto the `xdialServer` workspace and setup references to the custom `gssdp build`
```
cd $XDIALSERVER_WORKSPACE/xdialserver
export PKG_CONFIG_PATH=$GSSDP_WORKSPACE/gssdp/install/lib/pkgconfig
```
`cmake` will detect the custom `gssdp-1.0` via `pkg-config`
```
mkdir -p $XDIALSERVER_WORKSPACE/xdialserver/server/build
cd server/build
cmake -DCMAKE_BUILD_TYPE=CI ..
make
```
