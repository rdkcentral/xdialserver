#!/bin/bash
source ../ci/scripts/int_test_env.sh
source ../ci/scripts/int_test_util.sh

echo "Running xDIAL tests from ${HOSTIP} at dir $(pwd) ..."

bash ../ci/scripts/startXdial.sh &
echo "Waiting 1 sec for gdial-server to start..."
sleep ${XDIALSERVER_START_DELAY}

### ### ### ### ### ### #####
### Test cases start here ###
### ### ### ### ### ### #####

# TEST: valid origin HTTP
netflixOrigin="http://www.netflix.com"
retCode=$(curl -s -I -X GET -H "Origin:${netflixOrigin}" http://${HOSTIP}:56889/apps/Netflix |grep HTTP  |  awk '{ print $2 }' | tr -d '\n')
[ "${retCode}" -ne  "200" ] && echo "failed: expecting 200 for http, but actual=HTTP ${retCode}!" && error_exit;

# TEST: valid origin HTTPS
netflixOrigin="https://www.netflix.com"
retCode=$(curl -s -I -X GET -H "Origin:${netflixOrigin}" http://${HOSTIP}:56889/apps/Netflix |grep HTTP  |  awk '{ print $2 }' | tr -d '\n')
[ "${retCode}" -ne  "200" ] && echo "failed: expecting 200 for https, but actual=HTTP ${retCode}!" && error_exit;

# TEST: valid origin FILE
netflixOrigin="file://www.netflix.com"
retCode=$(curl -s -I -X GET -H "Origin:${netflixOrigin}" http://${HOSTIP}:56889/apps/Netflix |grep HTTP  |  awk '{ print $2 }' | tr -d '\n')
[ "${retCode}" -ne  "200" ] && echo "failed: expecting 200 for file , but actual=HTTP ${retCode}!" && error_exit;

# TEST: invalid origin HTTP
netflixOrigin="http://www.netflix.us"
retCode=$(curl -s -I -X GET -H "Origin:${netflixOrigin}" http://${HOSTIP}:56889/apps/Netflix |grep HTTP  |  awk '{ print $2 }' | tr -d '\n')
[ "${retCode}" -ne  "403" ] && echo "failed: expecting 403 for .us, but actual=HTTP ${retCode}!" && error_exit;

# TEST: valid any origin ftp 
netflixOrigin="ftp://www.example.com"
retCode=$(curl -s -I -X GET -H "Origin:${netflixOrigin}" http://${HOSTIP}:56889/apps/Netflix |grep HTTP  |  awk '{ print $2 }' | tr -d '\n')
[ "${retCode}" -ne  "200" ] && echo "failed: expecting 200 for ftp, but actual=HTTP ${retCode}!" && error_exit;

success_exit
