#!/usr/bin/env python

# The MIT License (MIT)
#
# Copyright (c) 2015 M. Belhorn
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

'''
This module provides a command for constructing a the command line call needed
to run independent programs in aprun's MPMD-mode with A. Simpson's MPI wrapper.

The main command expects an aprun MPMD-mode argument string. See the aprun man
page for details. The following nomeclature is assumed in this module given an
aprun call of the form:

    wraprun [GlobalOptions] Group [ : Group]...

Each "Group" consists of arguments in the form:

    -n PE1[,PE2]... [GroupOptions] prog [ProgOptions]

The global options allowed include all MPMD mode aprun options and all wraprun
global options. The aprun '-b' flag is explicitly added if missing.

Group options consist of the allowed MPMD-mode per-executable aprun options as
well as group-specific wraprun options. Each group is required to use the aprun
'-n' flag to specify the number of processing elements. If a comma-separated
list of integers is passed to '-n', the MPI ranks created for that group will be
'split' or divided amongst a number of MPI 'colors' such that multiple
independent parallel instances of prog can run on the same compute node.

For each MPI rank started by aprun, a line is added to a file in which runtime
details for the rank are passed to the MPI wrapper.
'''

from __future__ import print_function
import subprocess
import sys
import os
import argparse
import shlex
import tempfile

VERBOSE = False

def log(lvl, msg):
    '''Print log message at given level.'''
    log_string = 'wraprun [{lvl}]: {msg}'
    if VERBOSE or lvl in ['WARNING', 'ERROR']:
        print(log_string.format(lvl=lvl, msg=msg))


class WraprunError(Exception):
    '''Class for managing wraprun-specific errors.'''
    pass


class Rank(object):
    '''Information about ranks within an MPMD executable group.

    Stores the CWD and color of an MPI rank.
    '''
    # Ordered tuple of keys in output file lines.
    FILE_CONTENT = (
        'color',
        'path',
        )

    FILE_FORMAT = ' '.join(('{{{0}}}'.format(k) for k in FILE_CONTENT))

    def __init__(self, rank, color, **kwargs):
        '''Store data to be written to the rank parameters file.'''
        self.rank = rank
        self._data = {
            'color': color,
            'path': './',
            }
        self._data.update(kwargs)

    def string(self):
        '''Return a rank data string for writing to the rank parameters file.'''
        return Rank.FILE_FORMAT.format(**self._data)

    @property
    def color(self):
        '''Return the rank 'color' integer.'''
        return self._data['color']



class ArgAction(argparse.Action):
    '''Base class for custom group argument argparse Actions.'''
    def __init__(self, *args, **kwargs):
        '''Create arparse.Action.'''
        super(ArgAction, self).__init__(*args, **kwargs)
        self.split = False
        self.cli = None

    def __call__(self, parser, namespace, values, option_string=None):
        '''Used by Argparse to process arguments.'''
        setattr(namespace, self.dest, values)
        self.cli = [option_string, values]

class FlagAction(ArgAction):
    '''Argparse action to processes MPMD group 'flag'-type arguments.'''
    def __init__(self, *args, **kwargs):
        '''Create arparse.Action for processing flag-type argument.'''
        default = kwargs.pop('default', False)
        const = kwargs.pop('const', True)
        nargs = kwargs.pop('nargs', 0)
        if nargs != 0:
            log('DEBUG',
                'Assuming flag is meant to consume no additional args.')
        super(FlagAction, self).__init__(
            *args, default=default, nargs=0, const=const, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        '''Used by Argparse to process arguments.'''
        if self.const is not self.default:
            setattr(namespace, self.dest, self.const)
            self.cli = [option_string] if option_string is not None else None

class PesAction(ArgAction):
    '''Argparse action to process MPMD group "Processing Elements" (PES).

    Looks for rank colorsplitting as comma-separated integer lists of the form
        '-n x[,y]...'
    where 'x' and 'y' are integers.
    '''
    def __init__(self, *args, **kwargs):
        '''Create arparse.Action for processing PES argument.'''
        super(PesAction, self).__init__(*args, **kwargs)
        self.split = True

    def __call__(self, parser, namespace, values, option_string=None):
        '''Used by Argparse to process arguments.'''
        ranks_per_color = [int(i) for i in values.split(',')]
        setattr(namespace, self.dest, ranks_per_color)
        self.cli = [option_string, str(sum(ranks_per_color))]

class PathAction(ArgAction):
    '''Argparse action to process MPMD group CWD '--w-cd' arguments.

    Looks for rank colorsplitting as comma-separated path lists of the form
      --w-cd path1[,path2]...'
    where the paths cannot contain ','.
    '''
    def __init__(self, *args, **kwargs):
        '''Create arparse.Action for processing group CWD argument.'''
        super(PathAction, self).__init__(*args, **kwargs)
        self.split = True
        self.is_aprun_arg = False

    def __call__(self, parser, namespace, values, option_string=None):
        '''Used by Argparse to process arguments.'''
        paths_by_color = [i for i in values.split(',')]
        setattr(namespace, self.dest, paths_by_color)


class Group(object):
    '''MPMD mode executable group parameters from group argument list.'''
    def __init__(self, arg_list, first_color, first_rank, index=None):
        '''Parse group argument list.

        Ranks and colors are specified by the group arguments are enumerated
        from first_color and first_rank.
        '''
        self.wparser = argparse.ArgumentParser(add_help=False)
        if index == 0:
            self.wparser.add_argument('-b', dest='bypass', action=FlagAction)
        else:
            self.wparser.add_argument('-b', const=False, dest='bypass', action=FlagAction)
        self.wparser.add_argument('--w-cd', dest='path',
                default=['./'], action=PathAction)
        self.wparser.add_argument('-n', dest='pes', required=True, action=PesAction)
        self.wparser.add_argument('-a', dest='arch', action=ArgAction)
        self.wparser.add_argument('-cc', dest='cpu_list', action=ArgAction)
        self.wparser.add_argument('-cp', dest='cpu_placement_file', action=ArgAction)
        self.wparser.add_argument('-d', dest='depth', action=ArgAction)
        self.wparser.add_argument('-L', dest='node_list', action=ArgAction)
        self.wparser.add_argument('-N', dest='pes_per_node', action=ArgAction)
        self.wparser.add_argument('-S', dest='pes_per_numa_node', action=ArgAction)
        self.wparser.add_argument('-sl', dest='numa_node_list', action=ArgAction)
        self.wparser.add_argument('-sn', dest='numa_nodes_per_node', action=ArgAction)
        self.wparser.add_argument('-ss', dest='strict_memory', action=FlagAction)
        self.wparser.add_argument('exe', nargs=argparse.REMAINDER)

        self.wraprun = self.wparser.parse_args(arg_list)
        self.nsplits = self._get_nsplit(self.wparser)
        self._scale_split_args(self.wparser)

        self.ranks = self._set_ranks(first_rank, first_color)
        self.cli_arglist = self._generate_arglist()
        
    def _generate_arglist(self):
        return [s for a in self.wparser._actions
            if getattr(a, 'cli', None) is not None
            for s in a.cli] + self.wraprun.exe
        
    def update_arglist(self, arg, value):
        try:
            action = (a for a in self.wparser._actions if a.dest == arg).next()
        except StopIteration:
            raise WraprunError(
                    'No action with dest={0} in group parser.'.format(arg))
        opt_string = next(iter(action.option_strings), None)
        action(self.wparser, self.wraprun, value, action.option_strings[0])
        self.cli_arglist = self._generate_arglist()

    def scan_splits(self, parser):
        '''Iterate through arguments that split PEs into colors.'''
        for action in parser._actions:
            value = getattr(self.wraprun, action.dest, None)
            if getattr(action, 'split', False) and value is not None:
                yield value, action

    def _get_nsplit(self, parser):
        '''Return the number of rank colors specified by arguments that can
        trigger color splits.

        If more than 1 non-singular split is specified, an error is raised.
        '''
        splits = set(
                (len(arg) for arg, _ in self.scan_splits(parser)
                    if len(arg) != 1))
        if len(splits) > 1:
            raise WraprunError(
                'Inconsistent rank color memberships found {}'.format(splits))
        else:
            return max(splits) if splits else 1

    def _scale_split_args(self, parser):
        '''Set the number of processing elements started by the group to the sum
        of ranks across all colors.
        '''
        for value, action in self.scan_splits(parser):
            if len(value) != self.nsplits:
                setattr(self.wraprun, action.dest, value * self.nsplits)

    def _set_ranks(self, first_rank, first_color):
        '''Return a list of all ranks started by the group.'''
        ranks = []
        for subgroup_id, n_pes_in_sg in enumerate(self.wraprun.pes):
            color = first_color + subgroup_id
            for pid in range(n_pes_in_sg):
                rank_id = subgroup_id * n_pes_in_sg + pid + first_rank
                rank = Rank(rank_id, color, path=self.wraprun.path[subgroup_id])
                ranks.append(rank)
        return ranks

    @property
    def last_color(self):
        '''Return the color of the last rank in the group.'''
        return self.ranks[-1].color

    @property
    def last_rank(self):
        '''Return the last rank in the group.'''
        return self.ranks[-1].rank

    @property
    def total_pes(self):
        '''Return the total number of processing elements in the group.'''
        return sum(self.wraprun.pes)

    def has_arg(self, arg, value):
        '''Return true if arg in set of aprun arguments'''
        return getattr(self.wraprun, arg, None) == value

    def arg_string(self):
        '''Return the aprun command line string representing the group.'''
        return ' '.join(self.cli_arglist)


class Arguments(object):
    '''A class to parse and encapsulate arguments to a wraprun call and it's
    underlying aprun call.'''
    REQUIRED_APRUN_ARGS = {'bypass': True,}

    def __init__(self):
        '''Parse the command line arguments passed to wraprun.'''
        self._args = {
            "wraprun": None,
            "aprun": None,
            "groups": None,
            "unprocessed": None,
            }
        self._parse_cli()
        self._parse_groups()
        self._parse_global_args()

    @property
    def wraprun(self):
        '''Return the global wraprun-specific arguments namespace.'''
        return self._args["wraprun"]

    @wraprun.setter
    def wraprun(self, namespace):
        '''Set the global wraprun-specific arguments namespace.'''
        self._args["wraprun"] = namespace

    @property
    def groups(self):
        '''Return the list of groups.'''
        return self._args["groups"]

    @groups.setter
    def groups(self, group_list):
        '''Set the list of groups.'''
        self._args["groups"] = group_list

    @property
    def aprun(self):
        '''Return the global aprun-specific arguments.'''
        return self._args["aprun"]

    @aprun.setter
    def aprun(self, aprun_global_arglist):
        '''Set the global aprun-specific arguments.'''
        self._args["aprun"] = aprun_global_arglist

    @property
    def _unprocessed(self):
        '''Return the list of unprocessed argument strings.'''
        return self._args["unprocessed"]

    @_unprocessed.setter
    def _unprocessed(self, arglist):
        '''Set the list of unprocessed argument strings.'''
        self._args["unprocessed"] = arglist

    def _parse_cli(self):
        '''Parse CLI arguments for wraprun specific options and aprun specific
        arguments.'''
        cli_parser = argparse.ArgumentParser(
            description='Generates shell-evaluable string to set '
            'the environment and call wrapped aprun in MPMD-mode.')
        cli_parser.add_argument(
            '--w-debug', dest='debug', action='store_true',
            help='Show debugging information.')
        cli_parser.add_argument(
            '--w-roe', dest='redirect_stdout', action='store_true',
            help='Redirect stdout and stderr for each MPI group to separate output files')

        self.wraprun, self._unprocessed = cli_parser.parse_known_args()

        # Update global flags
        global VERBOSE
        VERBOSE = self.wraprun.debug

    def _group_arglists(self):
        '''Iterate by group through the lists of group argument string.'''
        arglist = self._unprocessed
        start = 0
        try:
            end = arglist.index(':')
        except ValueError:
            end = None
        while end is not None:
            yield arglist[start:end]
            start = end + 1
            try:
                end = arglist.index(':', start)
            except ValueError:
                end = None
        yield arglist[start:]

    def _parse_groups(self):
        '''Parse each group arguments and set the list of groups.'''
        self.groups = []
        groups = self.groups
        starting_color = 0
        starting_rank = 0
        for i, group_args in enumerate(self._group_arglists()):
            group = Group(group_args, starting_color, starting_rank, index=i)
            groups.append(group)
            starting_color = group.last_color + 1
            starting_rank = group.last_rank + 1

    def _parse_global_args(self):
        '''Parse the global arguments caught in the first group.'''
        first_group = self.groups[0]
        args = ['aprun']
        for arg, value in self.REQUIRED_APRUN_ARGS.items():
            if not first_group.has_arg(arg, value):
                log('WARNING',
                    "Adding required aprun argument '{a}'.".format(a=arg))
                first_group.update_arglist(arg, value)
        self.aprun = args

    def _group_strings(self):
        '''Iterate over the group command line strings by group.'''
        for group in self.groups:
            yield group.arg_string()

    def _groups_string(self):
        '''Return the command line string representing all the groups.'''
        return ' : '.join(self._group_strings())

    def _global_string(self):
        '''Return the command line string of the aprun global options.'''
        return ' '.join(self.aprun)

    def string(self):
        '''Return the aprun command line string.'''
        return '{aprun} {groups}'.format(
            aprun=self._global_string(),
            groups=self._groups_string())


class Wraprun(object):
    '''A class for translating wraprun arguments into the files, environment
    variables and cli call needed to run independent programs using
    aprun in MPMD-mode.
    '''
    def __init__(self):
        '''Initialize the data needed to call aprun with the MPI wrapper.'''
        self.args = Arguments()
        self.groups = self.args.groups

    @property
    def job_id(self):
        '''Determine PBS job ID from the environment.

        If not available (for instance if called outside a PBS script), an error
        is raised unless the script is run with debugging flags.
        '''
        if not hasattr(self, '_job_id'):
            try:
                self._job_id = os.environ["PBS_JOBID"]
            except KeyError:
                global VERBOSE
                VERBOSE = True
                self.args.wraprun.debug = True
                log('WARNING', 'No JOB ID found. Running in debug mode!')
                self._job_id = '000000'
        return self._job_id

    @property
    def account(self):
        '''Return the account string under which the parent PBS script is
        running.
        '''
        if not hasattr(self, '_account'):
            if self.job_id != '000000':
                qstat = subprocess.Popen(
                    ['qstat', '-f', self.job_id], stdout=subprocess.PIPE)
                for line in qstat.stdout.readlines():
                    if 'Account_Name' in line:
                        self._account = (line[19:-1]).lower()
                        break
            else:
                try:
                    self._account = os.environ["W_TEST_ACCOUNT"]
                except KeyError:
                    raise WraprunError(
                            "Job account not found!"
                            " Set W_TEST_ACCOUNT env-var for testing.")
        return self._account

    @property
    def file(self):
        '''Return the File object for storing the wrapper rank parameters.'''
        if not hasattr(self, '_file'):
            dir_path = os.path.join(
                os.environ["MEMBERWORK"],
                self.account,
                ".wraprun")
            try:
                os.makedirs(dir_path, 0700)
            except OSError as err:
                if err.errno is not 17:
                    raise
            self._file = tempfile.NamedTemporaryFile('a+',
                    prefix='tmp{job}_'.format(job=self.job_id),
                    suffix='.wraprun',
                    dir=dir_path)

            for group in self.groups:
                for rank in group.ranks:
                    self._file.file.write(rank.string() + "\n")
            self._file.file.flush()
        return self._file

    @property
    def env(self):
        '''Return a dictionary of environment variables set by wraprun
        at runtime.'''
        if not hasattr(self, '_env'):
            self._env = dict()
            # Add wraprun runtime vars to it.
            if 'WRAPRUN_PRELOAD' not in os.environ.keys():
                log('WARNING',
                        'Environment variable WRAPRUN_PRELOAD not set. '
                        'Using alternate MPI wrapper.')
            self._env['LD_PRELOAD'] = os.environ.pop(
                    'WRAPRUN_PRELOAD',
                    '/lustre/atlas/proj-shared/{0}/libsplit.so'.format(
                        self.account)
                    )
            self._env['WRAPRUN_FILE'] = self.file.name
            if self.args.wraprun.redirect_stdout:
                log('INFO',
                        'Explicit --w-roe is deprecated. '
                        'Redirecting stdio is the default behavior.')
            self._env['W_REDIRECT_OUTERR'] = '1'
            self._env['W_IGNORE_SEGV'] = '1'
            self._env['W_UNSET_PRELOAD'] = '1'
        return self._env

    def eval(self):
        '''Launch aprun in a subprocess.'''
        os.environ.update(self.env)

        # Last chance to update the log.
        sys.stdout.flush()
        if not self.args.wraprun.debug:
            log('INFO', 'Launching aprun in wrapped environment')
            aprun = subprocess.Popen(
                    shlex.split(self.args.string()),
                    env=os.environ)
            aprun.wait()
        else:
            log('WARNING', 'Job not launched due to debugging.')
            log('DEBUG', 'Input Arguments:')
            arglines = (" ".join(sys.argv)).split(':')
            for i, g in enumerate(arglines):
                if i == 0:
                    log('DEBUG', '  {0}'.format(g))
                else:
                    log('DEBUG', '    : {0}'.format(g))
            log('DEBUG', 'Environment Modifications:')
            for var, val in self.env.items():
                log('DEBUG', '  {0}={1}'.format(var, val))
            log('DEBUG', 'Aprun Invocation:')
            log('DEBUG', '  {0}'.format(self.args.string()))
            log('DEBUG', 'Rank Parameters File:')
            width = len(str(self.groups[-1].last_rank))
            self.file.file.seek(0)
            for line_number, line in enumerate(self.file.file.readlines()):
                log('DEBUG', '  {ln:0{width}d}|{line}'.format(
                    ln=line_number, width=width, line=line[:-1]))


def main():
    '''Produces the data needed by the MPI wrapper library to run aprun flexibly
    in MPMD mode.
    '''
    wraprun = Wraprun()
    wraprun.eval()


#===============================================================================
if __name__ == '__main__':
    main()

