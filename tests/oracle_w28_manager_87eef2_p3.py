#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/manager.py (gate w28_manager_87eef2_p3).

Seams substituted: git (every call answers from the fixture), the file hashes
the safe-restore plan compares, the store HEAD probe and stat() sizes.  The
decisions compared -- the error precedence, the restore/skip partition, the
diff result shape, session_diff's baseline pick and the size-cap predicate --
are live code.
"""
import json
import sys
import tempfile
from pathlib import Path, PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
GATE = "w28_manager_87eef2_p3"

if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

import agent.checkpoints.manager as live  # noqa: E402


class VP(PurePosixPath):
    """A path whose HEAD probe is the fixture's flag."""
    head = True

    def exists(self):
        return VP.head


live.Path = VP
live._normalize_path = lambda p: PurePosixPath(str(p))
live._store_path = lambda base=None: VP("/hermes/checkpoints/store")
live._project_hash = lambda p: "abcdef0123456789"
live._index_path = lambda st, h: VP("/hermes/checkpoints/store/indexes/x")

MGR = live.CheckpointManager()


def s(req, key, default=""):
    v = req.get(key)
    return v if isinstance(v, str) else default


def brief(req, limit=240):
    """Short, readable view of a fixture case for mismatch lines."""
    try:
        text = json.dumps({k: v for k, v in req.items() if k != "out"})
    except (TypeError, ValueError):
        text = repr(req)
    return text if len(text) <= limit else text[:limit] + "..."


def safe_restore_plan(req):
    names = req.get("names")
    names = names if isinstance(names, list) else []
    names_z = "".join(str(n) + "\x00" for n in names)
    ledger = req.get("ledger")
    ledger = ledger if isinstance(ledger, dict) else {}
    current = req.get("current")
    current = current if isinstance(current, dict) else {}
    ok = bool(req.get("ok", True))
    err = s(req, "err")

    def fake(args, store, working_dir, **kw):
        if args and args[0] == "diff" and "--name-only" in args:
            return (ok, names_z, err)
        return (True, "", "")

    live._run_git = fake
    live._load_ledger = lambda store, h: ledger
    live._hash_file = lambda p: current.get(str(p))
    VP.head = bool(req.get("has_head", True))
    MGR._ledger_key = lambda p: "L"
    return MGR._safe_restore_plan(s(req, "dir"), s(req, "commit"))


def diff_result(req):
    def fake(args, store, working_dir, **kw):
        if args and args[0] == "cat-file":
            return (bool(req.get("cat_ok", True)), "", "")
        if args and args[0] == "diff" and "--stat" in args:
            return (bool(req.get("stat_ok", True)), s(req, "stat"), "")
        if args and args[0] == "diff":
            return (bool(req.get("diff_ok", True)), s(req, "diff"), "")
        return (True, "", "")

    live._run_git = fake
    VP.head = bool(req.get("has_head", True))
    return MGR._diff(s(req, "dir"), s(req, "commit"))


def session_diff(req):
    cps = req.get("checkpoints")
    cps = cps if isinstance(cps, list) else []
    diff = req.get("diff")
    MGR.list_checkpoints = lambda wd: list(cps)
    MGR.diff = lambda wd, h: dict(diff) if isinstance(diff, dict) else {}
    return MGR.session_diff(s(req, "dir"))


def exceeds_size_cap(req):
    root = Path(tempfile.mkdtemp())
    name = "f.bin" if req.get("stat_ok", True) else "missing.bin"
    target = root / name
    if req.get("stat_ok", True):
        with open(target, "wb") as fh:
            fh.truncate(int(req.get("size") or 0))
    MGR.max_file_size_mb = int(req.get("cap_mb") or 0)
    return MGR._exceeds_size_cap(target)


FUNCS = {
    "safe_restore_plan": safe_restore_plan,
    "diff_result": diff_result,
    "session_diff": session_diff,
    "exceeds_size_cap": exceeds_size_cap,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in FUNCS:
            continue
        cases += 1
        try:
            py = FUNCS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s live-error=%r" % (r.get("func"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s inp=%s py=%r c=%r" % (r.get("func"), brief(r), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
