# wraprun
`wraprun` is a utility that enables independent execution of multiple MPI applications under a single `aprun` call.

## To install:
`wraprun` includes a Smithy formula to automate deployment, for centers not using Smithy the build looks as follow:

```
$ mkdir build
$ cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
$ make
$ make install
```
Inside of `/path/to/install` a `bin` directory will be created containing the `wraprun` scripts and a `lib` directory will be created containing `libsplit.so`. The `WRAPRUN_PRELOAD` environment variable must be correctly set to point to `libsplit.so` and in the case of fortran applications `libfmpich.so` at runtime.
e.g. `WRAPRUN_PRELOAD=/path/to/install/lib/libsplit.so:/path/to/mpi_install/lib/libfmpich.so`

On some systems libfmpich has a programming environment specific suffix that must be taken into account:
e.g. `WRAPRUN_PRELOAD=/path/to/install/lib/libsplit.so:/path/to/mpi_install/lib/libfmpich_pgi.so`

## To run:
Assuming that the module file created by the Smithy formula is used, or a similar one created, running looks as such.

```
$ module load wraprun
$ wraprun -n 80 ./foo.out : -n 160 ./bar.out ...
```

In addition to the standard process placement flags available to aprun the `--w-cd` flag can be set to change the current working directory for each executable:
```
$ wraprun -n 80 --w-cd /foo/dir ./foo.out : -n 160 --w-cd /bar/dir ./bar.out ...
```
This is particularly useful for legacy Fortran applications that use hard coded input and output file names.

Multiple instances of an application can be placed on a node using
comma-separated PES syntax `PES1,PES2,...,PESN` syntax, for instance:
```
$ wraprun -n 2,2,2 ./foo.out : ...
```
would launch 3 two-process instances of foo.out on a single node. 

In this case the number of allocated nodes must be at least equal to the sum of processes in the comma-separated list of processing elements divided by the maximum number of processes per node.

This may also be combined with the `--w-cd` flag :
```
$ wraprun -n 2,2,2 --w-cd /foo/dir1,/foo/dir2,/foo/dir3 ./foo.out : ...
```

For non MPI executables a wrapper application, `serial`, is provided. This wrapper ensures that all executables will run to completion before aprun exits. To use place, `serial` in front of your application and arguments:
```
$ wraprun -n 1 serial ./foo.out -foo_args : ...
```

By default all wraprun application's `stdout/err` will be directed to the same file, this if often not desirable. Setting the `--w-roe` flag will cause each executable to redirect its output to a unique file in the current working directory:
```
$ wraprun --w-roe -n 1 ./foo.out: ...
```

## Notes
* It is recommended that applications be dynamically linked
	* On Titan this can be accomplished by loading the dynamic-link module before invoking the Cray compile wrappers `CC`,`cc`, `ftn`.
  * The library may be statically linked although this is not fully supported.

* All executables must reside in a compute node visible filesystem, e.g. Lustre. Executables will not be copied as they normally are.


## Disclaimer
`wraprun` works by intercepting <i>all</i> MPI function calls that contain an `MPI_Comm` argument. If an application calls an MPI function, containing an `MPI_Comm` argument, not included in `src/split.c` the results are undefined.

If any executable is not dynamically linked the results are undefined.

If you spot a bug or missing functionality please submit a pull request!
