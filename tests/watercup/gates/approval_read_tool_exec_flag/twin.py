"""twin for approval_read_tool_exec_flag.

Imports the LIVE tools/approval.py::_read_tool_exec_flag (plus its flag
tables) and traces the SAME token ladder as the C harness. The V result is
the LIVE function's authoritative return; the branch events mirror the C
shim's decisions (which agree with LIVE on every input in the walk).
"""
import os
import sys
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_approval",
    os.path.join(DEV, "tools", "approval.py"))
_ap = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_approval"] = _ap
_spec.loader.exec_module(_ap)

EXEC_FLAGS = _ap._READ_TOOL_EXEC_FLAGS
MAN_ATTACHED = ("-P", "-H")


def run(inputs):
    cups = []
    for raw in inputs:
        tool, args_str = raw.split(" ", 1)
        args = args_str.split(" ") if args_str else []

        c = Cup("read_tool_exec_flag")
        result = None
        flags = EXEC_FLAGS.get(tool, set())
        is_man = tool == "man"

        i = 0
        while i < len(args):
            c.loop()
            token = args[i]
            is_sep = token == "--"
            c.branch(is_sep)                                   # B1
            if is_sep:
                break
            if "=" in token:
                option, payload = token.split("=", 1)
            else:
                option, payload = token, None
            man_attached = (is_man and token.startswith(MAN_ATTACHED)
                            and len(token) > 2)
            c.branch(man_attached)                             # B2
            if man_attached:
                option, payload = token[:2], token[2:]
                matched = True
            else:
                matched = option in flags
            c.branch(matched)                                  # B3
            if matched:
                if payload is None and i + 1 < len(args):
                    payload = args[i + 1]
                have = bool(payload)
                c.branch(have)                                 # B4
                if have:
                    result = (option, payload)
                    break
            else:
                c.branch(False)                                # B4 not reached
            i += 1

        res = "%s\t%s" % result if result else ""
        live = _ap._read_tool_exec_flag(tool, args)
        live_res = "%s\t%s" % live if live else ""
        assert res == live_res, "mirror drift: %r vs LIVE %r" % (res, live_res)
        c.valstr(res)
        cups.append(c.finalize())
    return cups
