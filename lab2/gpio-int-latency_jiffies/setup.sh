#!/bin/sh

rm /dev/gpio_int
/bin/mknod /dev/gpio_int c 237 0
sync
/sbin/rmmod gpio_int
/sbin/insmod gpio_int.ko

