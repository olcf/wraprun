"""
This module provides the primary user interface to wraprun.

The following classes are defined:
    WraprunError - for managing wraprun exceptions.
    Wraprun      - the primary wraprun API object.
"""

from __future__ import print_function
import subprocess
import sys
import os
import tempfile
import yaml

from .options import GROUP_OPTIONS, GLOBAL_OPTIONS
from .parsers import (TaskParser,
                      create_cli_parser,
                      ArgumentParserError,
                      parse_globals)
from .task import TaskGroup


class WraprunError(Exception):
    '''Class for managing wraprun-specific errors.'''
    pass


class Wraprun(object):
    """Job bundler for aprun.

    This object provides an interface for constructing and launching a bundle
    of tasks sharing an MPI communicator via aprun.

    Usage:
        bundle = Wraprun(...)
        bundle.add_task(...)
        bundle.launch()
    """
    def __init__(self, argv=None, **kwargs):
        """API constructor.

        Usage:
         Wraprun(argv=['-n', '3', ...])
         where argv is a list of CLI argument strings similar to 'sys.argv'.

         OR

         Wraprun(**kwargs)
         where kwargs excludes 'argv'.

        Args and kwargs:
          argv (list(str)): CLI argument strings like 'sys.argv'
              Cannot be used with other keyword args.
          conf (str): Path to YAML configuration file.
          debug (bool): Enables debugging output and does not launch aprun.
        """
        # Set null defaults.
        self._options = {}
        self._task_groups = []
        self._env = None
        self._tmpfile = None
        self._rank_and_color = {'rank': 0, 'color': 0}

        self._parser = TaskParser()

        # This funtionality might be better merged into TaskParser
        if argv and not kwargs:
            cli_parser = create_cli_parser()
            has_cli_tasks = True
            try:
                first_group_marker = argv.index(':')
            except ValueError:
                # Either one or no groups. Better catch any CLI errors.
                cli_parser.throw_errors = True
                first_group_marker = len(argv)
            try:
                # Help messages will print here, if they are invoked.
                known_args = cli_parser.parse_args(argv[1:first_group_marker])
            except ArgumentParserError as err:
                if err.args[0] == 'argument -n is required':
                    # No CLI groups are present:
                    has_cli_tasks = False
                    known_args, _ = parse_globals(argv[1:])
                    if known_args.conf is None:
                        # Must have a conf file to go on from here!
                        print('Please provide a config file or a task:\n')
                        cli_parser.parse_args(['--help'])
                else:
                    # An unexpected error occurred:
                    raise
            self._options.update(GLOBAL_OPTIONS.all_args(known_args))
            self._load_conf(kwargs)
            if has_cli_tasks:
                self.add_task(**(GROUP_OPTIONS.all_args(known_args)))
                for string in ' '.join(
                        argv[first_group_marker + 1:]).split(':'):
                    if string != '':
                        self.add_task(string=string)
        elif not argv:
            self._load_conf(kwargs)
        else:
            raise WraprunError("Use either CLI or API, not both.")

    def __repr__(self):
        """Wraprun representation."""
        return "Wraprun(ranks={rank}, colors={color})".format(
            **self._rank_and_color)

    def _load_conf(self, kwargs):
        """Sets API global options and tasks from the
        configuration file passed to the class constructor.
        """
        if kwargs and not self._options.get('conf'):
            self._options['conf'] = kwargs.pop('conf', None)

        valid_names = GLOBAL_OPTIONS.wraprun.names
        config_file = self._options.get('conf')
        if config_file is not None:
            with open(config_file, 'r') as stream:
                # read configuration file
                config = yaml.load(stream)
                # populate options
                if config['options'] is not None:
                    for k, arg in config['options'].items():
                        if k in valid_names:
                            self._options[k] = kwargs.get(k, arg)
                # populate tasks
                for group in config['groups']:
                    self.add_task(**group)
        else:
            for k, arg in kwargs.items():
                if k in valid_names:
                    self._options[k] = arg

    def add_task(self, string=None, **kwargs):
        """Add a task spec.

        Takes either a string of CLI arguments or keyword arguments
        corresponding to the of wraprun.GROUP_OPTIONS.
        If 'string' is present, it's contents override any conflicts with
        keyword arguments.

        EXAMPLES:
           add_task('-n 1,2,3 --w-cd ./a,./b,./c ./exe exe_arg')
           add_task(pes=[1,2,3], cd=['./a','./b','./c'], exe='./exe exe_arg')

        kwargs [kwarg (type): Description]:
           cd (str or [str,...]): Task working directory
           oe (str or [str,...]): Task stdout/stderr file basename
           pes (int or [int,..]): Number of processing elements (PEs). REQUIRED
           arch (int): Host architecture
           cpu_list (int): CPU list
           cpu_placement (int): CPU placement file
           depth (int): Process affinity depth
           cpus_per_cu (int): CPUs per CU
           node_list (int): Node list
           pes_per_node (int): PEs per node
           pes_per_numa_node (int): PEs per NUMA node
           numa_node_list (int): NUMA node list
           numa_nodes_per_node (int): Number of NUMA nodes per node
           strict_memory (bool): Use strict memory containment
           exe (str): Executable and argument string. REQUIRED
        """
        kwargs.update(
            {"first_" + k: v for k, v in self._rank_and_color.items()})
        self._parser.update(kwargs, string)
        if hasattr(kwargs['exe'], 'split'):
            kwargs['exe'] = kwargs['exe'].split()
        task_group = TaskGroup(**kwargs)
        self._rank_and_color = {
            k: v + 1 for k, v in task_group.last_rank_and_color().items()}
        self._task_groups.append(task_group)
        if len(self._task_groups) > 2048:
            raise WraprunError(
                'Too many task groups (> 2048) in bundle: '
                'Aborting to protect ALPS stability.')
        self._update_file(task_group)

    def _debug_mode(self):
        """Return True if debugging option is set."""
        return self._options.get('debug')

    @property
    def _file(self):
        """Return the File object for storing the wrapper rank parameters."""
        if self._tmpfile is None or self._tmpfile.file.closed:
            dir_path = self._options.get('scratch_dir', None)
            if dir_path is not None:
                try:
                    os.makedirs(dir_path, 700)
                except OSError as err:
                    if err.errno is not 17:
                        raise
            else:
                dir_path = os.getcwd()
            self._tmpfile = tempfile.NamedTemporaryFile(
                'a+', prefix='wraprun_', suffix='.tmp', dir=dir_path,
                delete=True)
            # delete=not self._debug_mode())
        return self._tmpfile

    def _update_file(self, task_group):
        """Write rank runtime parameters of task_group to file."""
        tmpfile = self._file.file
        for rank in task_group.ranks:
            tmpfile.write(rank.string() + "\n")
        tmpfile.flush()

    @property
    def env(self):
        """Return the dictionary of wraprun runtime environment variables."""
        try:
            if self._env is None:
                self._env = dict()
                if not self._options.get('no_ld_preload', False):
                    self._env['LD_PRELOAD'] = os.environ['WRAPRUN_PRELOAD']
                self._env['WRAPRUN_FILE'] = self._file.name
                self._env['W_REDIRECT_OUTERR'] = '1'
                self._env['W_IGNORE_SEGV'] = '1'
                self._env['W_UNSET_PRELOAD'] = '1'
            return self._env
        except KeyError as error:
            self._env = None
            raise WraprunError(
                'Missing {v} environment variable'.format(v=error))

    def _aprun_arglist(self):
        """Return a list of the global CLI strings to pass to aprun."""
        arglist = []
        for k, value in self._options.items():
            if value is None:
                continue
            option = GLOBAL_OPTIONS.aprun.get(k, None)
            if option is None:
                continue
            arglist.extend(option.format(value))
        return arglist

    def _task_arglist(self):
        """Return a list of the task CLI strings to pass to aprun."""
        args = []
        for i, task_group in enumerate(self._task_groups):
            if i > 0:
                args.append(':')
            args.extend(task_group.cli_args())
        return args

    def _subprocess_args(self):
        """Return a list of CLI strings needed to invoke aprun."""
        return ['aprun'] + self._aprun_arglist() + self._task_arglist()

    def launch(self):
        """Launch an aprun subprocess with all bundled tasks."""
        os.environ.update(self.env)
        # Last chance to update the log.
        sys.stdout.flush()
        if not self._debug_mode():
            aprun = subprocess.Popen(
                self._subprocess_args(),
                env=os.environ)
            aprun.wait()
        else:
            # Print debugging information
            print("BEGIN WRAPRUN DEBUGGING INFO")
            print(' Aprun call signature:\n   ',
                  ' '.join(self._subprocess_args()), '\n', sep='')
            print(' Environment variables:')
            for key, value in sorted(self.env.items()):
                print('   ', key, "=", value, sep="")
            print('\n Internal state:\n   ', self.__repr__(), '\n', sep='')
            self._file.file.seek(0)
            width = len(str(self._rank_and_color['rank']))
            print(' Tempfile contents:')
            for line_number, line in enumerate(self._file.file.readlines()):
                print('   {ln:0{width}d}|{line}'.format(
                    ln=line_number, width=width, line=line[:-1]))
            print("END WRAPRUN DEBUGGING INFO")
