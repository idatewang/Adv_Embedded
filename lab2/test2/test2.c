/*
 * Duo Wang
 * dw28746
 * 2/5/2023
 * Derived from dma_ocm_test.c
*/

#include "dm.c"
#include "pm.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <sys/mman.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/version.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

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
/* -------------------------------------------------------------------------------
 *      Flag to indicate that a SIGIO signal has been processed
 */
static volatile sig_atomic_t sigio_signal_processed = 0;
volatile int rc;
sigset_t signal_mask, signal_mask_old, signal_mask_most;


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
//    unsigned int status = dma_get(dma_virtual_address, CDMASR);
//    if ((status & 0x40) != 0) {
//        unsigned int desc = dma_get(dma_virtual_address, CURDESC_PNTR);
//        printf("error address : %X\n", desc);
//    }
//    while (!(status & 1 << 1)) {
//        status = dma_get(dma_virtual_address, CDMASR);
//    }
    printf("inside cdma_sync\n");
//    if (sigio_signal_processed == 0) {
//        printf("inside suspend\n");
//
//        rc = sigsuspend(&signal_mask_most);
//
//        /* Confirm we are coming out of suspend mode correcly */
//        assert(rc == -1 && errno == EINTR && sigio_signal_processed);
//    }
    pm(0xa0050004, 1, 2048 * 2);
    while (!sigio_signal_processed){
        /* ---------------------------------------------------------------------
 * Assert dma output pin to trigger generation of edge sensitive interrupt:
 */
    }
    printf("outside while\n");
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
    int total_count = 0;
    // turn interrupt flag off before transfer
    sigio_signal_processed = 0;
    // assert timer_enable
    pm(0xa0050004, 2, 2048 * 2);
    // transfer FFFC to b002
    dma_set(cdma_virtual_address, DA, BRAM_CDMA);   // Write destination address
    dma_set(cdma_virtual_address, SA, OCM);         // Write source address
    dma_set(cdma_virtual_address, CDMACR, 0x1000);  // Enable interrupts
    dma_set(cdma_virtual_address, BTT, length * 4);
    // aggregate counts
    total_count += dm(0xa0050008, 2048 * 2);
    // wait for interrupt to be handled, counted and dropped the flag
    cdma_sync(cdma_virtual_address);
    // assert timer_enable
    pm(0xa0050004, 2, 2048 * 2);
    dma_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts
    // turn interrupt flag off before transfer
    sigio_signal_processed = 0;
    // transder b002 to 2000
    dma_set(cdma_virtual_address, DA, OCM + 0x2000);   // Write destination address
    dma_set(cdma_virtual_address, SA, BRAM_CDMA);         // Write source address
    dma_set(cdma_virtual_address, CDMACR, 0x1000);  // Enable interrupts
    dma_set(cdma_virtual_address, BTT, length * 4);
    // aggregate counts
    total_count += dm(0xa0050008, 2048 * 2);
    // deassert timer_enable
    pm(0xa0050004, 0, 2048 * 2);
    // wait for interrupt to be handled, counted and dropped the flag
    cdma_sync(cdma_virtual_address);
    dma_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts
    // print total counts
    printf("3. total count counted in 250 MHz: %d\n", total_count);
}

/**************************************************************************
                                MAIN
**************************************************************************/
int ps_range[] = {45, 30, 25};
int pl_range[] = {5, 8, 15};
int number = 2048 * 4;
volatile int sigio_signal_count = 0;
/* -------------------------------------------------------------------------------
 * Device path name for the dma device
 */
#define DMA_DEV_PATH    "/dev/dma_int"

volatile int rc;

/* -------------------------------------------------------------------------------
 * File descriptor for dma device
 */
int dma_dev_fd = -1;

void clk_rng() {
    double ps_clk;
    double pl_clk;
    // PS clk:

    // get rand ps_range, if else for APLL_CTRL and APLL_CFG, ps_clk
    int ps_index = rand() % 3;
    int APLL_CTRL;
    int APLL_CFG;
    if (ps_index == 0) {
        ps_clk = 1499;
        APLL_CTRL = ps_range[ps_index] << 8;
        APLL_CFG = (3 << 5) + 12 + (3 << 10) + (63 << 25) + (825 << 13);
    } else if (ps_index == 1) {
        ps_clk = 1000;
        APLL_CTRL = ps_range[ps_index] << 8;
        APLL_CFG = (4 << 5) + 6 + (3 << 10) + (63 << 25) + (1000 << 13);
    } else if (ps_index == 2) {
        ps_clk = 416.6;
        APLL_CTRL = (ps_range[ps_index] << 8) + (1 << 16);
        APLL_CFG = (3 << 5) + 10 + (3 << 10) + (63 << 25) + (1000 << 13);
    } else {
        ps_clk = 1499;
        APLL_CTRL = ps_range[ps_index] << 8;
        APLL_CFG = (3 << 5) + 12 + (3 << 10) + (63 << 25) + (825 << 13);
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
    int pl_index = rand() % 3;
    divisor = pl_range[pl_index];
    if (pl_index == 0) {
        pl_clk = 300;
    } else if (pl_index == 1) {
        pl_clk = 187.5;
    } else if (pl_index == 2) {
        pl_clk = 100;
    } else {
        pl_clk = 300;
    }
    *pl0 = (1 << 24) // bit 24 enables clock
           | (1 << 16) // bit 23:16 is divisor 1
           | (divisor << 8); // bit 15:0 is clock divisor 0
    munmap(clk_reg, 0x1000);
    printf("PL switched to clock %f MHz with random index %i\n", pl_clk, pl_index);
}

/* -----------------------------------------------------------------------------
 * SIGIO signal handler
 */

void sigio_signal_handler(int signo) {
    assert(signo == SIGIO);   // Confirm correct signal #
    sigio_signal_count++;
    printf("sigio_signal_handler called (signo=%d)\n", signo);
    /* -------------------------------------------------------------------------
     * Set global flag
     */
    sigio_signal_processed = 1;
}

int main(int argc, char *argv[]) {
    // interrupt part
    /* --------------------------------------------------------------------------
    *      Register signal handler for SIGIO signal:
    */
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof sig_action);
    sig_action.sa_handler = sigio_signal_handler;
    /* --------------------------------------------------------------------------
     *      Block all signals while our signal handler is executing:
     */
    (void) sigfillset(&sig_action.sa_mask);
    rc = sigaction(SIGIO, &sig_action, NULL);
    if (rc == -1) {
        perror("sigaction() failed");
        return -1;
    }
    /* -------------------------------------------------------------------------
     *      Open the device file
     */
    dma_dev_fd = open(DMA_DEV_PATH, O_RDWR);
    if (dma_dev_fd == -1) {
        perror("open() of " DMA_DEV_PATH " failed");
        return -1;
    }
    /* -------------------------------------------------------------------------
     * Set our process to receive SIGIO signals from the dma device:
     */
    rc = fcntl(dma_dev_fd, F_SETOWN, getpid());
    if (rc == -1) {
        perror("fcntl() SETOWN failed\n");
        return -1;
    }
    /* -------------------------------------------------------------------------
     * Enable reception of SIGIO signals for the dma_dev_fd descriptor
     */
    int fd_flags = fcntl(dma_dev_fd, F_GETFL);
    rc = fcntl(dma_dev_fd, F_SETFL, fd_flags | O_ASYNC);
    if (rc == -1) {
        perror("fcntl() SETFL failed\n");
        return -1;
    }
    /* ---------------------------------------------------------------------
     * NOTE: This next section of code must be excuted each cycle to prevent
     * a race condition between the SIGIO signal handler and sigsuspend()
     */

    (void) sigfillset(&signal_mask);
    (void) sigfillset(&signal_mask_most);
    (void) sigdelset(&signal_mask_most, SIGIO);
    (void) sigprocmask(SIG_SETMASK, &signal_mask, &signal_mask_old);
    (void) sigprocmask(SIG_SETMASK, &signal_mask_old, NULL);
    //assert(sigio_signal_count == i + 1);   // Critical assertion!!


    // RNG and transfer part
    int count = 0;
    int loop_flag = 1;
    if (argc == 3) {
        count = strtoul(argv[1], 0, 0);
        loop_flag = count;
        number = strtoul(argv[2], 0, 0) * 4;
    }

    srand(time(0));         // Seed the random number generator

    while (loop_flag) {
        if (count > 0) {
            count -= 1;
            loop_flag -= 1;
        }
        // Open /dev/mem which represents the whole physical memory
        int dh = open("/dev/mem", O_RDWR | O_SYNC);
        if (dh == -1) {
            printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
            printf("Must be root to run this routine.\n");
            return -1;
        }
        // Memory map AXI Lite register block
        uint32_t *cdma_virtual_address = mmap(NULL,
                                              8192,
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED,
                                              dh,
                                              CDMA);
        uint32_t *BRAM_virtual_address = mmap(NULL,
                                              8192,
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED,
                                              dh,
                                              BRAM_PS);
        uint32_t *ocm = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, dh, OCM);
        printf("OCM virtual address = 0x%.8x\n", ocm);
        // Setup data to be transferred
        uint32_t c[2048] = {};
        for (int i = 0; i < 2048; ++i) {
            c[i] = rand();
        }
        // Setup data in OCM to be transferred to the BRAM
        for (int i = 0; i < 2048; i++)
            ocm[i] = c[i];
        // RESET DMA
        dma_set(cdma_virtual_address, CDMACR, 0x0004);
        // generate random clocks
        clk_rng();
        // sleep from piazza
        printf("Sleeping...\n");
        sleep(2);
        // transfer starts
        printf("Transfer starts...\n");
        transfer(cdma_virtual_address, 2048);
        // check results
        for (int i = 0; i < 2048; i++) {
            if (BRAM_virtual_address[i] != c[i]) {
                printf("RAM result: 0x%.8x and c result is 0x%.8x  element %d\n",
                       BRAM_virtual_address[i], c[i], i);
                printf("test failed!!\n");
                munmap(ocm, 65536);
                munmap(cdma_virtual_address, 8192);
                munmap(BRAM_virtual_address, 8192);
                return -1;
            }
        }
        printf("Loop %i: test passed!!\n", loop_flag);
        munmap(ocm, 65536);
        munmap(cdma_virtual_address, 8192);
        munmap(BRAM_virtual_address, 8192);
        // calls shell script to compare results
        system("./sha_comp.sh");
    }
    (void) close(dma_dev_fd);
    return 0;
}