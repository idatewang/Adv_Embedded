/*
Test #1: :
1. Load a memory page (i.e., 4K bytes) in the OCM with a 1024 random 32-bit data values
using the Linux srand(time(0)) and rand() routines. Use 0xFFFC_0000 as the starting
address of this page.
2. Transfer the random data in the OCM (0xFFFC_0000) to the BRAM (0xB002_8000) using
the CDMA unit.
3. Measure the number of cycles it took to do CDMA transfer using the Capture-Timer unit as described above
4. Transfer the random data in the BRAM (0xB002_8000) back to the OCM at a different
address (0xFFFC_2000) using the CDMA unit.
5. Measure the number of cycles it took to do CDMA transfer using the Capture-Timer unit as described above.
6. Once the steps above are complete, the OCM data at address 0xFFFC_0000 is compared to the OCM data at address 0xFFFC_2000. This confirms that DMA traffic to/from the OCM & BRAM works.
7. Change the PS and PL clock frequencies and repeat until all 9 combinations have been tested.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

#include "../APIs/pl.h"
#include "../APIs/ps.h"

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)
#define MAP_WORDS (MAP_SIZE / 4)

#define BRAM_MAX_BYTES 8192UL
#define BRAM_MAX_WORDS (BRAM_MAX_BYTES / 4)

#define OCM_MAX_BYTES 16384UL
#define OCM_MAX_WORDS (OCM_MAX_BYTES / 4)

#define NUM_FREQS 3

#define BUFF_SIZE 128

#define DEFAULT 0
#define BRAM_PS 0xA0028000
#define BRAM_DMA 0xB0028000
#define OCM 0xFFFC0000
#define OCM_MOVE 0xFFFC2000
#define CDMA 0xB0000000

#define CAP_TIMER 0xA0050000

//copied cdma defines
#define CDMACR              0x00	// Control register
#define CDMASR              0x04	// Status register
#define CURDESC_PNTR        0x08
#define CURDESC_PNTR_MSB    0x0C
#define TAILDESC_PNTR       0x10
#define TAILDESC_PNTR_MSB   0x14
#define SA                  0x18	// Source Address
#define SA_MSB              0x1C
#define DA                  0x20	// Destination Address
#define DA_MSB              0x24
#define BTT                 0x28

const unsigned int PS_FREQS[NUM_FREQS] = {1499, 1000, 416};
const unsigned int PL_FREQS[NUM_FREQS] = {300, 187, 100};

/* -------------------------------------------------------------------------------
 * One-bit masks for bits 0-31
 */
#define ONE_BIT_MASK(_bit)    (0x00000001 << (_bit))

/* -------------------------------------------------------------------------------
 * Device path name for the CDMA
 */

#define CDMA_DEV_PATH    "/dev/cdma_int"

/* -------------------------------------------------------------------------------
 * File descriptor for timer device
 */

int cdma_dev_fd  = -1;

/* -------------------------------------------------------------------------------
 *      Counter of number of times sigio_signal_handler() has been executed
 */

volatile int sigio_signal_count = 0;

/* -------------------------------------------------------------------------------
 *      Flag to indicate that a SIGIO signal has been processed
 */
static volatile sig_atomic_t sigio_signal_processed = 0;

/* -------------------------------------------------------------------------------
 * Number of interrupt latency measurements to take:
 */

#define NUM_MEASUREMENTS    10000	//TODO

/* -------------------------------------------------------------------------------
 *      Array of interrupt latency measurements (in clock cycles):
 */
unsigned long InterruptLatencies[NUM_MEASUREMENTS];


/***************************  PROTOTYPES ************************************
*
*/

unsigned int dma_set(unsigned int* dma_virtual_address, int offset, unsigned int value) {
    dma_virtual_address[offset>>2] = value;
    return 1;
}

unsigned int dma_get(unsigned int* dma_virtual_address, int offset) {
    return dma_virtual_address[offset>>2];
}

/***************************  CDMA_SYNC **********************************
*
* This is polling loop that waits for the DMA to complete. Need to replace
* interrupt capability
*/

int cdma_sync(unsigned int* dma_virtual_address) {
    unsigned int status = dma_get(dma_virtual_address, CDMASR);
    if( (status & 0x40) != 0)
    {
        unsigned int desc = dma_get(dma_virtual_address, CURDESC_PNTR);
        printf("error address : %X\n", desc);
    }
    while(!(status & 1<<1)){
        status = dma_get(dma_virtual_address, CDMASR);
    }

    return 1;
}

/***************************  MEMDUMP ************************************
*/

void memdump(void* virtual_address, int byte_count) {
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

void transfer(unsigned int *cdma_virtual_address, int length, unsigned int dest, unsigned int source, unsigned int *timer)
{
    dma_set(cdma_virtual_address, DA, dest);    // Write destination address
    dma_set(cdma_virtual_address, SA, source);     // Write source address
    dma_set(cdma_virtual_address, CDMACR, 0x1000);  // Enable interrupts
    timer[1] |= 0x2;
    dma_set(cdma_virtual_address, BTT, length*4);
    //cdma_sync(cdma_virtual_address);
    //dma_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts
}

/* -------------------------------------------------------------------------------
 *      Function prototypes
 */

unsigned long int_sqrt(unsigned long n);

void sigio_signal_handler(int signo);


/* -----------------------------------------------------------------------------
 * Compute  latency stats
 */
void compute_interrupt_latency_stats(
        unsigned long   *min_latency_p,
        unsigned long   *max_latency_p,
        double          *average_latency_p,
        double          *std_deviation_p,
        unsigned int numMeasures)
{
    int i;
    unsigned long   val;
    unsigned long   min = ULONG_MAX;
    unsigned long   max = 0;
    unsigned long   sum = 0;
    unsigned long   sum_squares = 0;

    for (i = 0; i < numMeasures; i ++) {
        val = InterruptLatencies[i];

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

    unsigned long average = (unsigned long)sum / numMeasures;

    unsigned long std_deviation = int_sqrt((sum_squares / numMeasures) -
                                           (average * average));


    *average_latency_p = average;
    *std_deviation_p = std_deviation;
}

/* -------------------------------------------------------------------------------
 * Main routine
 */

int main(int argc, char * argv[]) {

    unsigned int *ocm1, *ocmMove, *cdma, *address, *capTimer ;
    //unsigned int *bramDMA, *bramPS;
    unsigned int offset, value;
    unsigned int lpCnt;
    unsigned int numWords;

    unsigned int plClkSpd;
    unsigned int psClkSpd;

    unsigned int i;
    volatile int rc;

    /* --------------------------------------------------------------------------
    *      Register signal handler for SIGIO signal:
    */

    struct sigaction sig_action;

    memset(&sig_action, 0, sizeof sig_action);
    sig_action.sa_handler = sigio_signal_handler;

    /* --------------------------------------------------------------------------
    *      Block all signals while our signal handler is executing:
    */
    (void)sigfillset(&sig_action.sa_mask);

    rc = sigaction(SIGIO, &sig_action, NULL);

    if (rc == -1) {
        perror("sigaction() failed");
        return -1;
    }

    /* -------------------------------------------------------------------------
    *      Open the device file
    */

    cdma_dev_fd = open(CDMA_DEV_PATH, O_RDWR);

    if(cdma_dev_fd == -1)    {
        perror("open() of " CDMA_DEV_PATH " failed");
        return -1;
    }

    /* -------------------------------------------------------------------------
    * Set our process to receive SIGIO signals from the GPIO device:
    */

    rc = fcntl(cdma_dev_fd, F_SETOWN, getpid());

    if (rc == -1) {
        perror("fcntl() SETOWN failed\n");
        return -1;
    }

    /* -------------------------------------------------------------------------
    * Enable reception of SIGIO signals for the gpio_dev_fd descriptor
    */

    int fd_flags = fcntl(cdma_dev_fd, F_GETFL);
    rc = fcntl(cdma_dev_fd, F_SETFL, fd_flags | O_ASYNC);

    if (rc == -1) {
        perror("fcntl() SETFL failed\n");
        return -1;
    }

    sigset_t signal_mask, signal_mask_old, signal_mask_most; //???


// --------------------------- Lab1
    int fd = open("/dev/mem", O_RDWR|O_SYNC);

    if(fd == -1) {
        printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        return -1;
    }

    if(argc > 1) {
        lpCnt = atoi(argv[1]); //must have finite loops
    }else {
        printf("enter finite number of loops (<10,000) for first argument\n");
        return -1;
    }

    if(argc > 2) {
        numWords = atoi(argv[2]);
    }else {
        printf("enter number of words for second argument\n");
        return -1;
    }


    if(argc > 3) {
        psClkSpd = atoi(argv[3]);
    }else {
        printf("enter ps clk freq index (0 to 2) for third argument\n");
        return -1;
    }

    if(argc > 4) {
        plClkSpd = atoi(argv[4]);
    }else {
        printf("enter pl clk freq index (0 to 2) for fourth argument\n");
        return -1;
    }

    value = 0;
    offset = 0;

    if (numWords > (MAP_SIZE / 4))  {
        numWords = MAP_SIZE / 4;
        //printf("Setting max words to 0x7ff\n");
    }


    ocm1 = mmap(NULL,
                MAP_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                fd,
                OCM);
    ocmMove = mmap(NULL,
                   MAP_SIZE,
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED,
                   fd,
                   OCM_MOVE);
/*	bramPS = mmap(NULL,
	                            MAP_SIZE,
	                            PROT_READ|PROT_WRITE,
	                            MAP_SHARED,
	                            fd,
	                            BRAM_PS); */
    cdma = mmap(NULL,
                MAP_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                fd,
                CDMA);

    capTimer = mmap(NULL,
                    MAP_SIZE,
                    PROT_READ|PROT_WRITE,
                    MAP_SHARED,
                    fd,
                    CAP_TIMER);

    srand(time(0));         // Seed the ramdom number generator
    /* ---------------------------------------------------------------
    *   Main loop
    */

    unsigned int words;

    SetPSClk(PS_FREQS[psClkSpd]);
    SetPLClk(PL_FREQS[plClkSpd]);

    for(i = 0; i < lpCnt; i++) {

        offset = 0;

        // RESET DMA
        dma_set(cdma, CDMACR, 0x0004);

        //OCM random data step
        for(words = 0; words < numWords; words++) {
            value = rand();                 // Write random data

            address = ocm1 + (((OCM + offset) & MAP_MASK)>>2);
            *address = value; 			    // perform write command

            offset  = (offset + 4) % MAP_SIZE;	// WORD alligned, 4096 bytes per page
        }


        //printf("Source memory block:      "); memdump(ocm1, numWords * 4);
        //printf("Destination memory block: "); memdump(bramPS, numWords * 4);


        /* ---------------------------------------------------------------------
        * Reset sigio_signal_processed flag:
        */
        sigio_signal_processed = 0;
        /* ---------------------------------------------------------------------
        * NOTE: This next section of code must be excuted each cycle to prevent
        * a race condition between the SIGIO signal handler and sigsuspend()
        */
        (void)sigfillset(&signal_mask);
        (void)sigfillset(&signal_mask_most);
        (void)sigdelset(&signal_mask_most, SIGIO);
        (void)sigprocmask(SIG_SETMASK,&signal_mask, &signal_mask_old);


        //sleep(1);

//OCM to BRAM step
//printf("OCM to BRAM transfer start\n");
        transfer(cdma, numWords, BRAM_DMA, OCM, capTimer);	//moving whole thing because hash check
//printf("OCM to BRAM transfer fin\n");

        /* ---------------------------------------------------------------------
        * Wait for SIGIO signal handler to be executed.
        */
        if (sigio_signal_processed == 0) {
            rc = sigsuspend(&signal_mask_most);

            /* Confirm we are coming out of suspend mode correcly */
            assert(rc == -1 && errno == EINTR && sigio_signal_processed);
        }

        (void)sigprocmask(SIG_SETMASK, &signal_mask_old, NULL);

        assert(sigio_signal_count == (2*i + 1));   // Critical assertion!!
        dma_set(cdma, CDMACR, 0x0000);  // Disable interrupts

        InterruptLatencies[i] = capTimer[2];

        // RESET DMA
        dma_set(cdma, CDMACR, 0x0004);

        address = capTimer + (((CAP_TIMER + 4) & MAP_MASK)>>2);
        *address &= ~0x2;	//disabling/resetting timer

//BRAM to OCM step

        /* ---------------------------------------------------------------------
        * Reset sigio_signal_processed flag:
        */
        sigio_signal_processed = 0;
        /* ---------------------------------------------------------------------
        * NOTE: This next section of code must be excuted each cycle to prevent
        * a race condition between the SIGIO signal handler and sigsuspend()
        */
        (void)sigfillset(&signal_mask);
        (void)sigfillset(&signal_mask_most);
        (void)sigdelset(&signal_mask_most, SIGIO);
        (void)sigprocmask(SIG_SETMASK,&signal_mask, &signal_mask_old);

//printf("BRAM to OCM transfer start\n");
        transfer(cdma, numWords, OCM_MOVE, BRAM_DMA, capTimer);
//printf("BRAM to OCM transfer fin\n");

        /* ---------------------------------------------------------------------
        * Wait for SIGIO signal handler to be executed.
        */
        if (sigio_signal_processed == 0) {
            rc = sigsuspend(&signal_mask_most);

            /* Confirm we are coming out of suspend mode correcly */
            assert(rc == -1 && errno == EINTR && sigio_signal_processed);
        }

        (void)sigprocmask(SIG_SETMASK, &signal_mask_old, NULL);

        assert(sigio_signal_count == (2*i + 2));   // Critical assertion!!

        dma_set(cdma, CDMACR, 0x0000);  // Disable interrupts

        //get data before disabling/resetting counter
        InterruptLatencies[i] += capTimer[2];
//printf("cap timer reg value %x\n", capTimer[2]);


        address = capTimer + (((CAP_TIMER + 4) & MAP_MASK)>>2);
        *address &= ~0x2;	//disabling/resetting timer
//printf("finished transfers\n");
        //tester
        for(words = 0; words <numWords; words++) {
            if(ocm1[words] != ocmMove[words]) {
                printf("Original OCM result: 0x%.8x and Moved OCM result: 0x%.8x\n", ocm1[words], ocmMove[words]);
                printf("test failed! D:\n");

                int temp = close(fd);

                if(temp == -1)
                {
                    printf("Unable to close /dev/mem.  Ensure it exists (major=1, minor=1)\n");
                    return -1;
                }

                munmap(NULL, MAP_SIZE);
                return -1;

            }
        }

    }
    (void)close(cdma_dev_fd);

    //sha256 hashing
    FILE *fptrOrig;
    FILE *fptrTrans;
    char *orig;
    char *trans;

    //write into files to get sha hash
    fptrOrig = fopen("origOCM.txt","w");
    fptrTrans = fopen("transferredOCM.txt","w");

    for(i = 0; i < MAP_WORDS; i++) {
        fprintf(fptrOrig,"0x%.8x",ocm1[i]);
        fprintf(fptrTrans,"0x%.8x",ocmMove[i]);
    }


    fclose(fptrOrig);
    fclose(fptrTrans);

    //get and compare sha hases
    char *cmd = "sha256sum origOCM.txt";

    char buf[BUFF_SIZE];
    FILE *fp;

    if ((fp = popen(cmd, "r")) == NULL) {
        printf("Error opening pipe!\n");
        return -1;
    }

    //system("sha256sum origOCM.txt");

    fgets(buf, BUFF_SIZE, fp);

    //printf("OUTPUT: %s", buf);

    orig = strtok(buf, " ");

    //printf("OUTPUT: %s", orig);

    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
        return -1;
    }

    cmd = "sha256sum transferredOCM.txt";

    if ((fp = popen(cmd, "r")) == NULL) {
        printf("Error opening pipe!\n");
        return -1;
    }

    //system("sha256sum transferredOCM.txt"); //sufficient?

    fgets(buf, BUFF_SIZE, fp);

    //printf("OUTPUT: %s", buf);

    trans = strtok(buf, " ");

    //printf("OUTPUT: %s", trans);

    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
        return -1;
    }

    if(strcmp(orig, trans) != 0) {
        printf("sha256 hash check failed!\n");
        return -1;
    }else {
        //printf("sha256 hash check passed!\n");
    }
/*
printf("BRAM to OCM values: \n");
for(i = 0; i < lpCnt; i++) {
	printf("%lu , ", OCM_to_BRAM[i]);
}
printf("\nOCM to BRAM values: \n");
for(i = 0; i < lpCnt; i++) {
	printf("%lu , ", BRAM_to_OCM[i]);
}
printf("\n");
*/

    unsigned long   min_latency;
    unsigned long   max_latency;
    double          average_latency;
    double          std_deviation;

    compute_interrupt_latency_stats(
            &min_latency,
            &max_latency,
            &average_latency,
            &std_deviation,
            lpCnt);

    printf("***************************************************************\n");
    printf("Test status --- %d Loops and %d 32-bit words for PS clock %d and PL clock %d\n", lpCnt, numWords, PS_FREQS[psClkSpd], PL_FREQS[plClkSpd]);
    /*
   * Print interrupt latency stats:
   */
    printf("Minimum Latency:    %lu\n"
           "Maximum Latency:    %lu\n"
           "Average Latency:    %f\n"
           "Standard Deviation: %f\n"
           "\nNumber of samples:  %d\n",
           min_latency,
           max_latency,
           average_latency,
           std_deviation,
           lpCnt);

    printf("Number of interrupts detected = %d\n", sigio_signal_count);

    //print interrupt
    cmd = "more /proc/interrupts | grep cdma";

    if ((fp = popen(cmd, "r")) == NULL) {
        printf("Error opening pipe!\n");
        return -1;
    }

    fgets(buf, BUFF_SIZE, fp);
//printf("buffer: %s\n", buf);

    const char delim[2] = " ";
    char *token;
    token = strtok(buf, delim);
    printf("Interrupt %s ", token);
    token = strtok(NULL, delim);
    printf("%s ", token);
    token = strtok(NULL, delim);	//jank
    token = strtok(NULL, delim);
    token = strtok(NULL, delim);
    token = strtok(NULL, delim);
    printf("%s ", token);
    token = strtok(NULL, delim);
    printf("%s ", token);
    token = strtok(NULL, delim);
    printf("%s ", token);
    token = strtok(NULL, delim);
    printf("%s\n", token);



    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
        return -1;
    }


    printf("***************************************************************\n");


    /* ---------------------------------------------------------------
    *   Clenup and Exit
    */

    int temp = close(fd);

    if(temp == -1)
    {
        printf("Unable to close /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        return -1;
    }

    munmap(NULL, MAP_SIZE);

    return 0;



}

/* -----------------------------------------------------------------------------
 * SIGIO signal handler
 */

void sigio_signal_handler(int signo) {

    assert(signo == SIGIO);   // Confirm correct signal #
    sigio_signal_count ++;

    //printf("sigio_signal_handler called (signo=%d)\n", signo);

    /* -------------------------------------------------------------------------
     * Set global flag
     */

    sigio_signal_processed = 1;
}

/* ---------------------------------------------------------------
* sqrt routine
*/

unsigned long int_sqrt(unsigned long n) {
    unsigned long root = 0;
    unsigned long bit;
    unsigned long trial;

    bit = (n >= 0x10000) ? 1<<30 : 1<<14;
    do
    {
        trial = root+bit;
        if (n >= trial)
        {
            n -= trial;
            root = trial+bit;
        }
        root >>= 1;
        bit >>= 2;
    } while (bit);
    return root;
}





//
// Created by Steve Wang on 3/1/23.
//
