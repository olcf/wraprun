# wraprun
`wraprun` is a utility that enables independent execution of multiple MPI
applications under a single `aprun` call.

## To install:
`wraprun` includes a Smithy formula to automate deployment, for centers not
using Smithy the build looks as follow:

```
$ mkdir build
$ cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
$ make
$ make install
```
Inside of `/path/to/install` a `bin` directory will be created containing the
`wraprun` scripts and a `lib` directory will be created containing
`libsplit.so`. The `WRAPRUN_PRELOAD` environment variable must be correctly set
to point to `libsplit.so` and in the case of fortran applications
`libfmpich.so` at runtime.  e.g.
`WRAPRUN_PRELOAD=/path/to/install/lib/libsplit.so:/path/to/mpi_install/lib/libfmpich.so`

On some systems libfmpich has a programming environment specific suffix that
must be taken into account: e.g.
`WRAPRUN_PRELOAD=/path/to/install/lib/libsplit.so:/path/to/mpi_install/lib/libfmpich_pgi.so`

## To run:
Assuming that the module file created by the Smithy formula is used, or a
similar one created, basic running looks like the following examples.

```
$ module load python wraprun
$ wraprun -n 80 ./foo.out : -n 160 ./bar.out ...
```
A maximum of 2048 separate `:` separated task groups is enforced to protect
ALPS stability.

In addition to the standard process placement flags available to aprun the
`--w-cd` flag can be set to change the current working directory for each
executable:
```
$ wraprun -n 80 --w-cd /foo/dir ./foo.out : -n 160 --w-cd /bar/dir ./bar.out ...
```
This is particularly useful for legacy Fortran applications that use hard coded
input and output file names.

Multiple instances of an application can be placed on a node using
comma-separated PES syntax `PES1,PES2,...,PESN` syntax, for instance:
```
$ wraprun -n 2,2,2 ./foo.out : ...
```
would launch 3 two-process instances of foo.out on a single node. 

In this case the number of allocated nodes must be at least equal to the sum of
processes in the comma-separated list of processing elements divided by the
maximum number of processes per node.

This may also be combined with the `--w-cd` flag :
```
$ wraprun -n 2,2,2 --w-cd /foo/dir1,/foo/dir2,/foo/dir3 ./foo.out : ...
```

For non MPI executables a wrapper application, `serial`, is provided. This
wrapper ensures that all executables will run to completion before aprun exits.
To use, place `serial` in front of your application and arguments:
```
$ wraprun -n 1 serial ./foo.out -foo_args : ...
```
### Standard output/error Redirection

The `stdout/stderr` streams for each task are directed to a unique file for
that task since it is generally undesirable for the standard output of all
tasks to be merged into a single stream.  By default, each task writes their
own streams to files in the task's own current working directory named:

```
${JOBNAME}.${JOBID}_w${INSTANCE}.${TASKID}.out
${JOBNAME}.${JOBID}_w${INSTANCE}.${TASKID}.err
```

where `JOBNAME` is the batch job name (value of `$PBS_JOBNAME` for instance),
`JOBID` is the batch job number (or PID of parent shell if `$PBS_JOBID` is
unavailable), `INSTANCE` is the unique wraprun invocation called within the
parent shell, and `TASKID` is the task index among all bundled tasks. The
instance index is required so that multiple concurrent wraprun invocations in a
single batch job do not collide with each other. The task index is fixed in the
order that tasks are passed to wraprun such that for the following invocation:
```
$ wraprun -n 1,2 ./foo.out : -n 3 ./bar.out
```

task '0' is the instance of `foo.out` having 1 PE; task '1' is the 2 PE split of
`foo.out`, and task '2' is the instance of `bar.out`. 

In many cases, it is desirable to change the default standard output/error file names.
The default names can be overridden by supplying a basename path to the group
flag `--w-oe`:

```
$ wraprun -n 1,2 --w-oe name_a ./a.out : \
          -n 1,2 --w-oe name_b1,name_b2 ./b.out : \
          -n 1   --w-oe name_c1,sub/name_c2 ./c.out : \
          -n 1,1 ./d.out : \
          -n 1   ./e.out
```

The names can be either relative or absolute paths to an `aprun`-writable
location.  Relative names are taken from the task's CWD. Wraprun will not
create missing directories. Wraprun will add `.out` and `.err` extensions to
the given path for each stream. 

When ambiguous names are provided as for `a.out` in the above example, wraprun
will add `_w${SPLITID}` to the given path for each task split.  To avoid this, a
specific filename can be given to each task as a comma-separated list for each
group split.  The output of the above example would then be

```
.
├── name_a_w0.err
├── name_a_w0.out
├── name_a_w1.err
├── name_a_w1.out
├── name_b1.err
├── name_b1.out
├── name_b2.err
├── name_b2.out
├── name_c1.err
├── name_c1.out
├── sub/
│   ├── name_c2.err
│   └── name_c2.out
├── nameofbatchjob.123456._w0.6.err
├── nameofbatchjob.123456._w0.6.out
├── nameofbatchjob.123456._w0.7.err
├── nameofbatchjob.123456._w0.7.out
├── nameofbatchjob.123456._w0.8.err
└── nameofbatchjob.123456._w0.8.out
```

**WARNING:** Under no circumstances can standard shell redirection (`>`) be
used to separate the stdout and stderr streams of individual task groups. In
other words, the following is a shell syntax error and will not produce the
desired outcome:

```
# This will fail - redirection operators in the middle of
# a command incantation is a syntax error.
wraprun -n 1 ./a.out > a.log : -n 1 ./b.out > b.log
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
# Add tasks by single strings as would be passed to the commandline interface between `:`'s.
bundle.add_task('-n 2,4,6 --w-cd ./a,./b,./c -cc 2 -ss ./bin_a -cc')
bundle.add_task('-n 3,5,7 ./bin_b input_b')

# Add tasks by passing parameters as keyword arguments.
bundle.add_task(pes=[1,2,3], pes_per_node=5, exe='./bin_c with args', cd=['./aa', './bb','./cc'])

# Launch the job on the compute nodes (starts the `aprun` subprocess).
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
| Task stdout/stderr file basename               | --w-oe   | 'oe'                  | str or [str,...] |
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


### API Examples

Bundle five instances of the same program.

```python
from wraprun import Wraprun

bundle = Wraprun()
for _ in range(5):
    bundle.add_task('-n 1 a.out')

bundle.launch()
```

Loop over many similar input files organized in different directories, with customized stdout/stderr filenames for each task:

```python
from os import environ
from wraprun import Wraprun

dir_template = './run_{run_id}x{ranks:02}'
log_template = 'run_{run_id}x{ranks:02}.log.{pbs_job}'.format(pbs_job=environ.get('PBS_JOBID', 'none'))
ranks_list = [5, 10, 15]
binary_path = '/ccs/proj/stf007/bin/my_executable'
input_template = '{binary} data_{run_id}.dat'

bundle = Wraprun()
for run_id in range(5):
    bundle.add_task(
        pes=ranks_list,
        pes_per_node=5,
        cd=[dir_template.format(run_id=run_id, ranks=ranks) for ranks in ranks_list],
        oe=[log_template.format(run_id=run_id, ranks=ranks) for ranks in ranks_list],
        exe=input_template.format(binary=binary_path, run_id=run_id)
        )
bundle.launch()
```

This is equivalent to the following command line call, but is arguably easier
to read and certainly easier to edit, maintain, and scale to larger numbers of
runs.

```sh
wraprun -n 5,10,15 -N 5 --w-cd './run_0x05,./run_0x10,./run_0x15' --w-oe "run_0x05.log.${PBS_JOBID},run_0x10.log.${PBS_JOBID},run_0x15.log.${PBS_JOBID}" /ccs/proj/stf007/bin/my_executable data_0.dat : \
        -n 5,10,15 -N 5 --w-cd './run_1x05,./run_1x10,./run_1x15' --w-oe "run_1x05.log.${PBS_JOBID},run_1x10.log.${PBS_JOBID},run_1x15.log.${PBS_JOBID}" /ccs/proj/stf007/bin/my_executable data_1.dat : \
        -n 5,10,15 -N 5 --w-cd './run_2x05,./run_2x10,./run_2x15' --w-oe "run_2x05.log.${PBS_JOBID},run_2x10.log.${PBS_JOBID},run_2x15.log.${PBS_JOBID}" /ccs/proj/stf007/bin/my_executable data_2.dat : \
        -n 5,10,15 -N 5 --w-cd './run_3x05,./run_3x10,./run_3x15' --w-oe "run_3x05.log.${PBS_JOBID},run_3x10.log.${PBS_JOBID},run_3x15.log.${PBS_JOBID}" /ccs/proj/stf007/bin/my_executable data_3.dat : \
        -n 5,10,15 -N 5 --w-cd './run_4x05,./run_4x10,./run_4x15' --w-oe "run_4x05.log.${PBS_JOBID},run_4x10.log.${PBS_JOBID},run_4x15.log.${PBS_JOBID}" /ccs/proj/stf007/bin/my_executable data_4.dat
```

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
`wraprun` works by intercepting <i>all</i> MPI function calls that contain an
`MPI_Comm` argument. If an application calls an MPI function, containing an
`MPI_Comm` argument, not included in `src/split.c` the results are undefined.

If any executable is not dynamically linked the results are undefined.

If you spot a bug or missing functionality please submit a pull request!
