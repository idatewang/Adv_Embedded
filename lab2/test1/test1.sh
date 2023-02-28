#!/bin/sh

while ( true  ) do
./test1.exe 500 2048
cat /proc/interrupts | grep dma_int
done
