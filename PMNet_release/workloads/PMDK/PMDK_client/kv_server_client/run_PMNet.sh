#!/bin/bash
i=1;
for j in 0.25 0.5 0.75 1.0 ; do
./redisClient 100000 $j 1000 $i 51000 1 ;
done