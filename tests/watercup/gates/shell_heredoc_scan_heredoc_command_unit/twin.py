"""twin for shell_heredoc_scan_heredoc_command_unit.

Imports LIVE tools/shell_heredoc.py::_scan_heredoc_command_unit and traces the
SAME rung ladder as the C harness (mirroring src/tools/shell_heredoc.c).
The control ladder is replicated rung-for-rung; the V event carries the REAL
LIVE fn's returned tuple (end, specs, unknown_operator, has_list_operator)
canonically encoded, cross-checking the replicated ladder against LIVE.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_shell_heredoc",
                                   os.path.join(DEV, "tools", "shell_heredoc.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_shell_heredoc"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for command in inputs:
        c = Cup("_scan_heredoc_command_unit")
        n = len(command)
        cursor = 0
        quote = None
        comment = False
        specs_acc = []
        unknown = False
        has_list = False

        done = False
        while cursor < n and not done:
            c.loop()
            ch = command[cursor]
            if comment:
                c.branch(True)                        # B1
                if ch == "\n":
                    c.branch(True)                    # B2
                    done = True
                    break
                c.branch(False)
                cursor += 1
                continue
            c.branch(False)
            if quote is not None:
                c.branch(True)                        # B3
                if quote in {'"', "`"} and ch == "\\" and cursor + 1 < n:
                    cursor += 2
                    continue
                if ch == quote:
                    quote = None
                cursor += 1
                continue
            c.branch(False)
            if ch == "\\" and cursor + 1 < n:
                c.branch(True)                        # B4
                cursor += 2
                continue
            c.branch(False)
            if ch in "'\"`":
                c.branch(True)                        # B5
                quote = ch
                cursor += 1
                continue
            c.branch(False)
            if ch == "#":
                previous = command[cursor - 1] if cursor > 0 else ""
                if cursor == 0 or previous.isspace() or previous in ";&|()":
                    c.branch(True)                    # B6
                    comment = True
                    cursor += 1
                    continue
                c.branch(False)
            else:
                c.branch(False)
            if ch == "\n":
                c.branch(True)                        # B7
                done = True
                break
            c.branch(False)
            if command.startswith("<<<", cursor):
                c.branch(True)                        # B8
                cursor += 3
                continue
            c.branch(False)
            if command.startswith("<<", cursor):
                c.branch(True)                        # B9
                parsed = mod._parse_heredoc_operator(command, cursor)
                if parsed is None:
                    c.branch(True)                    # B10
                    unknown = True
                    cursor += 2
                    continue
                c.branch(False)
                cursor, delimiter, strip_tabs, quoted = parsed
                specs_acc.append("%s,%d,%d;" % (delimiter, 1 if strip_tabs else 0,
                                                1 if quoted else 0))
                continue
            c.branch(False)
            if ch in ";|&":
                has_list = True
            cursor += 1

        # V event: REAL LIVE fn result, canonical encoding
        live_end, live_specs, live_unknown, live_has_list = \
            mod._scan_heredoc_command_unit(command, 0)
        res = "%d|%s|%d|%d" % (
            live_end,
            "".join("%s,%d,%d;" % (d, 1 if st else 0, 1 if q else 0)
                    for (d, st, q) in live_specs),
            1 if live_unknown else 0,
            1 if live_has_list else 0,
        )
        c.valstr(res)
        cups.append(c.finalize())
    return cups
