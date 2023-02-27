#!/bin/sh

rm /dev/dma_int
/bin/mknod /dev/dma_int c 236 0
sync
/sbin/rmmod dma_int
/sbin/insmod dma_int.ko

