#!/bin/bash

if [[ $# -lt 1 ]]; then
    echo -e "Usage: ${0} [1/2/3/4]"
    exit -1
fi

pathtype=$1

clientnum="1 2 4 8 12 16 20 24 28 32"
VALUE_SIZE=64
BUILD_DIR="/home/lyp/dmabuf/remotepath/build"

for cn in $clientnum; do
    echo $cn
    # run server locally
    ${BUILD_DIR}/throughput -t 1 -p ${pathtype} -n ${cn} -s ${VALUE_SIZE} 2>>${BUILD_DIR}/server.log &
    sleep 3 # wait for the server to start listening
    
    # run benchmark at another machine
    sshpass -p "xxxxxx" ssh lyp@192.168.2.1 \
       "${BUILD_DIR}/throughput -t 0 -p ${pathtype} -n ${cn} -s ${VALUE_SIZE} " 2>>${BUILD_DIR}/client.log
    # ${BUILD_DIR}/throughput -t 0 -p ${pathtype} -n ${cn} -s ${VALUE_SIZE} 2>>${BUILD_DIR}/client.log
    sleep 2
done