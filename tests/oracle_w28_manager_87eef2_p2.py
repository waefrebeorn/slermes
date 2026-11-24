#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/manager.py (gate w28_manager_87eef2_p2).

Seams substituted (never ported): git subprocess output, the store's HEAD
probe and the on-disk ledger/project files -- each one is a fixed fixture
input.  Everything compared is live code: the __init__ clamps, the shortstat
and log-line parsers, the manifest parsers, the ledger retention cap and
format_checkpoint_list.
"""
import json
import sys
import tempfile
from pathlib import Path, PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
GATE = "w28_manager_87eef2_p2"

if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

import agent.checkpoints.manager as live  # noqa: E402


class VP(PurePosixPath):
    """A path whose HEAD probe is the fixture's flag."""
    head = True

    def exists(self):
        return VP.head


CONFIG_KEYS = ("enabled", "max_snapshots", "max_total_size_mb", "max_file_size_mb")


def cfg_of(req):
    return {k: req[k] for k in CONFIG_KEYS if k in req}


def brief(req, limit=240):
    """Short, readable view of a fixture case for mismatch lines."""
    try:
        text = json.dumps({k: v for k, v in req.items() if k != "out"})
    except (TypeError, ValueError):
        text = repr(req)
    return text if len(text) <= limit else text[:limit] + "..."


def coerce_init(req):
    mgr = live.CheckpointManager(**cfg_of(req))
    return {
        "enabled": bool(mgr.enabled),
        "max_snapshots": mgr.max_snapshots,
        "max_total_size_mb": mgr.max_total_size_mb,
        "max_file_size_mb": mgr.max_file_size_mb,
    }


def parse_shortstat(req):
    entry = {"files_changed": 0, "insertions": 0, "deletions": 0}
    line = req.get("stat_line")
    live.CheckpointManager._parse_shortstat(line if isinstance(line, str) else "", entry)
    return entry


def list_checkpoints(req):
    log = req.get("log") if isinstance(req.get("log"), str) else ""
    stats = req.get("stats") if isinstance(req.get("stats"), dict) else {}

    def fake(args, store, working_dir, **kw):
        if args and args[0] == "log":
            return (True, log, "")
        if args and args[0] == "diff" and "--shortstat" in args:
            h = str(args[2]).split("~")[0]
            st = stats.get(h)
            if not isinstance(st, dict):
                return (False, "", "bad revision")
            return (bool(st.get("ok")), st.get("out") or "", "")
        return (False, "", "unexpected git call")

    live._store_path = lambda base=None: VP("/hermes/checkpoints/store")
    live._project_hash = lambda p: "abcdef0123456789"
    live._index_path = lambda st, h: VP("/hermes/checkpoints/store/indexes/x")
    live._normalize_path = lambda p: PurePosixPath(str(p))
    live._run_git = fake
    VP.head = bool(req.get("has_head"))
    mgr = live.CheckpointManager()
    mgr.max_snapshots = req.get("max_snapshots", 20)
    return mgr.list_checkpoints("/work/proj")


def ledger_load(req):
    text = req.get("text") if isinstance(req.get("text"), str) else ""
    root = Path(tempfile.mkdtemp())
    (root / "ledgers").mkdir()
    (root / "ledgers" / "h.json").write_bytes(text.encode("utf-8"))
    return live._load_ledger(root, "h")


def ledger_trim(req):
    ledger = req.get("ledger") if isinstance(req.get("ledger"), dict) else {}
    root = Path(tempfile.mkdtemp())
    (root / "ledgers").mkdir()
    live._save_ledger(root, "h", dict(ledger))
    return json.loads((root / "ledgers" / "h.json").read_text(encoding="utf-8"))


def list_projects(req):
    entries = req.get("entries") if isinstance(req.get("entries"), list) else []
    root = Path(tempfile.mkdtemp())
    (root / "projects").mkdir()
    for e in entries:
        if not isinstance(e, dict):
            continue
        name = str(e.get("hash", "")) + ".json"
        (root / "projects" / name).write_bytes(str(e.get("text", "")).encode("utf-8"))
    metas = live._list_projects(root)
    return sorted(metas, key=lambda m: m["_hash"])


def format_list(req):
    cps = req.get("checkpoints")
    cps = cps if isinstance(cps, list) else []
    return live.format_checkpoint_list(cps, req.get("directory") or "")


FUNCS = {
    "coerce_init": coerce_init,
    "parse_shortstat": parse_shortstat,
    "list_checkpoints": list_checkpoints,
    "ledger_load": ledger_load,
    "ledger_trim": ledger_trim,
    "list_projects": list_projects,
    "format_list": format_list,
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
