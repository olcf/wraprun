# wraprun
`wraprun` is a utility that enables independent execution of applications launched with Cray's `aprun` MPMD mode. Normally applications launched in MPMD mode share a common `MPI_COMM_WORLD`; with wraprun however each application will have a private `MPI_COMM_WORLD`, transparent to the calling application.

## To install:
`wraprun` includes a Smithy formula to automate deployment, for centers not using Smithy the build looks as follow:

```
$ module load cmake
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
If the module file is not used it is expected that the `WRAPRUN_PRELOAD` environment variable is correctly set to point to `libsplit.so` and potentially `libfmpich.so`.

## Requirements
* All executables must be dynamically linked
	** On Titan this can be accomplished by loading the dynamic-link module before invoking the Cray compile wrappers `CC`,`cc`, `ftn`.
* All executables must reside in a compute mode visible filesystem, e.g. lustre. Executables will not be copied as they normally are.

## Disclaimer
`wraprun` works by intercepting <i>all</i> MPI function calls that contain an `MPI_Comm` argument. If an application calls an MPI function not included in `src/split.c` the results are undefined. If you spot a bug or missing functionality please submit a pull request!
