#!/bin/bash

if [ "$#" -lt 2 ]; then
	echo "usage: $(basename $0) <vipr_filename> <block_size>"
	exit 1
fi

./vipr_checker $1 $1.smt sat $2

exit 0
