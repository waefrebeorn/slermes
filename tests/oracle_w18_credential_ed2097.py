#!/usr/bin/env python3
"""Oracle for gate w18_credential_ed2097 (agent/credential_auth_recovery.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("agent.credential_auth_recovery", "agent/credential_auth_recovery.py")

def _counts(rows):
    return {(row[0], row[1]): dict(row[2]) for row in (rows or [])}

def _rows(counts):
    return [[k[0], k[1], v] for k, v in counts.items()]

def _entries(rows):
    return [SimpleNamespace(**e) for e in (rows or [])]

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
    if func == "nous_refreshed":
        agent = SimpleNamespace(_auth_pool_refresh_counts=_counts(r.get("counts")))
        py = live.nous_pool_refreshed_this_turn(agent)
    elif func == "entry_for":
        pool = SimpleNamespace(entries=lambda: _entries(r.get("entries")))
        found = live._entry_for(pool, r.get("credential_id"), r.get("api_key_hint"))
        py = None if found is None else {"id": found.id, "runtime_api_key": getattr(found, "runtime_api_key", None)}
    elif func == "episode":
        counts = _counts(r.get("counts"))
        agent = SimpleNamespace(_auth_pool_refresh_counts=counts)
        live.time.monotonic = lambda: r.get("now", 0)
        ep = live._episode(agent, r.get("provider"), r.get("entry_id"))
        py = {"counts": _rows(counts), "episode": ep}
    elif func == "constants":
        py = {
            "TRANSIENT_AUTH_BACKOFF_S": list(live.TRANSIENT_AUTH_BACKOFF_S),
            "_MAX_AUTH_REFRESH_ATTEMPTS": live._MAX_AUTH_REFRESH_ATTEMPTS,
            "_ACTIVITY_TOUCH_S": live._ACTIVITY_TOUCH_S,
            "_EPISODE_IDLE_S": live._EPISODE_IDLE_S,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_credential_ed2097: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
