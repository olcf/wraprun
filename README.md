# wraprun
`wraprun` is a utility that enables independent execution of applications launched with Cray's `aprun` MPMD mode. Normally applications launched in MPMD mode share a common `MPI_COMM_WORLD`; with wraprun however each application will have a private `MPI_COMM_WORLD`, transparent to the calling application. To facilitate independent application execution additional functionality has been added to the base `aprun` MPMD mode.

## To install:
`wraprun` includes a Smithy formula to automate deployment, for centers not using Smithy the build looks as follow:

```
$ mkdir build
$ cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
$ make
$ make install
```
Inside of `/path/to/install` a `bin` directory will be created containing the `wraprun` scripts and a `lib` directory will be created containing `libsplit.so`.

## To run:
Assuming that the module file created by the Smithy formula is used running is straight forward:

```
$ module load wraprun
$ wraprun -n 80 ./foo.out : -n 160 ./bar.out ...
```
If the provided module file is not used it is expected that the `WRAPRUN_PRELOAD` environment variable is correctly set to point to `libsplit.so` and in the case of fortran applications `libfmpich.so`.

In addition to the standard process placement flags available to aprun the `--w-cd` flag can be set to change the current working directory for each executable:
```
$ wraprun -n 80 --w-cd /foo/dir ./foo.out : -n 160 --w-cd /bar/dir ./bar.out ...
```
This is particularly useful for legacy Fortran codes that use hard coded input and output file names.

Multiple instances of an application can be placed on a node using the `{}` snytax, for instance:
```
$ wraprun -n {2,2,2} ./foo.out : ...
```
would launch 3 two rank instances of foo.out on a single node. This may also be combined with the `--w-cd` flag :
```
$ wraprun -n {2,2,2} --w-cd {/foo/dir1,/foo/dir2,/foo/dir3} ./foo.out : ...
```

For non MPI executables a wrapper application, `serial` is provided, this ensures that all executables will run to completion before aprun exits:
```
$ wraprun -n 1 serial ./foo.out -foo_args : ...
```

By default all `stdout/err` will be directed to the same file, this if often not desirable. Setting the `W_REDIRECT_OUTERR` environment variable will cause each executable to redirect it's output to unique file in it's current working directory:
```
$ export W_REDIRECT_OUTERR=1
$ wraprun -n 1 ./foo.out: ...
```

## Notes
* It is recommended that applications be dynamically linked
	* On Titan this can be accomplished by loading the dynamic-link module before invoking the Cray compile wrappers `CC`,`cc`, `ftn`.
  * The library may be statically linked although this is not fully supported.

* All executables must reside in a compute mode visible filesystem, e.g. Lustre. Executables will not be copied as they normally are.


## Disclaimer
`wraprun` works by intercepting <i>all</i> MPI function calls that contain an `MPI_Comm` argument. If an application calls an MPI function, containing an `MPI_Comm` argument, not included in `src/split.c` the results are undefined.

If any executable is not dynamically linked the results are undefined.

If you spot a bug or missing functionality please submit a pull request!
