#!/bin/bash

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 DRAM/PMR(0/1) Read/Write(0/1) "
    exit 1;
fi

if [[ $1 -eq 1 ]]; then
    flag="-p"
fi

if [[ $2 -eq 1 ]]; then
    flag="${flag} -w"
fi

size="64 128 256 512 1024"
for s in ${size}; do
    # echo $s
    ./latency -s ${s} ${flag}
done