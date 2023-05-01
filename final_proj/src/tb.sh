#!/bin/sh

# clear data.txt file
>data.txt

# deassert Timer_Enable and other registers
pm 0xa0070008 0x0 > /dev/null
pm 0xa0070018 0x0 > /dev/null
pm 0xa0070020 0x0 > /dev/null
pm 0xa0070028 0x0 > /dev/null
pm 0xa007002c 0x0 > /dev/null
pm 0xa007003c 0x0 > /dev/null

# check there are at least two arguments, $1 is lower bound address and $2 is upper bound
if [ $# -lt 2 ]; then
	echo "Less than two arguments"
	exit 1
fi
low="$1"
low_hex=$(printf "%d" "$low")
up="$2"
up_hex=$(printf "%d" "$up")

# assert user input address and Timer_Enable (global_enable)
pm 0xa0070000 "$1" > /dev/null
pm 0xa0070004 "$2" > /dev/null
pm 0xa0070008 0x1 > /dev/null

# look for the clock when the first transaction's address got set and don't assume the data transfer starts right away
start=0
transaction_idx=0
curr_clk_hex=0
last=0
while true; do
	pm 0xa0070018 "$transaction_idx" > /dev/null
	address=$(dm 0xa0070028 | grep -o "=.*" | grep -o "0x.*")
	address_hex=$(printf "%d" "$address")
	if [ "$address_hex" -eq 0 ]; then
		echo "$curr_clk_hex 0" >> data.txt
		curr_clk=$(dm 0xa0070024 | grep -o "=.*" | grep -o "0x.*")
		curr_clk_hex=$(printf "%d" "$curr_clk")
		continue
	else
		break
	fi
done

# skip the first transaction since data transfer for first transaction can not be determined. Look for the start of the second transaction by find the first wlast being turned from 1 to 0

transaction_idx=1

while true; do
	if [ "$last" -eq 1 ]; then
		break
	else	
		pm 0xa0070020 "$curr_clk_hex" > /dev/null
		signals=$(dm 0xA007003C | grep -o "=.*" | grep -o "0x.*")
		signals_hex=$(printf "%d" "$signals")
		last=$((signals_hex && 0x100))
		curr_clk_hex=$((curr_clk_hex + 1))
	fi
done

# check burst size at current clock for the current transaction and write the burst size number of 1s if the address is within range and 0s if the address is not within range

while true; do
	pm 0xa0070018 "$transaction_idx" > /dev/null
	signals=$(dm 0xa007002c | grep -o "=.*" | grep -o "0x.*")
	signals_hex=$(printf "%d" "$signals")
	length=$((signals_hex && 0xFF00))
	address=$(dm 0xa0070028 | grep -o "=.*" | grep -o "0x.*")
	address_hex=$(printf "%d" "$address")
	if [ "$address_hex" -ge "$low_hex" ] && [ "$address_hex" -le "$up_hex" ]; then
		for i in $(seq $start $length); do
			echo "$curr_clk_hex 1" >> data.txt
			curr_clk_hex=$((curr_clk_hex + 1))
		done
	else
		for i in $(seq $start $length); do
			echo "$curr_clk_hex 0" >> data.txt
			curr_clk_hex=$((curr_clk_hex + 1))
		done
	fi
	transaction_idx=$((transaction_idx + 1))
done


# deassert Timer_Enable (not reached because while true loop)
pm 0xA0070008 0x0 > /dev/null
