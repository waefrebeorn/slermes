"""twin for shell_heredoc_find_heredoc_close.

Traces LIVE tools/shell_heredoc.py::_find_heredoc_close with the same
rung order as the C harness. Input encoding:
"<command>\t<body_start>\t<delimiter>\t<flag>".
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(SLERMES, "tests", "watercup"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_shell_heredoc",
    os.path.join(DEV, "tools", "shell_heredoc.py"))
_hd = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_shell_heredoc"] = _hd
_spec.loader.exec_module(_hd)


def trace(command, body_start, delimiter, strip_tabs):
    """Trace the LIVE fn's branch ladder; return the finalized cup."""
    c = Cup("find_heredoc_close")
    result = None
    while True:
        newline = command.find("\n", body_start)
        have_nl = newline != -1
        c.branch(have_nl)                                # B1
        if have_nl:
            line = command[body_start:newline]
            after = newline + 1
        else:
            line = command[body_start:]
            after = len(command)
        if line.endswith("\r"):
            c.branch(True)                               # B2
            line = line[:-1]
        else:
            c.branch(False)
        if strip_tabs:
            c.branch(True)                               # B3
            candidate = line.lstrip("\t")
        else:
            c.branch(False)
            candidate = line
        c.valstr(candidate)                              # V1
        match = candidate == delimiter
        c.branch(match)                                  # B4
        if match:
            result = after
            break
        c.branch(not have_nl)                            # B5
        if not have_nl:
            result = None
            break
        body_start = after
    c.valstr("None" if result is None else str(result))  # V2
    return c.finalize()


def parse(inp):
    cmd, start, delim, flag = inp.split("\x1f", 3)
    return cmd, int(start), delim, flag == "1"


def run(inputs):
    return [trace(*parse(inp)) for inp in inputs]
