/*
 * Duo Wang
 * dw28746
 * 3/3/2023
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
 * Device path name for the dma device
 */
#define GPIO_DEV_PATH    "/dev/gpio_int"

// global variables
int ps_range[] = {45, 30, 25};
int pl_range[] = {5, 8, 15};
int number = 2048 * 4;
int loop_count;
volatile int sigio_signal_count = 0;

/* -------------------------------------------------------------------------------
 * File descriptor for dma device
 */
int gpio_dev_fd = -1;

/* -------------------------------------------------------------------------------
 *      Flag to indicate that a SIGIO signal has been processed
 */
static volatile sig_atomic_t sigio_signal_processed = 0;
volatile int rc;
sigset_t signal_mask, signal_mask_old, signal_mask_most;
struct timeval start_timestamp;
/* -------------------------------------------------------------------------------
 *      Time stamp set in the last sigio_signal_handler() invocation:
 */
struct timeval sigio_signal_timestamp;


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

void cdma_sync() {
    /* ---------------------------------------------------------------------
 * Take a start timestamp for interrupt latency measurement
 */
    //printf("inside cdma\n");
    pm(0xa0050004, 1, 2048 * 2);
    (void) gettimeofday(&start_timestamp, NULL);
    if (sigio_signal_processed == 0) {

        rc = sigsuspend(&signal_mask_most);

        /* Confirm we are coming out of suspend mode correctly */
        assert(rc == -1 && errno == EINTR && sigio_signal_processed);
    }
    (void) sigprocmask(SIG_SETMASK, &signal_mask_old, NULL);
    // turn interrupt flag off before transfer, clear pin out
    sigio_signal_processed = 0;
    pm(0xa0050004, 0, 2048 * 2);
    //printf("outside while\n");
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


/**************************************************************************
                                MAIN
**************************************************************************/
/* ---------------------------------------------------------------
* sqrt routine
*/

unsigned long int_sqrt(unsigned long n) {
    unsigned long root = 0;
    unsigned long bit;
    unsigned long trial;

    bit = (n >= 0x10000) ? 1 << 30 : 1 << 14;
    do {
        trial = root + bit;
        if (n >= trial) {
            n -= trial;
            root = trial + bit;
        }
        root >>= 1;
        bit >>= 2;
    } while (bit);
    return root;
}

/* -----------------------------------------------------------------------------
 * Compute interrupt latency stats
 */
void compute_interrupt_latency_stats(
        unsigned long *min_latency_p,
        unsigned long *max_latency_p,
        double *average_latency_p,
        double *std_deviation_p,
        int *latency_array) {
    int i;
    unsigned long val;
    unsigned long min = ULONG_MAX;
    unsigned long max = 0;
    unsigned long sum = 0;
    unsigned long sum_squares = 0;

    for (i = 0; i < loop_count; i++) {
        val = latency_array[i];

        if (val < min) {
            min = val;
        }

        if (val > max) {
            max = val;
        }

        sum += val;
        sum_squares += val * val;
    }

    *min_latency_p = min;
    *max_latency_p = max;

    unsigned long average = (unsigned long) sum / loop_count;

    unsigned long std_deviation = int_sqrt((sum_squares / loop_count) -
                                           (average * average));


    *average_latency_p = average;
    *std_deviation_p = std_deviation;
}

void clk_iterate(int ps_index, int pl_index) {
    // PS clk:

    // get ps_range, if else for APLL_CTRL and APLL_CFG, ps_clk
    int APLL_CTRL;
    int APLL_CFG;
    if (ps_index == 0) {
        APLL_CTRL = ps_range[ps_index] << 8;
        APLL_CFG = (3 << 5) + 12 + (3 << 10) + (63 << 25) + (825 << 13);
    } else if (ps_index == 1) {
        APLL_CTRL = ps_range[ps_index] << 8;
        APLL_CFG = (4 << 5) + 6 + (3 << 10) + (63 << 25) + (1000 << 13);
    } else if (ps_index == 2) {
        APLL_CTRL = (ps_range[ps_index] << 8) + (1 << 16);
        APLL_CFG = (3 << 5) + 10 + (3 << 10) + (63 << 25) + (1000 << 13);
    } else {
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
    //printf("PS switched to clock %f MHz with index %i\n", ps_clk, ps_index);

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
    // get pl_range, if else for pl_clk
    divisor = pl_range[pl_index];
    *pl0 = (1 << 24) // bit 24 enables clock
           | (1 << 16) // bit 23:16 is divisor 1
           | (divisor << 8); // bit 15:0 is clock divisor 0
    munmap(clk_reg, 0x1000);
    (void) close(dh);
}

/* -----------------------------------------------------------------------------
 * SIGIO signal handler
 */

void sigio_signal_handler(int signo) {
    assert(signo == SIGIO);   // Confirm correct signal #
    sigio_signal_count++;
    //printf("sigio_signal_handler called (signo=%d)\n", signo);
    /* -------------------------------------------------------------------------
     * Set global flag
     */
    sigio_signal_processed = 1;
    /* -------------------------------------------------------------------------
 * Take end timestamp for interrupt latency measurement
 */
    (void) gettimeofday(&sigio_signal_timestamp, NULL);
}

int main(int argc, char *argv[]) {
    int count = 0;
    int loop_flag = 1;
    if (argc == 3) {
        count = strtoul(argv[1], 0, 0);
        loop_flag = count;
        loop_count = loop_flag;
        number = strtoul(argv[2], 0, 0) * 4;
    }
    srand(time(0));         // Seed the random number generator
    int latency_0_0[loop_flag];
    int latency_0_1[loop_flag];
    int latency_0_2[loop_flag];
    int latency_1_0[loop_flag];
    int latency_1_1[loop_flag];
    int latency_1_2[loop_flag];
    int latency_2_0[loop_flag];
    int latency_2_1[loop_flag];
    int latency_2_2[loop_flag];

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
    gpio_dev_fd = open(GPIO_DEV_PATH, O_RDWR);
    if (gpio_dev_fd == -1) {
        perror("open() of " GPIO_DEV_PATH " failed");
        return -1;
    }
    /* -------------------------------------------------------------------------
     * Set our process to receive SIGIO signals from the dma device:
     */
    rc = fcntl(gpio_dev_fd, F_SETOWN, getpid());
    if (rc == -1) {
        perror("fcntl() SETOWN failed\n");
        return -1;
    }
    /* -------------------------------------------------------------------------
     * Enable reception of SIGIO signals for the gpio_dev_fd descriptor
     */
    int fd_flags = fcntl(gpio_dev_fd, F_GETFL);
    rc = fcntl(gpio_dev_fd, F_SETFL, fd_flags | O_ASYNC);
    if (rc == -1) {
        perror("fcntl() SETFL failed\n");
        return -1;
    }
    // clear interrupt_out pin
    pm(0xa0050004, 0, 2048 * 2);

    // main loop
    while (loop_flag) {
        if (count > 0) {
            count -= 1;
            loop_flag -= 1;
        }
        for (int ps_i = 0; ps_i < 3; ++ps_i) {
            for (int pl_i = 0; pl_i < 3; ++pl_i) {
                /* ---------------------------------------------------------------------
                 * NOTE: This next section of code must be excuted each cycle to prevent
                 * a race condition between the SIGIO signal handler and sigsuspend()
                 */
                (void) sigfillset(&signal_mask);
                (void) sigfillset(&signal_mask_most);
                (void) sigdelset(&signal_mask_most, SIGIO);
                (void) sigprocmask(SIG_SETMASK, &signal_mask, &signal_mask_old);

                clk_iterate(ps_i, pl_i);
                cdma_sync();
                if (ps_i == 0 && pl_i == 0) {
                    latency_0_0[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 0 && pl_i == 1) {
                    latency_0_1[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 0 && pl_i == 2) {
                    latency_0_2[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 1 && pl_i == 0) {
                    latency_1_0[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 1 && pl_i == 1) {
                    latency_1_1[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 1 && pl_i == 2) {
                    latency_1_2[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 2 && pl_i == 0) {
                    latency_2_0[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 2 && pl_i == 1) {
                    latency_2_1[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                } else if (ps_i == 2 && pl_i == 2) {
                    latency_2_2[loop_flag] = (sigio_signal_timestamp.tv_sec -
                                              start_timestamp.tv_sec) * 1000000 +
                                             (sigio_signal_timestamp.tv_usec -
                                              start_timestamp.tv_usec);
                }
            }
        }
    }
    (void) close(gpio_dev_fd);

    /* -------------------------------------------------------------------------
 * Compute interrupt latency stats:
 */
    unsigned long min_latency;
    unsigned long max_latency;
    double average_latency;
    double std_deviation;
    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_0_0);
    /*
     * Print interrupt latency stats:
     */
    printf("1499 300\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_0_1);
    /*
     * Print interrupt latency stats:
     */
    printf("1499 187.5\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_0_2);
    /*
     * Print interrupt latency stats:
     */
    printf("1499 100\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_1_0);
    /*
     * Print interrupt latency stats:
     */
    printf("999 300\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_1_1);
    /*
     * Print interrupt latency stats:
     */
    printf("999 187.5\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_1_2);
    /*
     * Print interrupt latency stats:
     */
    printf("999 100\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_2_0);
    /*
     * Print interrupt latency stats:
     */
    printf("416.6 300\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_2_1);
    /*
     * Print interrupt latency stats:
     */
    printf("416.6 187.5\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            latency_2_2);
    /*
     * Print interrupt latency stats:
     */
    printf("416.6 100\n");
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "Number of samples:  %d\n"
           "Number of interrupts: %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           loop_count,
           sigio_signal_count);

    return 0;
}
