# AprunSplit
AprunSplit is a library that intercepts MPI calls and will switch out MPI_COMM_WORLD for a smaller communicator
containing a subset of MPI_COMM_WORLD ranks.


## To install:

```
$ module load cmake
$ mkdir build
$ cmake ..
$ make
$ make install
```

## To run C:
* copy lib/libsplit.so to lustre
* LD_PRELOAD=/path/to/libsplit.so aprun as normal

## To run Fortran:
* copy lib/libsplit.so to lustre
* prepend LD_PRELOAD env variable to point to libfmpich*.so followed by libsplit.so to aprun
* e.g. "/opt/cray/mpt/7.0.4/gni/mpich2-pgi/141/lib/libfmpich_pgi.so:/lustre/atlas/scratch/atj/stf007/libsplit.so"
