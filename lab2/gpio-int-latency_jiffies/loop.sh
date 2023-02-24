#!/bin/sh

while ( true  ) do
./intr_latency.exe
cat /proc/interrupts | grep gpio_int
done
