#!/bin/sh

while ( true  ) do
./test1.exe 35 2048
cat /proc/interrupts | grep dma_int
done
