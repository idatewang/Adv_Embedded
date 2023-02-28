#!/bin/sh

while ( true  ) do
./test2.exe 35 2048
cat /proc/interrupts | grep gpio
done
