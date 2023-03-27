#!/bin/sh

# display interrupts before and after a 500 times run
cat /proc/interrupts | grep gpio_int
./test1.exe 1 2048
cat /proc/interrupts | grep gpio_int