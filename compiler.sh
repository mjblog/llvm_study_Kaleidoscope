#!/bin/sh
workdir=$(cd $(dirname $0); pwd)
TOY_COMPILER=${workdir}/toy_compiler
${TOY_COMPILER} $1
g++ $1.o  -o $1.out -L${workdir} -lcore_support
rm $1.o
