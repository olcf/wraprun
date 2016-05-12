"""
The arguments module provides the following classes:

    Argument     - for managing specifying API and CLI arguments to wraprun.
    ArgumentList - for managing an ordered collection of named Arguments.
"""

from collections import OrderedDict


class Argument(object):
    """A wraprun API argument.

    Valid Argument instances must have the following:

    name - Each argument has a unique name that is used to specify the argument
    when passing a value to it as a keyword argument in the wraprun API.

    flags - The argument is triggered when any of the specified list of unique
    'flag' strings is encountered in parsing CLI strings. These same flags are
    used to populate the arguments passed to aprun.

    split (default: False) - Specifies whether the argument can split a task
    MPI communicator into several 'colors'. Spitting arguments are always
    passed as an iterable in the API or a comma-separated string on the CLI.

    parser - a dictionary of extra keyword arguments passed to
    argparse.ArgumentParser.add_argument to define help strings, default
    values, and parser actions. In particular, arguments must have an 'action'
    that over-rides the default behavior of argparse such that arguments that
    do not appear on the CLI are given a value of None in the namespace
    returned by argparse.parse_*_args()

    formatter - an optional function that formats the argument value as it
    needs to appear on the CLI to be passed to aprun. IE, this is used to
    convert split-capable aprun arguments from their wraprun form to their
    aprun form.  See the 'pes' argument for an example.
    """
    def __init__(self, name, **kwargs):
        """Constructor. See class documentation for details."""
        self.name = name
        self.flags = kwargs.pop('flags', None)
        self.split = kwargs.pop('split', False)
        self.unique = kwargs.pop('unique', False)
        self.parser = kwargs.pop('parser')
        self._formatter = kwargs.pop('formatter', None)

        self.parser['dest'] = self.name

        assert isinstance(self.flags, (list, type(None)))

    def format(self, value):
        """Return a list of aprun CLI format 'flag value' strings.

        The 'value' string is converted from it's wraprun API form
        to an appropriate aprun CLI form using an optional class instance
        formatter function when necessary.
        """
        output = []
        if self.flags:
            output.append(self.flags[0])

        if self._formatter is not None:
            output.append(self._formatter(value))
        elif isinstance(value, list):
            for i in value:
                output.append(str(i))
        elif isinstance(value, bool):
            pass
        else:
            output.append(str(value))
        return output

    def add_to_parser(self, parser):
        """Adds argument to the given argpase.ArgumentParser object."""
        if self.flags is not None:
            parser.add_argument(*self.flags, **self.parser)
        else:
            parser.add_argument(**self.parser)

    @property
    def default(self):
        """Return the default argument value in Wraprun API form."""
        return self.parser.get(
            'default',
            [None] if self.split else None)


class ArgumentList(object):
    """A list of Argument objects.

    This class provides special methods for common operations on sets of
    wraprun API and CLI arguments. In particular, it maintains the order of
    the added arguments and allows lookup by argument name.
    """
    def __init__(self, *args):
        self._args = OrderedDict()
        for arg in args:
            self._args[arg.name] = arg

    def __repr__(self):
        return '{0}'.format(tuple(self.names))

    @property
    def names(self):
        """Return the defined argument names, in the order added."""
        return list(self._args.keys())

    @property
    def splits(self):
        """Return a list of arguments that can split MPI comm colors."""
        return [k for k in self._args if self._args[k].split]

    @property
    def needs_unique(self):
        """Return a list of arguments that require color-unique values."""
        return [k for k in self._args if self._args[k].unique]

    def __iter__(self):
        """Iterates through arguments in order added."""
        for _, arg in self._args.items():
            yield arg

    def __len__(self):
        """Return the number of arguments added."""
        return len(self._args)

    def __contains__(self, name):
        """Return True if argument name is defined."""
        return name in self._args

    def __getitem__(self, name):
        """Return argument with given name."""
        return self._args[name]

    def get(self, name, default=None):
        """Return argument with given name if exists, else return default."""
        return self._args.get(name, default)
