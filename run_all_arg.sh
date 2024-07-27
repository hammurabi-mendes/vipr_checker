#!/bin/sh

for i in $@; do
	./run_one.sh $i 50;
done
