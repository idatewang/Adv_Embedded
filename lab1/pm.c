/* -----------------------------------------------------------------------------
 *  pm.c
 *
 *  author: Mark McDermott 
 *  Created: May 13, 2015
 *
 * The routine is used to write to registers in the MX6 SOC
 *
 *  ------------------------------------------------------------------------------ */


#include "stdio.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include "h1kp_opcodes.h"
//#include "mx6_registers.h"
#include <unistd.h>

#define MAP_SIZE 8192UL
#define MAP_MASK (MAP_SIZE - 1)


int pm( unsigned int target_addr, unsigned int value ) 
{

	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	volatile unsigned int *regs, *address ;
	
	if(fd == -1)
	{
		perror("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		return -1;
	}	
		
	regs = (unsigned int *)mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, target_addr & ~MAP_MASK);		

    address = regs + (((target_addr) & MAP_MASK)>>2);    	

	*address = value; 	                    // Perform write command
        	
	int temp = close(fd);                   // Close memory
	if(temp == -1)
	{
		perror("Unable to close /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		return -1;
	}	

	munmap(NULL, MAP_SIZE);                 // Unmap memory
	return 0;                               // Return status

}   // End of routine

