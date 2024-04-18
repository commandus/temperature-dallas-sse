#!/bin/sh
for N in $(seq 1 10000);
do
    D=`date +"%H%M%S"`
    wget -q -O - "http://localhost:1234/send?d=$D&n=$N"
    echo $r
done
