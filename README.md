# wraprun
wraprun is a library that intercepts MPI function calls and will switch out MPI_COMM_WORLD for a communicator
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
* LD_PRELOAD=/path/to/libsplit.so wraprun -n 80 ./a.out : -n 160 ./b.out ...

## To run Fortran:
* copy lib/libsplit.so to lustre
* LD_PRELOAD=$MPICH_DIR/lib/libfmpich.so:/path/to/libsplit.so wraprun -n 80 ./a.out : -n 160 ./b.out ...
