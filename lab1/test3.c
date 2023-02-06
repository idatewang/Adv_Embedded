#include "dm.c"
#include "pm.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
/*********************************************************************
   # DEFINES
*/

#undef DEBUG
#define DEBUG         // Uncomment to enable debug mode

#define CDMA                0xB0000000
#define BRAM_PS             0xa0028000
#define BRAM_CDMA           0xb0028000
#define OCM                 0xFFFC0000


#define CDMACR              0x00        // Control register
#define CDMASR              0x04        // Status register
#define CURDESC_PNTR        0x08
#define SA                  0x18        // Source Address
#define DA                  0x20        // Destination Address
#define BTT                 0x28
#define uint32_t unsigned int
int number = 2048 * 4;


/*************************** DMA_SET ************************************
*
*/

unsigned int dma_set(unsigned int *dma_virtual_address, int offset, unsigned int value) {
    dma_virtual_address[offset >> 2] = value;
}

/*************************** DMA_GET ************************************
*
*/

unsigned int dma_get(unsigned int *dma_virtual_address, int offset) {
    return dma_virtual_address[offset >> 2];
}

/***************************  CDMA_SYNC **********************************
*
* This is polling loop that waits for the DMA to complete. Need to replace
* with interrupt capability
*/

int cdma_sync(unsigned int *dma_virtual_address) {
    unsigned int status = dma_get(dma_virtual_address, CDMASR);
    if ((status & 0x40) != 0) {
        unsigned int desc = dma_get(dma_virtual_address, CURDESC_PNTR);
        printf("error address : %X\n", desc);
    }
    while (!(status & 1 << 1)) {
        status = dma_get(dma_virtual_address, CDMASR);
    }
}

/***************************  MEMDUMP ************************************
*/

void memdump(void *virtual_address, int byte_count) {
    char *p = virtual_address;
    int offset;
    for (offset = 0; offset < byte_count; offset++) {
        printf("%02x", p[offset]);
        if (offset % 4 == 3) { printf(" "); }
    }
    printf("\n");
}

/***************************  TRANSFER ************************************
*
*/

void transfer(unsigned int *cdma_virtual_address, int length) {
    // transfer FFFC to b002
    dma_set(cdma_virtual_address, DA, BRAM_CDMA);   // Write destination address
    dma_set(cdma_virtual_address, SA, OCM);         // Write source address
    dma_set(cdma_virtual_address, CDMACR, 0x1000);  // Enable interrupts
    dma_set(cdma_virtual_address, BTT, length * 4);
    cdma_sync(cdma_virtual_address);
    dma_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts
    // transder b002 to 2000
    dma_set(cdma_virtual_address, DA, OCM + 0x2000);   // Write destination address
    dma_set(cdma_virtual_address, SA, BRAM_CDMA);         // Write source address
    dma_set(cdma_virtual_address, CDMACR, 0x1000);  // Enable interrupts
    dma_set(cdma_virtual_address, BTT, length * 4);
    cdma_sync(cdma_virtual_address);
    dma_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts
}

/**************************************************************************
                                MAIN
**************************************************************************/

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
    int count = 0;
    int loop_flag = 1;
    if (argc == 3) {
        count = strtoul(argv[1], 0, 0);
        loop_flag = count;
        number = strtoul(argv[2], 0, 0) * 4;
    }

    srand(time(0));         // Seed the random number generator

    while (loop_flag) {
        if (count>0) {
            count-=1;
            loop_flag-=1;
        }
        // Open /dev/mem which represents the whole physical memory
        int dh = open("/dev/mem", O_RDWR | O_SYNC);
        if (dh == -1) {
            printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
            printf("Must be root to run this routine.\n");
            return -1;
        }

        uint32_t *cdma_virtual_address = mmap(NULL,
                                              4096,
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED,
                                              dh,
                                              CDMA); // Memory map AXI Lite register block
        //printf("cdma_virtual_address = 0x%.8x\n", cdma_virtual_address);
        uint32_t *BRAM_virtual_address = mmap(NULL,
                                              4096,
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED,
                                              dh,
                                              BRAM_PS); // Memory map AXI Lite register block
        //printf("BRAM_virtual_address = 0x%.8x\n", BRAM_virtual_address);
        // Setup data to be transferred
        uint32_t c[1024] = {};
        for (int i = 0; i < 1024; ++i) {
            c[i] = rand();
        }
        clk_rng();

        //printf("Starting memory allocation section\n");
        uint32_t *ocm = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, dh, OCM);

        printf("OCM virtual address = 0x%.8x\n", ocm);

        // Setup data in OCM to be transferred to the BRAM
        for (int i = 0; i < 1024; i++)
            ocm[i] = c[i];

        // RESET DMA
        dma_set(cdma_virtual_address, CDMACR, 0x0004);

        //printf("Source memory block:      "); memdump(ocm, 32);
        //printf("Destination memory block: "); memdump(BRAM_virtual_address, 32);
        sleep(2);
        printf("Sleeping...\n");
        transfer(cdma_virtual_address, 1024);
        //printf("DMA Registers:            "); memdump(cdma_virtual_address, 32);

        for (int i = 0; i < 1024; i++) {
            if (BRAM_virtual_address[i] != c[i]) {
                printf("RAM result: 0x%.8x and c result is 0x%.8x  element %d\n",
                       BRAM_virtual_address[i], c[i], i);
                printf("test failed!!\n");
                munmap(ocm, 65536);
                munmap(cdma_virtual_address, 4096);
                munmap(BRAM_virtual_address, 4096);
                return -1;
            }
        }
        printf("Loop: %i: test passed!!\n", loop_flag);
        munmap(ocm, 65536);
        munmap(cdma_virtual_address, 4096);
        munmap(BRAM_virtual_address, 4096);
        system("./sha_comp_in_C.sh");
    }
    return 0;
}
