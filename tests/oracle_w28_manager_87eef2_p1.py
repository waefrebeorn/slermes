#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/manager.py (gate w28_manager_87eef2_p1).

The live module is imported as a package module; the only seam substituted is
``_normalize_path`` (Path.expanduser().resolve()), which is an identity over the
fixture's already-normalized paths.  Everything compared -- validation
messages, the sha256 project id, the ref/index/ledger/meta path shapes and the
store-exclusion git argv -- is computed by the live code.
"""
import json
import sys
from pathlib import PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = "C:/Users/eman5/slermes"
QUARRY = "C:/Users/eman5/hermes-agent"
GATE = "w28_manager_87eef2_p1"

if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

import agent.checkpoints.manager as live  # noqa: E402

live._normalize_path = lambda p: PurePosixPath(str(p))


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


def validate(req):
    return live._validate_commit_hash(s(req, "commit_hash"))


def gitlinks(req):
    recs = req.get("records")
    recs = recs if isinstance(recs, list) else []
    z = "".join((x if isinstance(x, str) else "") + "\x00" for x in recs)
    return live._gitlink_paths(z)


def note(req):
    recs = req.get("nested_repos")
    return live._uncaptured_note(recs if isinstance(recs, list) else [])


def project_hash(req):
    return live._project_hash(s(req, "abs_path"))


def store_path(req):
    return str(live._store_path(PurePosixPath(s(req, "base"))))


def index_path(req):
    return (live._index_path(PurePosixPath(s(req, "store")), s(req, "dir_hash"))).as_posix()


def ledger_path(req):
    return (live._ledger_path(PurePosixPath(s(req, "store")), s(req, "dir_hash"))).as_posix()


def meta_path(req):
    return (live._project_meta_path(PurePosixPath(s(req, "store")), s(req, "dir_hash"))).as_posix()


def pathspecs(req):
    return live._store_pathspecs(PurePosixPath(s(req, "base")), s(req, "working_dir"))


def exclude_args(req):
    return live._exclude_store_args(PurePosixPath(s(req, "base")), s(req, "working_dir"))


def stage_args(req):
    return live._stage_all_args(PurePosixPath(s(req, "base")), s(req, "working_dir"))


FUNCS = {
    "validate_commit_hash": validate,
    "ref_name": lambda r: live._ref_name(s(r, "dir_hash")),
    "gitlink_paths": gitlinks,
    "uncaptured_note": note,
    "project_hash": project_hash,
    "store_path": store_path,
    "index_path": index_path,
    "ledger_path": ledger_path,
    "meta_path": meta_path,
    "store_pathspecs": pathspecs,
    "exclude_store_args": exclude_args,
    "stage_all_args": stage_args,
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
        fn = FUNCS[r["func"]]
        try:
            py = fn(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
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
