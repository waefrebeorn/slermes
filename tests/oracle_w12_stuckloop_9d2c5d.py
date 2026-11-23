#!/usr/bin/env python3
"""Differential oracle for gateway/stuck_loop_counts.py (w12_stuckloop_9d2c5d).

Calls the LIVE functions with an in-memory `utils` (read_json_or_empty /
atomic_json_write) and a FakePath whose unlink() is observable, so the real
read/mutate/write decisions are exercised without touching the filesystem.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

_STORE = {}
_UNLINKED = set()


class FakePath:
    def __init__(self, key):
        self.key = key

    def __hash__(self):
        return hash(self.key)

    def __eq__(self, other):
        return isinstance(other, FakePath) and self.key == other.key

    def unlink(self, missing_ok=False):
        _UNLINKED.add(self.key)
        _STORE.pop(self.key, None)


def _read(path):
    v = _STORE.get(path)
    return dict(v) if isinstance(v, dict) else {}


def _write(path, data, *, indent=None, **kw):
    _STORE[path] = dict(data)


utils = types.ModuleType("utils")
utils.read_json_or_empty = _read
utils.atomic_json_write = _write
sys.modules["utils"] = utils

SRC = Q + "/gateway/stuck_loop_counts.py"
spec = importlib.util.spec_from_file_location("stuckloop_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["stuckloop_live"] = live
spec.loader.exec_module(live)


def reveal(v):
    if v is None or isinstance(v, (str, int, float, bool)):
        return v
    if isinstance(v, dict):
        return {str(k): reveal(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [reveal(x) for x in v]
    return str(v)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        p = FakePath("case")
        _STORE.clear()
        _UNLINKED.clear()
        if func == "increment":
            _STORE[p] = dict(r.get("counts") or {})
            live.increment_restart_failure_counts(p, set(r.get("keys") or []))
            py = reveal(_STORE.get(p, {}))
        elif func == "clear":
            init = dict(r.get("counts") or {})
            _STORE[p] = init
            key = r.get("session_key")
            live.clear_restart_failure_count(p, key)
            changed = key in init
            unlinked = p.key in _UNLINKED
            final = {} if unlinked else dict(_STORE.get(p, {}))
            py = {"counts": reveal(final), "changed": changed, "unlink": unlinked}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w12_stuckloop_9d2c5d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
