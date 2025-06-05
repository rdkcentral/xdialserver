#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
############################
# Build xdialserver
echo "buliding xdialserver"

cd ${GITHUB_WORKSPACE}

make

echo "======================================================================================"
exit 0
