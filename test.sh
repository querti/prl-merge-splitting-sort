#!/bin/bash

numbers=$1;
processors=$2;

mpic++ --prefix /usr/local/share/OpenMPI -o mss mss.cpp

dd if=/dev/random bs=1 count=$numbers of=numbers &>/dev/null

mpirun --prefix /usr/local/share/OpenMPI -np $processors mss

#uklid
rm -f mss numbers