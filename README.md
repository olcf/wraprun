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

## To run:
* module load wraprun
* wraprun -n 80 ./a.out : -n 160 ./b.out ...
