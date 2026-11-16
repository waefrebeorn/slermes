#!/usr/bin/env python3
"""The real user home, resolved the same way on Windows, Linux and macOS.

Two oracles did `import pwd; pwd.getpwuid(os.getuid()).pw_dir` to get an
env-independent home. `pwd` and `os.getuid()` are Unix-only, so on Windows the
import itself raised ModuleNotFoundError and the port was reported
ORACLE-BROKEN -- an absent measurement, not a port defect.

Order: pwd (ignores $HOME, which the harness deliberately points at the sandbox)
-> expanduser("~") -> HOME. The sandbox override must NOT win, which is why
expanduser is only the fallback.
"""
from __future__ import annotations

import os


def real_home() -> str:
    try:
        import pwd
        return pwd.getpwuid(os.getuid()).pw_dir
    except (ImportError, AttributeError, KeyError):
        pass
    for cand in (os.path.expanduser("~"), os.environ.get("HOME"),
                 os.environ.get("USERPROFILE")):
        if cand and cand != "/":
            return cand
    return "."


REAL_HOME = real_home()
