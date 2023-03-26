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
a31c9cbe
df4c5ec3
3dcc888a
ce7a27a1
ea2b6a82
e41fc204
4afbbbf7
e4f8b6ff
96dd7d83
c0ededd5
0385447e
6c9bb4d0
e6542575
ddc165b8
b5fa7245
c98b10d1
"


