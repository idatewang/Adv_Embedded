#!/bin/sh

rm /dev/gpio_int
/bin/mknod /dev/gpio_int c 235 0
sync
/sbin/rmmod gpio_int
/sbin/insmod gpio_int.ko

