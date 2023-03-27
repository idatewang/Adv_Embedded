#!/bin/sh

echo "---------------------------"
echo " Fill OCM with all FFFFFs"
echo "---------------------------"
# Fill memory with all fffff's
/bin/fm 0xfffc0000 0xffffffff 100  0 > /dev/null

echo "---------------------------"
echo " Start DMA transfer"
echo "---------------------------"

# Reset the cdma unit
/bin/pm 0xb0000000 0x4
 
# Write the destination address to the CDMA unit
/bin/pm 0xB0000020 0xb0080000

# Write the Source address to the CDMA unit
/bin/pm 0xB0000018 0xfffc0000

# Start the DMA transfer
/bin/pm 0xB0000028 0x100


echo "---------------------------"
echo " Check DMA transfer"
echo "---------------------------"
/usr/bin/pm 0xA0080008 0x00   > /dev/null
/usr/bin/dm 0xA008000c

/usr/bin/pm 0xA0080008 0x4    > /dev/null
/usr/bin/dm 0xA008000c

/usr/bin/pm 0xA0080008 0x8    > /dev/null
/usr/bin/dm 0xA008000c

/usr/bin/pm 0xA0080008 0xc    > /dev/null
/usr/bin/dm 0xA008000c 

# Reset Keccak Unit
/usr/bin/pm 0xA0080000 0x02 > /dev/null
/usr/bin/pm 0xA0080000 0x00 > /dev/null

# Check 16 bytes
/usr/bin/pm 0xA0080014 16 > /dev/null
echo "---------------------------"
echo "Start Keccak Unit"

/usr/bin/pm 0xA0080000 0x01 > /dev/null
/usr/bin/pm 0xA0080000 0x00 > /dev/null


echo "---------------------------"
echo "Expected results: 
0xa0080040 = 0xc2d1e942
0xa0080044 = 0x1945bdac
0xa0080048 = 0x3aa06074
0xa008004c = 0x8d4b7f67
0xa0080050 = 0x7bcb22b5
0xa0080054 = 0xa06be2e3
0xa0080058 = 0xe1cc4a7c
0xa008005c = 0xaa8e8b9e
0xa0080060 = 0xddbb88a7
0xa0080064 = 0x43c530cb
0xa0080068 = 0x58367898
0xa008006c = 0x8f4d5893
0xa0080070 = 0xdc4fb0f5
0xa0080074 = 0xaefa63b2
0xa0080078 = 0xcd06dbf8
0xa008007c = 0xdd1843e5

"

echo "Actual results:"
# Dump memory
/usr/bin/dm 0xA0080040 16  
