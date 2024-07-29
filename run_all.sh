#!/bin/sh

for i in ./easy/*.vipr; do
	./run_one.sh $i 0;
done

for i in ./hard/*.vipr; do
	./run_one.sh $i 0;
done
