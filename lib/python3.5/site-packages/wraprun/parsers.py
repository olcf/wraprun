"""
This module provides classes and functions for parsing CLI arguments available
to the wraprun API.
"""

import argparse
from .options import GROUP_OPTIONS, GLOBAL_OPTIONS


class ArgumentParserError(Exception):
    '''Exception class for catching custom errors in the wraprun CLI parser.'''
    pass


class ArgumentParser(argparse.ArgumentParser):
    '''Derived argument parser that provides a flag for throwing exceptions
    normally handled internally by argparse.'''
    def __init__(self, *args, **kwargs):
        self.throw_errors = False
        super(ArgumentParser, self).__init__(*args, **kwargs)

    def error(self, message):
        if self.throw_errors:
            raise ArgumentParserError(message)
        else:
            super(ArgumentParser, self).error(message)


class TaskParser(object):
    """Parses task-group API arguments."""
    def __init__(self):
        """Constructor.

        Defines an argparse.ArgumentParser for task group options.
        """
        self._task_parser = ArgumentParser(add_help=False)
        GROUP_OPTIONS.add_to_parser(self._task_parser)

    def parse_task(self, string):
        """Retrun a dictionary of specified task arguments in 'string'

        Pseudo-arguments appearing in the executable string are removed.
        """
        arglist = string.split()
        names = self._task_parser.parse_args(arglist)
        args = GROUP_OPTIONS.all_args(names)
        if args['exe'][0] == '--':
            args['exe'].pop(0)
        return args

    def update(self, dictionary, string):
        '''Update given dictionary with arguments parsed from string.'''
        if string is not None:
            dictionary.update(self.parse_task(string))


def create_cli_parser():
    """Returns a parser with fully formatted help information for use on the
    command line.

    In order to have correct CLI syntax in the help string, the structure of
    the parser is invalid passed the first specified task group. As such, the
    argv list passed to this parser must be cleaned of all task groups beyond
    the first.
    """
    parser = ArgumentParser(
        add_help=False,
        description='A flexible aprun task bundling tool.')

    global_opts = parser.add_argument_group(
        title='Global Options',
        description=('Global options may only occur once'
                     ' and must precede the first task spec.'))

    global_opts.add_argument(
        '-h',
        '--help',
        action='help',
        help='Show this help message and exit')

    GLOBAL_OPTIONS.add_to_parser(global_opts)

    task = parser.add_argument_group(
        title='Defining Task Specs',
        description='''A task must have a number of PEs and an executable on Lustre.
        All Wraprun/Aprun options for the task must be given before the
        executable. Ambiguous option errors can be avoided by inserting the
        pseudo-argument '--' prior to the executable string.
        ''')

    GROUP_OPTIONS.add_to_parser(task)

    task_extra = parser.add_argument_group(
        title='Additional Task Specs',
        description='Each additional task spec is separated by " : ". ')
    task_extra.add_argument(
        'other_tasks',
        metavar=' : task',
        nargs="*",
        help="Optional additional tasks")
    return parser


def parse_globals(argv):
    '''Parse argv for global options and return the resulting namespace.'''
    parser = ArgumentParser()
    GLOBAL_OPTIONS.add_to_parser(parser)
    return parser.parse_known_args(argv)
