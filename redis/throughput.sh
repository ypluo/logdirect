##!/bin/bash

TESTS=SET,GET
clientnum="1 2 4 8 16 24 32"
PAYLOAD=40

for cn in $clientnum; do
    echo $cn

    /home/lyp/redis/src/redis-server /home/lyp/redis/redis.conf --io-threads ${cn} >> /home/lyp/redis/server.log &
    sleep 5 # wait for the server to recover from initial database
    
    # run benchmark at another machine
    sshpass -p "xxxxxx" ssh lyp@192.168.2.2 \
        "./redis-benchmark -h 192.168.2.1 -n 8000000 -t ${TESTS} -d ${PAYLOAD} -r 67108864 -c ${cn} --threads ${cn}" 2>>/home/lyp/redis/client.log
    
    # clear files and server thread
    ps aux | grep /home/lyp/redis/src/redis-server | awk '/grep/{next}{print $2}' | xargs kill -9
    rm redislog -r
    sleep 2
done