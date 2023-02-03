/*
 *  Part 1. derived from frm.c
 *
 *  author: Mark McDermott
 *  Created: Feb 12, 2012
 *
      frm - Fill memory with random data
      USAGE:  frm (address) (count)

      Example:  frm 0x40000000 0x5
                0x40000000 = 0x3d6af58a
                0x40000004 = 0x440be909
                0x40000008 = 0x25b16123
                0x4000000c = 0x648b26f3
                0x40000010 = 0x177985f0
 *
 */

#include "stdio.h"
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "dm.c"
#include "pm.c"

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

int ps_range[] = {45, 75, 30, 52, 25};

int ol_range[] = {5, 6, 8, 10, 15};

void clk_rng() {
    int ps_clk;
    int pl_clk;
    // PS clk:

    // get rand ps_range, switch case for APLL_CTRL and APLL_CFG, ps_clk
    // program bypass
    // assert reset
    // deassert reset
    // while check for lock
    // deassert bypass

    // PL clk
    int dh = open("/dev/mem", O_RDWR | O_SYNC);
    if (dh == -1) {
        printf("Unable to open /dev/mem\n");
    }
    uint32_t *clk_reg = mmap(NULL,
                             0x1000,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, dh, 0xFF5E0000);
    uint32_t *pl0 = clk_reg;
    pl0 += 0xC0; // PL0_REF_CTRL reg offset 0xC0
    int divisor;
    // get rand pl_range, switch case for pl_clk

    *pl0 = (1 << 24) // bit 24 enables clock
           | (1 << 16) // bit 23:16 is divisor 1
           | (divisor << 8); // bit 15:0 is clock divisor 0
    munmap(clk_reg, 0x1000);

}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Lab 1 - USAGE:  (loop count) (number of 32 bit words) \n");
        return -1;
    }
    int count = 1;
    int loop_flag = 0;
    int number = 2048;
    // if argc == 3, set loop count to argv[1] and turn flag to 1, set number to argv[2]
    if (argc == 3) {
        count = strtoul(argv[1], 0, 0);
        loop_flag = 1;
        number = strtoul(argv[2], 0, 0);
    }
    /* ---------------------------------------------------------------
    *   Main loop
    */
    while (count) {
        // generate random address within 0xA002_8000 - 0xA002_9FFC since RAND_MAX is defined as a 4 byte value 0x7FFF_FFFF
        int address = rand() % 8188 + 0xA0028000;
        // generate random value
        int value = rand();
        // call clk_rng to change rand clocks
        // use pm to program the data at the address
        // use dm to check for correctness and print output "Test passed: "xx" loops of "yy" 32-bit words
        // check loop flag to decrement count
    }
    return 0;
}
