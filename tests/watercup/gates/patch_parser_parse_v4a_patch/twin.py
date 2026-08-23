"""twin for patch_parser_parse_v4a_patch.

Traces the LIVE tools/patch_parser.py::parse_v4a_patch with the SAME rung
order as the C harness (a traced mirror of src/tools/patch_parser.c's exact
control flow). The LIVE function itself is called for the real result and the
derived (op-count, paths, error-string) summary is asserted against the
mirrored state, so the trace can't silently drift from LIVE behavior.

Rung contract: see harness.c header comment.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_patch_parser", os.path.join(DEV, "tools", "patch_parser.py"))
_pp = ilu.module_from_spec(_spec)
sys.modules["wc_live_patch_parser"] = _pp
_spec.loader.exec_module(_pp)


def _c_marker(line, opkw):
    """Mirror of the C match_file_marker (agreed-subset semantics)."""
    if not line.startswith("***"):
        return None
    p = line[3:]
    while p.startswith(" "):
        p = p[1:]
    if not p.startswith(opkw):
        return None
    p = p[len(opkw):]
    while p.startswith(" "):
        p = p[1:]
    if not p.startswith("File:"):
        return None
    p = p[5:]
    while p.startswith(" "):
        p = p[1:]
    return p.strip(" \t\r\n")


def _c_move(line):
    """Mirror of the C match_move_marker."""
    if not line.startswith("***"):
        return None
    p = line[3:]
    while p.startswith(" "):
        p = p[1:]
    if not p.startswith("Move"):
        return None
    p = p[4:]
    while p.startswith(" "):
        p = p[1:]
    if not p.startswith("File:"):
        return None
    p = p[5:]
    while p.startswith(" "):
        p = p[1:]
    idx = p.find("->")
    if idx < 0:
        return None
    src = p[:idx].rstrip(" \t")
    dst = p[idx + 2:].lstrip(" \t").strip(" \t\r\n")
    return src, dst


def trace(patch_content):
    c = Cup("parse_v4a_patch")
    lines = [ln[:-1] if ln.endswith("\r") else ln for ln in patch_content.split("\n")]

    start_idx = None
    end_idx = None
    for i, line in enumerate(lines):
        if ("*** Begin Patch" in line) or ("***Begin Patch" in line):
            start_idx = i
        elif ("*** End Patch" in line) or ("***End Patch" in line):
            end_idx = i
            break
    c.branch(start_idx is not None)
    c.valstr(str(-1 if start_idx is None else start_idx))
    c.branch(end_idx is not None)
    c.valstr(str(len(lines) if end_idx is None else end_idx))

    lo = -1 if start_idx is None else start_idx
    hi = len(lines) if end_idx is None else end_idx

    # mirrored op/hunk state (C semantics)
    ops = []            # dicts: path,new_path,type,n_hunks
    cur = None
    pend = 0            # cur_hunk_nlines

    for i in range(lo + 1, hi):
        c.loop()
        line = lines[i]
        upd = _c_marker(line, "Update")
        add = _c_marker(line, "Add")
        dele = _c_marker(line, "Delete")
        mv = _c_move(line)

        if upd:
            c.branch(True)
            cond = cur is not None and pend > 0
            c.branch(cond)
            if cond:
                ops[cur]["n_hunks"] += 1
                pend = 0
            cur = len(ops)
            ops.append({"path": upd, "new_path": "", "type": 1, "n_hunks": 0})
            pend = 0
            c.valstr("U|" + upd)
        elif add:
            c.branch(False)
            c.branch(True)
            cond = cur is not None and pend > 0
            c.branch(cond)
            if cond:
                ops[cur]["n_hunks"] += 1
                pend = 0
            cur = len(ops)
            ops.append({"path": add, "new_path": "", "type": 0, "n_hunks": 0})
            pend = 0
            c.valstr("A|" + add)
        elif dele:
            c.branch(False)
            c.branch(False)
            c.branch(True)
            cond = cur is not None and pend > 0
            c.branch(cond)
            if cond:
                ops[cur]["n_hunks"] += 1
                pend = 0
            cur = len(ops)
            ops.append({"path": dele, "new_path": "", "type": 2, "n_hunks": 0})
            # closecond2 (into the fresh delete op) — mirrors C exactly
            cond2 = cur is not None and pend > 0
            c.branch(cond2)
            if cond2:
                ops[cur]["n_hunks"] += 1
                pend = 0
            cur = None
            pend = 0
            c.valstr("D|" + dele)
        elif mv:
            src, dst = mv
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(True)
            cond = cur is not None and pend > 0
            c.branch(cond)
            if cond:
                ops[cur]["n_hunks"] += 1
                pend = 0
            cur = len(ops)
            ops.append({"path": src, "new_path": dst, "type": 3, "n_hunks": 0})
            cond2 = cur is not None and pend > 0
            c.branch(cond2)
            if cond2:
                ops[cur]["n_hunks"] += 1
                pend = 0
            cur = None
            pend = 0
            c.valstr("M|%s|%s" % (src, dst))
        elif line.startswith("@@"):
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(True)
            cond = cur is not None and pend > 0
            c.branch(cond)
            if cond:
                ops[cur]["n_hunks"] += 1
                pend = 0
            a = line[2:]
            while a.startswith(" "):
                a = a[1:]
            b = a.find("@@")
            c.branch(b >= 0)
            hint = ""
            if b >= 0:
                hint = a[:b].rstrip(" \t")
            pend = 0
            c.valstr(hint)
        elif cur is not None and line != "":
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(True)
            if line.startswith("+"):
                c.branch(True)
                c.valstr("+" + line[1:])
                pend += 1
            elif line.startswith("-"):
                c.branch(False)
                c.branch(True)
                c.valstr("-" + line[1:])
                pend += 1
            elif line.startswith(" "):
                c.branch(False)
                c.branch(False)
                c.branch(True)
                c.valstr(" " + line[1:])
                pend += 1
            elif line.startswith("\\"):
                c.branch(False)
                c.branch(False)
                c.branch(False)
                c.branch(True)
            else:
                c.branch(False)
                c.branch(False)
                c.branch(False)
                c.branch(False)
                c.valstr(" " + line)
                pend += 1
        else:
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(False)
            c.branch(False)

    c.branch(cur is not None)
    if cur is not None:
        cond = pend > 0
        c.branch(cond)
        if cond:
            ops[cur]["n_hunks"] += 1
    errs = []
    c.branch(len(ops) == 0)
    if len(ops) == 0:
        c.valstr("EMPTY")
    else:
        errs = []
        for op in ops:
            c.loop()
            c.branch(op["path"] == "")
            if op["path"] == "":
                errs.append("Operation with empty file path")
            c.branch(op["type"] == 1 and op["n_hunks"] == 0)
            if op["type"] == 1 and op["n_hunks"] == 0:
                errs.append("UPDATE %r: no hunks found" % op["path"])
            c.branch(op["type"] == 3 and op["new_path"] == "")
            if op["type"] == 3 and op["new_path"] == "":
                errs.append(
                    "MOVE %r: missing destination path (expected 'src -> dst')"
                    % op["path"])
        c.branch(bool(errs))
        if errs:
            c.valstr("Parse error: " + "; ".join(errs))
        else:
            c.valstr("OK")
            c.valstr(str(len(ops)))

    # Cross-check the mirrored summary against the LIVE function's real result.
    live_ops, live_err = _pp.parse_v4a_patch(patch_content)
    live_summary = [
        (o.operation.value, o.file_path, o.new_path or "", len(o.hunks))
        for o in live_ops]
    expected_err = None if not errs else "Parse error: " + "; ".join(errs)
    assert expected_err == live_err, (
        "error drifted from LIVE: %r vs %r" % (expected_err, live_err))
    # On the error path LIVE returns ([], err) — mirror must agree it failed.
    mine = [({0: "add", 1: "update", 2: "delete", 3: "move"}[o["type"]],
             o["path"], o["new_path"], o["n_hunks"]) for o in ops]
    if live_err is not None:
        # On the error path LIVE returns ([], err); mirror must agree it failed.
        assert not live_ops and mine
    else:
        assert mine == live_summary, (
            "mirror drifted from LIVE: %r vs %r" % (mine, live_summary))

    return c.finalize()


def run(inputs):
    return [trace(p) for p in inputs]
