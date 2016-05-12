"""
This module produces a singleton INSTANCE_ID that is unique to each instance of
wraprun launched from a common parent shell script, nominally a PBS job.
"""

import os
import fcntl
from tempfile import gettempdir

JOB_ID = os.environ.get('PBS_JOBID', os.getppid())

# NOTE: MPB 2016.05.12: Maybe put these in a dedicated tmp dir to avoid noise.
_LOCKFILE = os.path.join(
    gettempdir(),
    'wraprun.{0}.{1}'.format(JOB_ID, os.getppid()))

with open(_LOCKFILE, "a") as f:
    os.chmod(_LOCKFILE, 0o600)
    fcntl.flock(f, fcntl.LOCK_EX)
    f.write(str(os.getpid())+'\n')
    fcntl.flock(f, fcntl.LOCK_UN)

with open(_LOCKFILE, "r") as f:
    INSTANCE_ID = [int(i.strip())
                   for i in f.readlines()].index(os.getpid())
