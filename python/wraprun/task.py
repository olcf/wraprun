"""
The task module provides the following classes:

    Rank      - for managing rank runtime parameters.
    TaskError - for mananging exceptions in task module classes.
    TaskGroup - for processing and formatting MPMD mode task specs in the
                wraprun API.
"""

from collections import OrderedDict
from .options import GROUP_OPTIONS
from .instance import JOB_ID, INSTANCE_ID


class Rank(object):
    '''Information about ranks within an MPMD task group.

    Stores the CWD and color of an MPI rank.
    '''
    # Ordered tuple of keys in output file lines.
    FILE_CONTENT = (
        'color',
        'path',
        'fname',
        'env',
        )

    FILE_FORMAT = ' '.join(('{{{0}}}'.format(k) for k in FILE_CONTENT))

    def __init__(self, rank, color, **kwargs):
        '''Store data to be written to the rank parameters file.'''
        self.rank = rank
        self._data = {
            'color': color,
            'path': './',
            'fname': '{job}_{instance}_w_{color}'.format(
                job=JOB_ID, instance=INSTANCE_ID, color=color),
            'env': '',
            }
        self._data.update(kwargs)

    def __repr__(self):
        """Return the representation of a Rank runtime data."""
        return 'Rank({0}:{1})'.format(self.rank, self._data['color'])

    def string(self):
        '''Return rank data string for writing to the rank parameters file.'''
        return Rank.FILE_FORMAT.format(**self._data)

    @property
    def color(self):
        '''Return the rank 'color' integer.'''
        return self._data['color']


class TaskError(Exception):
    """A class for managing Task exceptions."""
    pass


class TaskGroup(object):
    """An MPMD task group.

    Each TaskGroup contains the ordered dictionary of arguments specifying an
    MPMD mode aprun task.
    """
    def __init__(self, first_rank=0, first_color=0, **kwargs):
        """Constructs a TaskGroup.

        Ranks and communicator colors start at first_rank and first_color
        respectively. Arguments are passed by name as keywords with wraprun API
        format values.
        """
        self._ranks = []
        self._first_color = first_color
        self.args = OrderedDict.fromkeys(
            GROUP_OPTIONS.wraprun.names +
            GROUP_OPTIONS.aprun.names)
        for k in self.args:
            self.args[k] = kwargs.pop(k, GROUP_OPTIONS.get(k).default)
        self._balance()
        self._set_ranks(first_rank, first_color)

    def __repr__(self):
        format_string = "TaskGroup(r{r0}.c{c0}-r{r1}.c{c1}, exe='{exe}')"
        return format_string.format(r0=self._ranks[0].rank,
                                    c0=self._ranks[0].color,
                                    r1=self._ranks[-1].rank,
                                    c1=self._ranks[-1].color,
                                    exe=self.args['exe'][0])

    @property
    def ranks(self):
        """Return a list of rank objects in this task group."""
        return self._ranks

    def _balance(self):
        """Balances the split arguments by number of colors.

        Arguments that can split the MPI communicator must have the same number
        of colors. If a split arg is given a single value (default), that value
        is used by each color. If a split arg has more than one value, that
        number of communicator colors are generated and default/single split
        args are extended to match the correct length.

        It is an error for specified split args to differ in number of colors.
        """
        splits = set()
        for k in GROUP_OPTIONS.coloring():
            value = self.args[k]
            if value is not None:
                if not isinstance(value, (tuple, list)):
                    value = [value]
                    self.args[k] = value
                if len(value) > 1:
                    splits.add(len(value))
        if len(splits) > 1:
            raise TaskError(
                'Inconsistent split lengths {0}'.format(splits))
        number_of_splits = max(splits) if splits else 1
        for k in GROUP_OPTIONS.needs_unique_value():
            value = self.args[k]
            default = GROUP_OPTIONS.get(k).default
            if value == default:
                # Len should be 1 and type should be tuple or list: be sure to
                # provide a default when adding new splitting options.
                self.args[k] = ["{0}.{1}".format(value[0],
                                                 self._first_color + i)
                                for i in range(number_of_splits)]
            elif len(value) != number_of_splits:
                # Len should be 1 and type should be tuple or list: be sure to
                # provide a default when adding new splitting options.
                self.args[k] = ["{0}_w{1}".format(value[0], i)
                                for i in range(number_of_splits)]

        for k in GROUP_OPTIONS.coloring():
            value = self.args[k]
            if value is not None and len(value) != number_of_splits:
                # Len should be 1 and type should be tuple or list: be sure to
                # provide a default when adding new splitting options.
                self.args[k] = value * number_of_splits

    def last_rank_and_color(self):
        """Return a dictionary of the highest rank and color indexes in this
        task group.
        """
        try:
            rank = self._ranks[-1].rank
            color = self._ranks[-1].color
        except IndexError:
            rank = None
            color = None
        return {'rank': rank, 'color': color}

    def _set_ranks(self, first_rank, first_color):
        """Populate the list of ranks given the specified number of processing
        elements."""
        ranks = []
        rank_id = first_rank
        for i, pes_count in enumerate(self.args['pes']):
            color = first_color + i
            if pes_count is None:
                raise TaskError('Invalid PES')
            for _ in range(pes_count):
                rank = Rank(rank_id, color,
                            path=self.args['cd'][i],
                            fname=self.args['oe'][i],
                            env=self.args['env'][i])
                ranks.append(rank)
                rank_id += 1
        self._ranks = ranks

    def file_lines(self):
        """Generator for lines written to rank runtime parameter file."""
        for rank in self._ranks:
            yield rank.string()

    def cli_args(self):
        """Return a list of aprun-format CLI argument strings that represent
        this task group.
        """
        arglist = []
        for name, value in self.args.items():
            if value is None:
                continue
            arg = GROUP_OPTIONS.aprun.get(name, None)
            if arg is None:
                continue
            arglist.extend(arg.format(value))
        return arglist

    def cli_string(self):
        """Return a string of aprun CLI arguments representing this task."""
        return ' '.join(self.cli_args())
