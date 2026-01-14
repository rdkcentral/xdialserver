#!/bin/bash
set -e
set -x

##############################
GITHUB_WORKSPACE="${PWD}"

ls -la "${GITHUB_WORKSPACE}"

echo "building xdialserver"

cd "${GITHUB_WORKSPACE}/server"

# 🔥 CRITICAL: clean cache from previous Option 2 builds
rm -rf CMakeCache.txt CMakeFiles

cmake .
make

echo "===== xdialserver build completed successfully ====="
