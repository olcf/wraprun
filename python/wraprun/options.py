"""
A module for mananging the available options to the wraprun API, and by
extension, the CLI.

Classes provided include:
    OptionsBase
    GlobalOptions
    GroupOptions
"""


# # For generating doc strings:
# args = (list(GROUP_OPTIONS.wraprun._args.items()) +
#         list(GROUP_OPTIONS.aprun._args.items()))
# for k,v in args:
#     name = v.name if v.name else 'None'
#     flag = v.flags[0] if v.flags else 'None'
#     help = str(v.parser.get('help'))
#     action = v.parser.get('action')
#     type_key = action.__name__ if action is not None else 'action'
#     types = {
#         'ArgAction': 'int',
#         'PesAction': 'int or [int,..]',
#         'PathAction': 'str or [str,...]',
#         'FlagAction': 'bool',
#         'action': 'str',
#         }
#     print(name, '({}):'.format(types[type_key]), help)


import argparse
from os import environ as os_env
from .parseractions import (ArgAction, FlagAction, PesAction, PathAction,
                            OEAction)
from .arguments import Argument, ArgumentList
from .instance import JOB_ID, INSTANCE_ID


class OptionsBase(object):
    """Base class for managing available CLI options.

    The class contains two wraprun.arguments.ArgumentList objects:
      OptionsBase.wraprun corresponding to wraprun-specific options
      and
      OptionsBase.aprun corresponding to aprun-specific options

    This class is intended to organize both option types at each the global and
    task-spec level. To alter the options available in each scope, modify the
    daughter classes.
    """
    def __init__(self, wraprun, aprun):
        """Takes wraprun.arguments.ArgumentList objects containing
        wraprun-specific options and aprun-specific options at the daughter
        class scope (global- or task-level).
        """
        assert isinstance(wraprun, ArgumentList)
        self.wraprun = wraprun
        assert isinstance(aprun, ArgumentList)
        self.aprun = aprun

        self._coloring = None
        self._needs_unique_value = None

    def __repr__(self):
        return "{0}(wraprun={1}, aprun={2})".format(
            self.__class__.__name__, self.wraprun, self.aprun)

    def get(self, name):
        """Return an existing named option Argument object or None."""
        arg = None
        for option in (self.aprun, self.wraprun):
            try:
                arg = option[name]
            except KeyError:
                continue
        return arg

    def add_to_parser(self, parser):
        """Add instance arguments an argparse.ArgumentParser object."""
        for i in self.wraprun:
            i.add_to_parser(parser)
        for i in self.aprun:
            i.add_to_parser(parser)

    def coloring(self):
        """Return a list of instance arguments that can split a task group's MPI
        communicator into different colors.
        """
        if self._coloring is None:
            self._coloring = (
                [k for k in self.wraprun.splits] +
                [k for k in self.aprun.splits])
        return self._coloring

    def needs_unique_value(self):
        """Return a list of instance arguments that can split a task group's MPI
        communicator into different colors but requires unique default values.
        """
        if self._needs_unique_value is None:
            self._needs_unique_value = (
                [k for k in self.wraprun.needs_unique] +
                [k for k in self.aprun.needs_unique])
        return self._needs_unique_value

    def wraprun_args(self, namespace):
        """Return a dictionary of wraprun arguments explicitly defined in an
        argparse.Namespace."""
        return {k: getattr(namespace, k) for k in self.wraprun.names
                if getattr(namespace, k) is not None}

    def aprun_args(self, namespace):
        """Return a dictionary of aprun arguments explicitly defined in an
        argparse.Namespace."""
        return {k: getattr(namespace, k) for k in self.aprun.names
                if getattr(namespace, k) is not None}

    def all_args(self, namespace):
        """Return a dictionary of all arguments explicitly defined in an
        argparse.Namespace."""
        return {k: getattr(namespace, k) for k in (
            self.wraprun.names + self.aprun.names)
                if getattr(namespace, k) is not None}


class GlobalOptions(OptionsBase):
    """Manages the wraprun and aprun CLI options available
    at the global scope that can be processed by the API.
    """
    def __init__(self):
        """Defines a hard-coded list of available global-scope options.

        See wraprun.arguments.Argument for format needed to add new options.
        """
        wraprun = ArgumentList(
            Argument(
                name='conf',
                flags=['--w-conf'],
                parser={
                    'help': 'Wraprun configuration file',
                    },
                ),
            Argument(
                name='debug',
                flags=['--w-debug'],
                parser={
                    'action': FlagAction,
                    'help': 'Print debugging information and exit',
                    },
                ),
            Argument(
                name='roe',
                flags=['--w-roe'],
                parser={
                    'action': FlagAction,
                    'help': 'DEPRECATED: Redirect group IO to separate files',
                    },
                ),
            Argument(
                name='no_ld_preload',
                flags=['--w-no-ld-pre'],
                parser={
                    'action': FlagAction,
                    'default': False,
                    'help': 'Disable setting LD_PRELOAD for advanced users.',
                    },
                ),
            )

        aprun = ArgumentList(
            Argument(
                name='nocopy',
                flags=['-b'],
                parser={
                    'action': FlagAction,
                    'default': True,
                    'help': 'Do not copy executable to compute nodes',
                    },
                ),
            )
        OptionsBase.__init__(self, wraprun, aprun)


class GroupOptions(OptionsBase):
    """Manages the wraprun and aprun CLI options available
    at the task-group scope that can be processed by the API.
    """
    def __init__(self):
        """Defines a hard-coded list of available task-scope options.

        See wraprun.arguments.Argument for format needed to add new options.
        """
        wraprun = ArgumentList(
            Argument(
                name='cd',
                flags=['--w-cd'],
                split=True,
                parser={
                    'metavar': "dir[,dir...]",
                    'default': ['./'],
                    'action': PathAction,
                    'help': 'Task working directory',
                    },
                ),
            Argument(
                name='oe',
                flags=['--w-oe'],
                split=True,
                unique=True,
                parser={
                    'metavar': "fname[,fname...]",
                    'default': ['{name}.{job}_w{instance}'.format(
                        name=os_env.get('PBS_JOBNAME', 'unnamed'),
                        job=JOB_ID,
                        instance=INSTANCE_ID)],
                    'action': OEAction,
                    'help': 'STDOUT/STDERR file basename',
                    },
                ),
            )

        aprun = ArgumentList(
            Argument(
                name='pes',
                flags=['-n'],
                split=True,
                parser={
                    'metavar': "pes[,pes...]",
                    'required': True,
                    'action': PesAction,
                    'help': 'Number of processing elements (PEs). REQUIRED',
                    },
                formatter=lambda v: str(sum(v)),
                ),
            Argument(
                name='arch',
                flags=['-a'],
                parser={
                    'metavar': 'arch',
                    'action': ArgAction,
                    'help': 'Host architecture',
                    },
                ),
            Argument(
                name='cpu_list',
                flags=['-cc'],
                parser={
                    'metavar': 'list',
                    'action': ArgAction,
                    'help': 'CPU list',
                    },
                ),
            Argument(
                name='cpu_placement',
                flags=['-cp'],
                parser={
                    'metavar': 'file',
                    'action': ArgAction,
                    'help': 'CPU placement file',
                    },
                ),
            Argument(
                name='depth',
                flags=['-d'],
                parser={
                    'metavar': 'depth',
                    'action': ArgAction,
                    'help': 'Process affinity depth',
                    },
                ),
            Argument(
                name='cpus_per_cu',
                flags=['-j'],
                parser={
                    'metavar': 'cpupcu',
                    'action': ArgAction,
                    'help': 'CPUs per CU',
                    },
                ),
            Argument(
                name='node_list',
                flags=['-L'],
                parser={
                    'metavar': 'nodes',
                    'action': ArgAction,
                    'help': 'Node list',
                    },
                ),
            Argument(
                name='pes_per_node',
                flags=['-N'],
                parser={
                    'metavar': 'ppn',
                    'action': ArgAction,
                    'help': 'PEs per node'
                    },
                ),
            Argument(
                name='pes_per_numa_node',
                flags=['-S'],
                parser={
                    'metavar': 'ppnn',
                    'action': ArgAction,
                    'help': 'PEs per NUMA node'
                    },
                ),
            Argument(
                name='numa_node_list',
                flags=['-sl'],
                parser={
                    'metavar': 'list',
                    'nargs': 1,
                    'action': ArgAction,
                    'help': 'NUMA node list'
                    },
                ),
            Argument(
                name='numa_nodes_per_node',
                flags=['-sn'],
                parser={
                    'metavar': 'nnpn',
                    'nargs': 1,
                    'action': ArgAction,
                    'help': 'Number of NUMA nodes per node'
                    },
                ),
            Argument(
                name='strict_memory',
                flags=['-ss'],
                parser={
                    'nargs': 0,
                    'action': FlagAction,
                    'help': 'Use strict memory containment'
                    },
                ),
            Argument(
                name='exe',
                parser={
                    'metavar': 'exe [...]',
                    'nargs': argparse.REMAINDER,
                    'help': 'Executable and argument string. REQUIRED'
                    },
                ),
            )
        OptionsBase.__init__(self, wraprun, aprun)


GROUP_OPTIONS = GroupOptions()
GLOBAL_OPTIONS = GlobalOptions()
