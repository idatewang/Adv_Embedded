#!/bin/sh

while ( true  ) do
./dma_latency.exe
cat /proc/interrupts | grep gpio_int
done
