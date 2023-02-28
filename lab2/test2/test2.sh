#!/bin/sh

while ( true  ) do
./test2 500 2048
cat /proc/interrupts | grep gpio_int
done
