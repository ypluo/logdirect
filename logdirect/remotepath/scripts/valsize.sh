#!/bin/bash

if [[ $# -lt 1 ]]; then
    echo -e "Usage: ${0} [1/2/3/4]"
    exit -1
fi

pathtype=$1
VALUE_SIZE="64 128 256 512 1024 2048 4096 8192"
BUILD_DIR="/home/lyp/dmabuf/remotepath/build"

for vs in $VALUE_SIZE; do
    echo $vs
    # run server locally
    ${BUILD_DIR}/latency -t 1 -p ${pathtype} -s ${vs} 2>>${BUILD_DIR}/server.log &
    sleep 3 # wait for the server to start listening
    
    # run benchmark at another machine
    sshpass -p "xxxxxx" ssh lyp@192.168.2.1 \
        "${BUILD_DIR}/latency -t 0 -p ${pathtype} " 2>>${BUILD_DIR}/client.log
    # ${BUILD_DIR}/latency -t 0 -p ${pathtype} -s ${vs} 2>>${BUILD_DIR}/client.log
    sleep 2
done