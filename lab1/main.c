/*
 * Duo Wang
 * dw28746
 * 2/2/2023
 * Derived from frm.c
*/

#include "stdio.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "dm.c"
#include "pm.c"

#define uint32_t unsigned int
int ps_range[] = {45, 75, 30, 52, 25};
int pl_range[] = {5, 6, 8, 10, 15};
int number = 2048 * 4;

void clk_rng() {
    double ps_clk;
    double pl_clk;
    // PS clk:

    // get rand ps_range, if else for APLL_CTRL and APLL_CFG, ps_clk
    int ps_index = rand() % 5;
    int APLL_CTRL;
    int APLL_CFG;
    if (ps_index == 0) {
        ps_clk = 1499;
        APLL_CTRL = 45 << 8;
        APLL_CFG = (3 << 5) + 12 + (3 << 10) + (63 << 25) + (825 << 13);
    } else if (ps_index == 1) {
        ps_clk = 1250;
        APLL_CTRL = (75 << 8) + (1 << 16);
        APLL_CFG = (3 << 5) + 2 + (3 << 10) + (63 << 25) + (600 << 13);
    } else if (ps_index == 2) {
        ps_clk = 1000;
        APLL_CTRL = 30 << 8;
        APLL_CFG = (4 << 5) + 6 + (3 << 10) + (63 << 25) + (1000 << 13);
    } else if (ps_index == 3) {
        ps_clk = 858;
        APLL_CTRL = (52 << 8) + (1 << 16);
        APLL_CFG = (3 << 5) + 2 + (3 << 10) + (63 << 25) + (700 << 13);
    } else if (ps_index == 4) {
        ps_clk = 416.6;
        APLL_CTRL = (25 << 8) + (1 << 16);
        APLL_CFG = (3 << 5) + 10 + (3 << 10) + (63 << 25) + (1000 << 13);
    }
    pm(0xfd1a0020, APLL_CTRL, number);
    pm(0xfd1a0024, APLL_CFG, number);
    // program bypass
    pm(0xfd1a0020, APLL_CTRL + 8, number);
    // assert reset
    pm(0xfd1a0020, APLL_CTRL + 9, number);
    // deassert reset
    pm(0xfd1a0020, APLL_CTRL + 8, number);
    // while check for lock
    while ((dm(0xfd1a0044, number) & 1) != 0x1) {
        sleep(1);
        printf("Waiting check for lock\n");
    }
    // deassert bypass
    pm(0xfd1a0020, APLL_CTRL, number);
    printf("PS switched to clock %f MHz with random index %i\n", ps_clk, ps_index);

    // PL clk:
    int dh = open("/dev/mem", O_RDWR | O_SYNC);
    if (dh == -1) {
        printf("Unable to open /dev/mem\n");
    }
    uint32_t *clk_reg = mmap(NULL,
                             0x1000,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, dh, 0xFF5E0000);
    uint32_t *pl0 = clk_reg;
    pl0 += 0xC0 >> 2; // PL0_REF_CTRL reg offset 0xC0
    int divisor;
    // get rand pl_range, if else for pl_clk
    int pl_index = rand() % 5;
    divisor = pl_range[pl_index];
    if (pl_index == 0) {
        pl_clk = 300;
    } else if (pl_index == 1) {
        pl_clk = 250;
    } else if (pl_index == 2) {
        pl_clk = 187.5;
    } else if (pl_index == 3) {
        pl_clk = 150;
    } else if (pl_index == 4) {
        pl_clk = 100;
    }
    *pl0 = (1 << 24) // bit 24 enables clock
           | (1 << 16) // bit 23:16 is divisor 1
           | (divisor << 8); // bit 15:0 is clock divisor 0
    munmap(clk_reg, 0x1000);
    printf("PL switched to clock %f MHz with random index %i\n", pl_clk, pl_index);

}

int main(int argc, char *argv[]) {
    int count = 1;
    int loop_flag = 0;
    // if argc == 3, set loop count to argv[1] and turn flag to 1, set number to argv[2]
    if (argc == 3) {
        count = strtoul(argv[1], 0, 0);
        loop_flag = 1;
        number = strtoul(argv[2], 0, 0) * 4;
    }

    srand(time(0));         // Seed the random number generator

    /* ---------------------------------------------------------------
    *   Main loop
    */
    while (count) {
        // generate random address within 0xA002_8000 - 0xA002_9FFC since RAND_MAX is defined as a 4 byte value 0x7FFF_FFFF
        int address = rand() % 8188 + 0xA0028000;
        // generate random value
        int value = rand();
        // call clk_rng to change rand clocks
        clk_rng();
        // use pm to program the data at the address
        pm(address, value, number);
        // use dm to check for correctness and print output "Test passed: "xx" loops of "yy" 32-bit words
        if (dm(address, number) == value) {
            printf("Test passed: %i loops of %i 32-bit words with %i\n", count, number, value);
        } else {
            printf("Test failed: %i doesn't match %i\n", dm(address, number), value);
        }
        // check loop flag to decrement count
        if (loop_flag) {
            count -= 1;
        }
    }
    return 0;
}
