#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/update.py (gate w5_update_6d82c1).

The module's pm.network import is stubbed so the live module loads. Only the
four pure helpers (version_key, minor_of, best_in_minor, resolve_best) are
compared; all network/artifact helpers stay untouched.
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


pm = _mod("pm")
pm.__path__ = []
_mod("pm.network", retry_network=lambda fn: fn())

SRC = Q + "/pm/update.py"
spec = importlib.util.spec_from_file_location("w5_update_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_update_live"] = live
spec.loader.exec_module(live)


def key_json(k):
    return [[int(part[0]), part[1]] for part in k]


def resolved_json(r):
    return {
        "name": r.name,
        "locked": r.locked,
        "style": r.style,
        "version": r.version,
        "per_target": r.per_target,
        "reason": r.reason,
        "artifact_updates": r.artifact_updates,
        "changed": r.changed,
    }


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
    version = r.get("version", "")
    try:
        if func == "version_key":
            py = key_json(live.version_key(version))
        elif func == "minor_of":
            m = live.minor_of(version)
            py = list(m) if m is not None else None
        elif func == "best_in_minor":
            py = live.best_in_minor(r.get("versions") or [],
                                    (int(r.get("major", 0)), int(r.get("minor", 0))))
        elif func == "resolve_best":
            locked = r.get("locked") if isinstance(r.get("locked"), str) else None
            py = resolved_json(live.resolve_best(
                r.get("name", ""), r.get("latest") or {}, locked,
                r.get("style", "semver")))
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

print(f"ORACLE w5_update_6d82c1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
