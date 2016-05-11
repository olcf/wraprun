# pylint: disable=too-few-public-methods
"""
A module for managing argparse.Action daughter classes needed to parse wraprun
format arguments and leave None type argument values when options are not
explicitly passed.

This module provides the classes:
    ArgAction
    FlagAction
    PesAction
    PathAction

each for different flag types.
"""

import argparse


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
        default = kwargs.pop('default', None)
        const = kwargs.pop('const', True)
        nargs = kwargs.pop('nargs', 0)
        if nargs != 0:
            raise Exception(
                "Flag arguments should not consume additional arguments")
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


class OEAction(ArgAction):
    '''Argparse action to process MPMD group CWD '--w-oe' arguments.

    Looks for rank colorsplitting as comma-separated filename lists of the form
      --w-oe fname1[,fname2...]'
    where the paths cannot contain ','.
    '''
    def __init__(self, *args, **kwargs):
        '''Create arparse.Action for processing group OE argument.'''
        super(OEAction, self).__init__(*args, **kwargs)
        self.split = True
        self.is_aprun_arg = False

    def __call__(self, parser, namespace, values, option_string=None):
        '''Used by Argparse to process arguments.'''
        oe_filenames_by_color = [i for i in values.split(',')]
        setattr(namespace, self.dest, oe_filenames_by_color)
