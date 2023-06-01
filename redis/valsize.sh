##!/bin/bash

TESTS=SET
VALSIZE="40 104 232 488 1000 2024 4072"
VALSIZE="4072"
THREAD=1

for v in $VALSIZE; do
    echo $v

    /home/lyp/redis/src/redis-server /home/lyp/redis/redis.conf --io-threads ${THREAD} >> /home/lyp/redis/server.log &
    sleep 5 # wait for the server to recover from initial database
    
    # run benchmark at another machine
    sshpass -p "xxxxxx" ssh lyp@192.168.2.2 \
        "./redis-benchmark -h 192.168.2.1 -n 8000000 -t ${TESTS} -d ${v} -r 67108864 -c ${THREAD} --threads ${THREAD}" 2>>/home/lyp/redis/client.log
    
    # clear files and server thread
    ps aux | grep /home/lyp/redis/src/redis-server | awk '/grep/{next}{print $2}' | xargs kill -9
    rm redislog -r
    sleep 1
done