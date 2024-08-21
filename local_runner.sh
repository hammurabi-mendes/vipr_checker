#!/bin/sh

if [ "$#" -lt 1 ]; then
	echo "File not provided"
fi

CVC=<cvc_path>
DIRNAME=<working_directory>

cd $DIRNAME

OUTPUT=$($CVC $1)

rm -f $1

if [ "$OUTPUT" = "sat" ]; then
    exit 1
else
    exit 0
fi