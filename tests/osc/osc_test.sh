#!/bin/bash

max=1
if (( $# > 0 )); then
    if (( $1 > 1 )); then
        max=$1
    fi
fi

sleep 2

while :
do
   echo -n "."
   for s in $(seq 0 $max); do
        oscsend localhost 7000 /vimix/#$s/alpha f "0.$((RANDOM%9999))"
        oscsend localhost 7000 /vimix/#$s/gamma f "0.$((RANDOM%9999))"
   done
   sleep 0.1
done

