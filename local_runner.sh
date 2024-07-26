#!/bin/sh

if [ "$#" -lt 1 ]; then
	echo "File not provided"
fi

CVC=$<cvc5 bin directory>
DIRNAME=<working directory>

cd $DIRNAME

$CVC $DIRNAME/$1
