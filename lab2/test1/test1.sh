#!/bin/sh

# display interrupts before and after a 500 times run
cat /proc/interrupts | grep dma_int
./test1.exe 500 2048
cat /proc/interrupts | grep dma_int