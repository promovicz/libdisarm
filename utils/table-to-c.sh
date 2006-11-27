#!/bin/bash
while read i; do
    [[ -z $i ]] && echo || (
	mask=$(echo $i | cut -d ' ' -f 1);
	compare=$(echo $i | cut -d ' ' -f 2);
	name=$(echo $i | cut -d ' ' -f 3);
	pattern=$(echo $i | cut -d ' ' -f 4);
	echo "{" 0x$(printf "%08x" $(echo "ibase=2; $mask;" | bc)), \
	    0x$(printf "%08x" $(echo "ibase=2; $compare" | bc)),
	echo -n "  "ARM_INSTR_TYPE_$(echo $name | tr [a-z] [A-Z])", "
	[[ -z $pattern ]] && echo -n ${name}_pattern || \
	    echo -n ${pattern}
	echo " },"
    )
done
echo "{ 0, 0, 0, NULL }"
