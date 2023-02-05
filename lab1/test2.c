/*
 * Duo Wang
 * dw28746
 * 2/4/2023
 * Derived from frm.c
*/

#include "stdio.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "dm.c"
#include "pm.c"

int ps_range[] = {45, 75, 30, 52, 25};
int pl_range[] = {5, 6, 8, 10, 15};
int number = 4096 * 4;

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
        // generate random address within 0xFFFC_0000 - 0xFFFD_FFFC since RAND_MAX is defined as a 4 byte value 0x7FFF_FFFF
        int address = rand() % 131068 + 0xFFFC0000;
        // generate random value
        int value = rand();
        // call clk_rng to change rand clocks
        clk_rng();
        // use pm to program the data at the address
        pm(address, value, number);
        // use dm to check for correctness and print output "Test passed: "xx" loops of "yy" 32-bit words
        if (dm(address, number) == value) {
            printf("Test passed: %i loops of %i 32-bit words with %i on address 0x%x\n", count, number, value, address);
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
