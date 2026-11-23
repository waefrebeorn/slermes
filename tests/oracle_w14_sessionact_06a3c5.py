#!/usr/bin/env python3
"""Differential oracle for gateway/session_active_list.py (gate w14_sessionact_06a3c5).

The LIVE _status / active_list run against injected fake entries; the authority
and event-stream readers are registered in sys.modules so no live I/O occurs.
"""
import asyncio
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/gateway/session_active_list.py"
spec = importlib.util.spec_from_file_location("w14_sal_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


class Entry:
    def __init__(self, d):
        self.row = d.get("row") or {}
        self.closed = bool(d.get("closed", False))
        self.session_id = d.get("session_id", "")
        self.pending = [{"status": s} for s in d.get("pending", [])]
        asked = bool(d.get("asked", False))
        self.live = SimpleNamespace(
            controls=SimpleNamespace(snapshot=lambda sid, gen: asked),
            event_stream=object())
        self.handle = SimpleNamespace(
            execution_state=d.get("execution_state", "idle"),
            execution_generation=0,
            ref=SimpleNamespace(session_id=self.session_id))


def register_fakes(entries):
    fake = types.ModuleType("gateway.session_live_listing")

    async def live_entries(connection, actor):
        return tuple(entries)

    fake.live_entries = live_entries
    sys.modules["gateway.session_live_listing"] = fake
    if "gateway" in sys.modules:
        setattr(sys.modules["gateway"], "session_live_listing", fake)

    fake_sr = types.ModuleType("gateway.session_server_requests")
    fake_sr.open_requests = lambda stream, sid: False
    sys.modules["gateway.session_server_requests"] = fake_sr
    if "gateway" in sys.modules:
        setattr(sys.modules["gateway"], "session_server_requests", fake_sr)


def want_for(r):
    func = r.get("func")
    if func == "status":
        e = Entry(r)
        return live._status(e.live, e.handle, e.pending)
    if func == "active_list":
        entries = [Entry(d) for d in r.get("entries") or []]
        register_fakes(entries)
        conn = SimpleNamespace(
            authority=object(),
            actor=SimpleNamespace(capabilities=set(r.get("capabilities") or [])))
        try:
            result = asyncio.run(live.active_list(conn, None, r.get("params") or {}))
            return {"ok": True, "error": None, "sessions": result["sessions"]}
        except live.AuthorityRefusal as exc:
            return {"ok": False, "error": str(exc), "sessions": None}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    want = want_for(r)
    if want is None:
        continue
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w14_sessionact_06a3c5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
