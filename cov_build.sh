#!/bin/bash
set -e
set -x

##############################
GITHUB_WORKSPACE="${PWD}"

ls -la "${GITHUB_WORKSPACE}"

echo "building xdialserver"

cd "${GITHUB_WORKSPACE}/server"

rm -rf CMakeCache.txt CMakeFiles

cmake .
make

echo "===== xdialserver build completed successfully ====="
