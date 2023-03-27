#!/bin/sh

rm /dev/sha_interrupt
/bin/mknod /dev/sha_interrupt c 235 0
sync
/sbin/rmmod sha_interrupt
/sbin/insmod sha_interrupt.ko

