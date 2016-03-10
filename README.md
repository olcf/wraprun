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
Assuming that the module file created by the Smithy formula is used, or a similar one created, basic running looks like the following examples.

```
$ module load python wraprun
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

For non MPI executables a wrapper application, `serial`, is provided. This wrapper ensures that all executables will run to completion before aprun exits. To use, place `serial` in front of your application and arguments:
```
$ wraprun -n 1 serial ./foo.out -foo_args : ...
```

The `stdout/err` for each task is directed to it's own unique file in the
current working directory. In early releases, this effect was accomplished
with the `--w-roe` flag. However, manually specifying this flag has not been
required since v0.1.11. 
```
$ wraprun --w-roe -n 1 ./foo.out: ...
```

## Python API

Wraprun version 0.2.1 introduces a minimal API that can be used to bundle and
launch tasks programatically via python scripts.

To use the API, load the wraprun environment module and import the `wraprun`
python module into your script:

```
#!/usr/bin/env python
import wraprun

bundle = wraprun.Wraprun()
bundle.add_task('-n 2,4,6 --w-cd ./a,./b,./c -cc 2 -ss ./bin_a -cc')
bundle.add_task('-n 3,5,7 ./bin_b input_b')
bundle.add_task(pes=[1,2,3], pes_per_node=5, exe='./bin_c with args', cd=['./aa', './bb','./cc'])
bundle.launch()
```

The `Wraprun` constructor accepts keyword arguments for global options. The two
principle options are `conf` for passing a configuration file path (see below)
or running in debug mode which does not launch a job.

```
wraprun.Wraprun(conf='/path/to/conf.yml')
wraprun.Wraprun(debug=True)
```

The `add_task` method adds a new group to the bundle. It accepts either a string
as would be passed via the command-line utility or keyword arguments for each
group option.


| Parameter                                      | CLI Flag | API keyword           | API format       |
| ----------------------------------------------:|:--------:|:---------------------:|:---------------- |
| Task working directory                         | --w-cd   | 'cd'                  | str or [str,...] |
| Number of processing elements (PEs). REQUIRED  | -n       | 'pes'                 | int or [int,...] |
| Host architecture                              | -a       | 'arch'                | str              |
| CPU list                                       | -cc      | 'cpu_list'            | str              |
| CPU placement file                             | -cp      | 'cpu_placement'       | str              |
| Process affinity depth                         | -d       | 'depth'               | int              |
| CPUs per CU                                    | -j       | 'cpus_per_cu'         | int              |
| Node list                                      | -L       | 'node_list'           | str              |
| PEs per node                                   | -N       | 'pes_per_node'        | int              |
| PEs per NUMA node                              | -S       | 'pes_per_numa_node'   | int              |
| NUMA node list                                 | -sl      | 'numa_node_list'      | int              |
| Number of NUMA nodes per node                  | -sn      | 'numa_nodes_per_node' | int              |
| Use strict memory containment                  | -ss      | 'strict_memory'       | bool             |
| Executable and argument string. REQUIRED       |          | 'exe'                 | str              |


The `launch` method launches an aprun subprocess call for the added tasks.

## Configuration Files

Both the command line tool and the API can accept a YAML format configuration
file for specifing tasks. See the command line utility help documentation
`wraprun --help` for CLI usage.

See the testing/example_config.yaml file for format information.

## Notes
* It is recommended that applications be dynamically linked.
	* On Titan this can be accomplished by loading the dynamic-link module before invoking the Cray compile wrappers `CC`,`cc`, `ftn`.
  * The library may be statically linked although this is not fully supported.

* All executables must reside in a compute node visible filesystem, e.g. Lustre. Executables will not be copied as they normally are.


## Disclaimer
`wraprun` works by intercepting <i>all</i> MPI function calls that contain an `MPI_Comm` argument. If an application calls an MPI function, containing an `MPI_Comm` argument, not included in `src/split.c` the results are undefined.

If any executable is not dynamically linked the results are undefined.

If you spot a bug or missing functionality please submit a pull request!
