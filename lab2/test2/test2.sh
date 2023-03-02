#!/bin/sh

for i in {1..500};
do
./test2.exe 2048
cat /proc/interrupts | grep gpio
done
