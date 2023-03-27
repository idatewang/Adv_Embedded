#!/bin/sh

rm /dev/sha_int
/bin/mknod /dev/sha_int c 235 0
sync
/sbin/rmmod sha_int
/sbin/insmod sha_int.ko

