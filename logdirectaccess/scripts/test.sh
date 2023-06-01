#!/bin/bash

if [[ $# -lt 2 ]]; then
    echo -e "Usage: ${0} [sync/group/hybrid] [clientnum] [0/1]"
    exit -1
fi

fronttype=$1
clientnum=$2
latmode=$3

BUILD_DIR=/home/lyp/logdirect/build
DATA_DIR=/data/nvme0
DB_TYPE=bitcask

rm -r ${DATA_DIR}/${DB_TYPE}

${BUILD_DIR}/server -f ${fronttype} -d ${DB_TYPE} 2>>${BUILD_DIR}/server.log &
sleep 0.2
${BUILD_DIR}/benchmark -f ${fronttype} -l ${latmode} -c ${clientnum} 2>>${BUILD_DIR}/client.log
ps aux | grep ${BUILD_DIR}/server | awk '/grep/{next}{print $2}' | xargs kill -9