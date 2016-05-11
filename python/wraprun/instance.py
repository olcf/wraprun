"""
This module produces a singleton INSTANCE_ID that is unique to each instance of
wraprun launched from a common parent shell script, nominally a PBS job.
"""

import os
import fcntl

_WRAPRUN_PID = os.getpid()
_SHELL_PID = os.getppid()
JOB_ID = os.environ.get('PBS_JOBID', _SHELL_PID)

_LOCKFILE = '/tmp/wraprun.{0}.{1}'.format(JOB_ID, _SHELL_PID)

with open(_LOCKFILE, "a") as f:
    fcntl.flock(f, fcntl.LOCK_EX)
    f.write(str(_WRAPRUN_PID)+'\n')
    fcntl.flock(f, fcntl.LOCK_UN)

with open(_LOCKFILE, "r") as f:
    INSTANCE_ID = [int(i.strip()) for i in f.readlines()].index(_WRAPRUN_PID)
