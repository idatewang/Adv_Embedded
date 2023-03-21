#!/bin/sh
#
# ADDRESSES:
# CDMA UNIT     0xB0000000
# CDMA BRAM     0xB0080000
# SHA3_BRAM     0xA0080000 
# OCM           0xfffc0000
#
##############################################################

# Reset Keccak Unit
/usr/bin/pm 0xA0080000 0x02
/usr/bin/pm 0xA0080000 0x00


# Set starting address
/usr/bin/pm 0xA0080008 0x00
/usr/bin/pm 0xA0080010 0x61616161

/usr/bin/pm 0xA0080008 0x4
/usr/bin/pm 0xA0080010 0x61616161

/usr/bin/pm 0xA0080008 0x8
/usr/bin/pm 0xA0080010 0x61616161

/usr/bin/pm 0xA0080008 0xc
/usr/bin/pm 0xA0080010 0x61616161

/usr/bin/pm 0xA0080014 16

/usr/bin/pm 0xA0080000 0x01


# Dump memory
/usr/bin/dm 0xA0080040 16



echo "---------------------------"
echo "Expected results
0xb0000020 = 0xedeaaff3
0xb0000024 = 0xf1774ad2
0xb0000028 = 0x88867377
0xb000002c = 0x0c6d6409
0xb0000030 = 0x7e391bc3
0xb0000034 = 0x62d7d6fb
0xb0000038 = 0x34982ddf
0xb000003c = 0x0efd18cb
"


