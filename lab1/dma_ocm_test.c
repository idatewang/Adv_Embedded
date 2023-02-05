



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>

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
#define CURDESC_PNTR_MSB    0x0C
#define TAILDESC_PNTR       0x10
#define TAILDESC_PNTR_MSB   0x14
#define SA                  0x18        // Source Address
#define SA_MSB              0x1C
#define DA                  0x20        // Destination Address
#define DA_MSB              0x24
#define BTT                 0x28


/*************************** DMA_SET ************************************
*   
*/

unsigned int dma_set(unsigned int* dma_virtual_address, int offset, unsigned int value) {
    dma_virtual_address[offset>>2] = value;
}

/*************************** DMA_GET ************************************
*   
*/

unsigned int dma_get(unsigned int* dma_virtual_address, int offset) {
    return dma_virtual_address[offset>>2];
}

/***************************  CDMA_SYNC **********************************
*   
* This is polling loop that waits for the DMA to complete. Need to replace
* with interrupt capability
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

void transfer(unsigned int *cdma_virtual_address, int length)
{
    dma_set(cdma_virtual_address, DA, BRAM_CDMA);   // Write destination address
    dma_set(cdma_virtual_address, SA, OCM);         // Write source address
    dma_set(cdma_virtual_address, CDMACR, 0x1000);  // Enable interrupts
    dma_set(cdma_virtual_address, BTT, length*4);
    cdma_sync(cdma_virtual_address);
    dma_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts
}

/**************************************************************************
                                MAIN
**************************************************************************/

int main() {
    
    // Open /dev/mem which represents the whole physical memory
    int dh = open("/dev/mem", O_RDWR | O_SYNC); 
    if(dh == -1)
	{
		printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		printf("Must be root to run this routine.\n");
		return -1;
	}
		
    uint32_t* cdma_virtual_address = mmap(NULL, 
                                          4096, 
                                          PROT_READ | PROT_WRITE, 
                                          MAP_SHARED, 
                                          dh, 
                                          CDMA); // Memory map AXI Lite register block
    //printf("cdma_virtual_address = 0x%.8x\n", cdma_virtual_address);                                     
    uint32_t* BRAM_virtual_address = mmap(NULL, 
                                          4096, 
                                          PROT_READ | PROT_WRITE, 
                                          MAP_SHARED, 
                                          dh, 
                                          BRAM_PS); // Memory map AXI Lite register block
    //printf("BRAM_virtual_address = 0x%.8x\n", BRAM_virtual_address);                                     
    // Setup data to be transferred
    uint32_t c[20] = {  0x2caf3444,
                        0x1ab0eab6,
                        0x11ccbda2,
                        0x81991ac6,
                        0x11110a03,
                        0x2499aef8,
                        0x55aa55aa,
                        0x44556677,
                        0xaaacccd6,
                        0x00000008,
                        0x10000003,
                        0x00911a42,
                        0x1ab0eab6,
                        0x11ffbda2,
                        0x8199aac6,
                        0x1aa10a03,
                        0x24944ef8,
                        0x553355aa,
                        0x12345678,
                        0x000cccd6};  // 
    uint32_t c_t[20];
    
    //printf("Starting memory allocation section\n");
    uint32_t* ocm = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, dh, OCM);
    
    printf("OCM virtual address = 0x%.8x\n", ocm);
    
    // Setup data in OCM to be transferred to the BRAM
    for(int i=0; i<20; i++)
        ocm[i] = c[i];
    
    // RESET DMA
    dma_set(cdma_virtual_address, CDMACR, 0x0004);
    
    //printf("Source memory block:      "); memdump(ocm, 32);    
    //printf("Destination memory block: "); memdump(BRAM_virtual_address, 32);

    transfer(cdma_virtual_address, 20);
    //printf("DMA Registers:            "); memdump(cdma_virtual_address, 32);
    
    for(int i=0; i<20; i++)
    {
        if(BRAM_virtual_address[i] != c[i])
        {
            printf("RAM result: 0x%.8x and c result is 0x%.8x  element %d\n", 
                    BRAM_virtual_address[i], c[i], i);
            printf("test failed!!\n"); 
            
            munmap(ocm,65536);
            munmap(cdma_virtual_address,4096);
            munmap(BRAM_virtual_address,4096);
            return -1;
        }
    }
    printf("test passed!!\n");
    munmap(ocm,65536);
    munmap(cdma_virtual_address,4096);
    munmap(BRAM_virtual_address,4096);
    return 0;
}
