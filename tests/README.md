# Tests

## L1 Tests (Unit Tests)

The L1 tests are unit tests built with Google Test (gtest) framework for xdialserver components.

### Structure

The test suite is organized by component to promote modularity and maintainability:

```
tests/
├── L1Tests/
│   ├── server/                     # REST/DIAL protocol tests
│   │   ├── test_gdialServer.cpp    # Test cases
│   │   ├── gdial_rest_stubs.h      # Stub declarations
│   │   └── gdial_rest_stubs.cpp    # Stub implementations
│   ├── plat/                       # Platform-specific tests
│   │   ├── test_gdialPlat.cpp      # Test cases
│   │   ├── gdial_plat_stubs.h      # Stub declarations
│   │   └── gdial_plat_stubs.cpp    # Stub implementations
│   ├── utils/                      # Utility function tests
│   │   ├── test_gdialUtil.cpp      # Test cases
│   │   ├── gdial_util_stubs.h      # Stub declarations
│   │   └── gdial_util_stubs.cpp    # Stub implementations
│   ├── stubs/                      # Shared test stubs
│   │   ├── xdialserver_test_stubs.h    # Combined stub interface
│   │   └── xdialserver_test_stubs.cpp  # Combined stub implementations
│   ├── mocks/                      # Shared mock implementations
│   │   ├── IarmBusMock.h           # IARM bus mock
│   │   └── IarmBusMock.cpp         # IARM bus mock implementation
│   ├── test_main.cpp               # Test runner entry point
│   └── Makefile.am                 # Autotools build configuration
├── mocks/xdialserver/              # xdialserver-specific shared mocks
├── Makefile.am
└── README.md
```

### Building Locally

Prerequisites:
- autoconf, automake, libtool, pkg-config
- Build tools: gcc, g++, make, cmake
- Google Test: libgtest-dev, libgmock-dev
- GLib: libglib2.0-dev, libgobject2.0-dev, libgio-2.0-dev
- DIAL/SSDP: libgssdp-1.0-dev (or libgssdp-1.2-dev), libsoup-2.4-dev (or libsoup3.0-dev)
- XML: libxml2-dev
- Other: uuid-dev, libdbus-1-dev, valgrind, lcov

Optional (for full platform support, not required for basic tests):
- WPEFramework: libwpeframework-dev
- IARM Bus: libiarmbus-dev

Install on Ubuntu 22.04:

```bash
sudo apt update
sudo apt install -y \
    autoconf automake libtool pkg-config \
    libgtest-dev libgmock-dev \
    build-essential g++ cmake \
    libglib2.0-dev libgobject2.0-dev libgio-2.0-dev \
    libgssdp-1.0-dev libsoup-2.4-dev libxml2-dev \
    uuid-dev libdbus-1-dev \
    valgrind lcov
```

Build steps:

```bash
# Generate configure script
autoreconf -if

# Configure with L1 tests enabled
./configure --enable-l1tests

# Build tests
make -C tests/L1Tests

# Run tests
./tests/L1Tests/run_L1Tests
```

### Adding New Tests

1. **Choose a component** — Add test cases to the appropriate subdirectory:
   - `server/` for REST/DIAL protocol tests
   - `plat/` for platform-specific component tests
   - `utils/` for utility function tests

2. **Create test file** — Add a new test cpp file with the pattern `test_*.cpp`
   ```cpp
   #include <gtest/gtest.h>
   
   class MyComponentTest : public ::testing::Test {
       protected:
           void SetUp() override { /* Initialize */ }
           void TearDown() override { /* Cleanup */ }
   };
   
   TEST_F(MyComponentTest, MyTestCase) {
       EXPECT_TRUE(true);
   }
   ```

3. **Add stubs if needed** — Create component-specific stub headers and implementations
   - `component_stubs.h` — Stub declarations
   - `component_stubs.cpp` — Stub implementations

4. **Update Makefile.am** — Add your test source files to the `run_L1Tests_SOURCES` list

### Test Organization

Tests follow the same component structure as the source code:

| Component | Location | Tests |
|-----------|----------|-------|
| REST/DIAL | `server/` | `test_gdialServer.cpp` |
| Platform | `plat/` | `test_gdialPlat.cpp` |
| Utilities | `utils/` | `test_gdialUtil.cpp` |
| Shared Stubs | `stubs/` | `xdialserver_test_stubs.*` |
| IARM/Mocks | `mocks/` | `IarmBusMock.*` |

### GitHub Actions CI

Tests are automatically built and run on:
- Push to `develop` and `main` branches
- Pull requests to `develop` and `main` branches

See [.github/workflows/L1-tests.yml](../.github/workflows/L1-tests.yml) for workflow details.
