# Tests

`xdialServer` supports continus integration through github Action.
Please refers to [ci directory](https://github.com/rdkcentral/xdialserver/tree/master/server/ci) for CI test cases.

The same CI tests can also be run locally. Please select the network interface name that `xdialServer` should run on and set it to `XDIAL_HostIfname`.  This interface cannot be loopback `lo`.

After initial checkout, please do:

```
export XDIAL_HostIfname=eth0
cd xdialserver
mkdir -p server/build
cd server/build
cmake -DCMAKE_BUILD_TYPE=CI ..
make
```

To run the CI tests locally, under the `server/build` directory, please do
```
make test
```
or if you want verbose output
```
ctest --verbose
```
and to enable verbose glib logging
```
export G_MESSAGE_DEBUG=all
```

The CI tests cover the core DIAL functionalities and are platform agnostic. The platform integration tests such as Application Manager or Communication between dial and applications, should be tested at application platform level.
