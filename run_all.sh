#!/bin/sh

for i in ./easy/*.vipr; do
	./run_one.sh $i 50;
done

for i in ./hard/*.vipr; do
	./run_one.sh $i 50;
done

#for i in ./hard/*.vipr; do
#	./run_one.sh $i 25;
#done

#for i in ./hard/*.vipr; do
#	./run_one.sh $i 100;
#done

#for i in ./hard/*.vipr; do
#	./run_one.sh $i 12;
#done
