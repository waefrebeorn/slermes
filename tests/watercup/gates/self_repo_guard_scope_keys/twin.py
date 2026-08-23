"""twin for self_repo_guard_scope_keys.

Imports the LIVE tools/self_repo_guard.py::_scope_keys helpers and traces the
SAME branch ladder as the C harness (see harness.c for rung order).

The live function's control structure is mirrored rung-for-rung here; the
shell-context semantics (quote stack, $(, backtick, paren) come from LIVE code.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_self_repo_guard",
    os.path.join(DEV, "tools", "self_repo_guard.py"))
_srg = ilu.module_from_spec(_spec)
sys.modules["wc_live_self_repo_guard"] = _srg
if _spec.loader is not None:
    _spec.loader.exec_module(_srg)


def trace(command, starts):
    c = Cup("srg_scope_keys")
    contexts = [(0, -1, None)]  # (kind, opener, quote); kinds 0=root 1=( 2=$(
    scopes = []
    cursor = 0

    def set_quote(i, q):
        k, o, _ = contexts[i]
        contexts[i] = (k, o, q)

    for start in sorted(set(starts)):
        while True:
            have = cursor < start
            c.branch(have)
            if not have:
                break
            ctx = len(contexts) - 1
            quote = contexts[ctx][2]
            ch = command[cursor]
            if quote == "'":
                c.branch(True)
                c.branch(ch == "'")
                if ch == "'":
                    set_quote(ctx, None)
                cursor += 1
                continue
            c.branch(False)
            if quote == '"':
                c.branch(True)
                c.branch(ch == "\\" and cursor + 1 < start)
                if ch == "\\" and cursor + 1 < start:
                    cursor += 2
                    continue
                c.branch(ch == '"')
                if ch == '"':
                    set_quote(ctx, None)
                    cursor += 1
                    continue
                c.branch(command.startswith("$(", cursor))
                if command.startswith("$(", cursor):
                    contexts.append((2, cursor, None))
                    cursor += 2
                    continue
                c.branch(ch == "`")
                if ch == "`":
                    contexts.append((3, cursor, None))
                cursor += 1
                continue
            c.branch(False)
            c.branch(ch in ("'", '"'))
            if ch in ("'", '"'):
                set_quote(ctx, ch)
                cursor += 1
                continue
            c.branch(ch == "\\" and cursor + 1 < start)
            if ch == "\\" and cursor + 1 < start:
                cursor += 2
                continue
            c.branch(command.startswith("$(", cursor))
            if command.startswith("$(", cursor):
                contexts.append((2, cursor, None))
                cursor += 2
                continue
            c.branch(ch == "(")
            if ch == "(":
                contexts.append((1, cursor, None))
                cursor += 1
                continue
            c.branch(ch == ")" and len(contexts) > 1 and contexts[-1][0] in (1, 2))
            if ch == ")" and len(contexts) > 1 and contexts[-1][0] in (1, 2):
                contexts.pop()
                cursor += 1
                continue
            c.branch(ch == "`")
            if ch == "`":
                c.branch(len(contexts) > 1 and contexts[-1][0] == 3)
                if len(contexts) > 1 and contexts[-1][0] == 3:
                    contexts.pop()
                else:
                    contexts.append((3, cursor, None))
            cursor += 1
        scope = ",".join(str(item[1]) for item in contexts[1:])
        c.valstr(scope)
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        start_csv, command = raw.split("|", 1)
        starts = [int(t) for t in start_csv.split(",") if t != ""]
        cups.append(trace(command, starts))
    return cups
