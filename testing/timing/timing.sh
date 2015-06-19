#!/bin/bash -l
PBS_NUM_NODES=2

PROG=$RUNDIR/a.out

cmd="wraprun-env --w-debug "
arg="-n 16 -N 16 $PROG"
sep=" : "
for i in `seq 1 $(($PBS_NUM_NODES-1))`; do
  cmd+=$arg
  cmd+=$sep
done
cmd+=$arg

eval $cmd 
