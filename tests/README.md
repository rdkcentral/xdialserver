# Tests

This folder contains the xdialserver test harness and all unit-level (L1) tests.

## What Is Here

Current top-level layout:

- tests/L1Tests: L1 test sources and build recipe
- tests/mocks: shared mock code used by tests
- tests/Makefile.am: autotools entry for test subdirectories
- tests/README.md: this guide

Inside tests/L1Tests:

- tests/L1Tests/server: server-layer tests (REST, SSDP, shield, service, app lifecycle)
- tests/L1Tests/plat: platform integration tests for plat APIs and gdial.cpp adapter paths
- tests/L1Tests/utils: utility tests
- tests/L1Tests/stubs: shadow headers and test-specific C/C++ shim headers
- tests/L1Tests/mocks: C mocks compiled into run_L1Tests
- tests/L1Tests/tests: legacy unit file coverage entry points
- tests/L1Tests/test_main.cpp: gtest main
- tests/L1Tests/Makefile.am: source list and compile/link flags

## How The L1 System Works

The L1 test binary is a single executable:

- run_L1Tests

It links:

- test files from tests/L1Tests
- selected real implementation files from server and server/plat
- test stubs and mocks that replace external dependencies

Key behavior:

1. Real source + selective stubbing
- We compile real modules for behavior coverage, but override dependencies below those modules.
- Example: a real server/plat module can be tested while its OS/backend calls are stubbed.

2. Include shadowing for external frameworks
- tests/L1Tests/stubs is first on the include path.
- CI generates wrapper headers there so build-time includes resolve to local stub content instead of requiring full external frameworks.

3. One process, shared globals
- Most L1 tests run in the same process, so static/global state in C modules can leak between tests unless explicitly reset.
- Tests that touch module-level caches must clean up in TearDown.

## Build And Run Locally

Typical local flow:

1. Generate autotools files
- autoreconf -if

2. Configure with L1 enabled
- ./configure --enable-l1tests

3. Build
- make -C tests/L1Tests

4. Run
- ./tests/L1Tests/run_L1Tests

Optional single-test execution:

- ./tests/L1Tests/run_L1Tests --gtest_filter=GDialSsdpTest.*
- ./tests/L1Tests/run_L1Tests --gtest_filter=GDialPlatAppTest.*

## CI Workflow Summary

The workflow in .github/workflows/L1-tests.yml does the following:

1. Installs dependencies and builds googletest
2. Generates stub wrapper headers under tests/L1Tests/stubs
3. Builds run_L1Tests using autotools
4. Runs tests normally and under valgrind
5. Publishes test results, valgrind log, and coverage artifacts

## Adding Or Modifying Tests Safely

When adding tests:

1. Place file in the matching area
- server logic: tests/L1Tests/server
- platform logic: tests/L1Tests/plat
- utility helpers: tests/L1Tests/utils

2. Register file in tests/L1Tests/Makefile.am
- Add it to run_L1Tests_SOURCES or it will not compile in CI.

3. Isolate global state
- If module under test uses static/global variables, reset via module destroy/reset APIs in TearDown.
- Do not rely on test execution order.

4. Prefer deterministic async tests
- For GLib/libsoup event paths, avoid timing-sensitive sleeps when possible.
- Use explicit loop-driven completion or cancel/remove handles in tests to avoid race-driven flakes.

5. Keep assertions aligned with implementation contracts
- Validate against current API behavior and constants from headers.
- If a behavior changed intentionally, update tests and document the expected contract in the test name.

## Known Pitfalls (Important)

1. Static cache/state in SSDP tests
- server/gdial-ssdp.c caches dd.xml response and keeps process-global name/model overrides.
- If a test sets manufacturer/model through setter APIs, later tests may observe those values unless reset or overridden.

2. Async source lifecycle in platform app tests
- server/plat/gdial-plat-app.c async calls use GLib timeout sources.
- Running timer callbacks near teardown can trigger use-after-free races if test cleanup is not deterministic.
- Prefer cancel/remove flow tests over race-prone timer execution unless explicitly testing callback timing.

3. Header dependency visibility in C++ tests
- Some C headers expose GLib types such as gboolean.
- In C++ test translation units, include glib.h before such headers when needed to avoid type-resolution build breaks.

## Quick Troubleshooting

If CI fails but local passes:

1. Re-run only the failing fixture with gtest_filter.
2. Run under valgrind locally if available.
3. Check for shared static state and missing teardown cleanup.
4. Verify test source is included in tests/L1Tests/Makefile.am.
5. Check .github/workflows/L1-tests.yml for CI-only generated stubs that local build may not have.

## Maintainer Notes

- Keep this document updated whenever structure or test harness behavior changes.
- When introducing new shared stubs or wrappers, document where they are generated and why.
