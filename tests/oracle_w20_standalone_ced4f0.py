#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/signal/standalone.py (w20_standalone_ced4f0).

The live ``_send_signal`` is imported straight out of the quarry and awaited
against a fake httpx transport and a fake scheduler: the fixture scripts one
outcome per RPC attempt (ok / rpc error / transport exception), and this oracle
reports the payload the live function returned plus every RPC it made, every
``scheduler.acquire``/``feedback`` call and how the attachments were chunked.
The live ``signal_rate_limit`` helpers (rate-limit classification, Retry-After
extraction) are the real ones -- they are the module's, not this file's.

Media files are materialised under SLERMES_HOME, which the gate points at an
isolated temp dir: the existence check is a real filesystem check on both sides.
"""
import asyncio
import importlib
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

GATE = "w20_standalone_ced4f0"
MAX_ATTACHMENTS = 32


def home():
    return os.environ.get("SLERMES_HOME") or "."


def materialise(rel):
    path = "%s/%s" % (home(), rel)
    if not rel.startswith("missing/"):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "ab"):
            pass
    return path


class Sched:
    def __init__(self, estimates):
        self.estimates = list(estimates or [])
        self.acquired = []
        self.reported = []
        self.feedback_calls = []

    async def acquire(self, n):
        self.acquired.append(n)
        return 0.0

    async def report_rpc_duration(self, rpc_duration, n):
        self.reported.append(n)

    def feedback(self, retry_after, n):
        self.feedback_calls.append([retry_after, n])

    def estimate_wait(self, n):
        return self.estimates.pop(0) if self.estimates else 0.0

    def state(self):
        return {"tokens": 0}


def fake_httpx(state):
    class Resp:
        def __init__(self, outcome):
            self.outcome = outcome

        def raise_for_status(self):
            if "raise" in self.outcome:
                raise RuntimeError(str(self.outcome["raise"]))

        def json(self):
            if "error" in self.outcome:
                return {"error": self.outcome["error"]}
            return {"ok": True}

    class Client:
        def __init__(self, timeout=None):
            self.timeout = timeout

        async def __aenter__(self):
            return self

        async def __aexit__(self, *exc):
            return False

        async def post(self, url, json=None):
            payload = json or {}
            ident = str(payload.get("id") or "")
            params = payload.get("params") or {}
            atts = params.get("attachments") or []
            kind = "notice" if ident.startswith("notice") else "send"
            if kind == "notice":
                state["calls"].append({"kind": "notice", "batch": None, "n": len(atts)})
                return Resp({"ok": True})
            # A retry re-posts the same list object; a text-only RPC has no
            # attachments at all, and there is only ever one such batch.
            if atts:
                batch = state["batch_of"].setdefault(id(atts), len(state["batch_of"]))
            else:
                batch = 0
            state["calls"].append({"kind": "send", "batch": batch, "n": len(atts)})
            outcomes = state["post"]
            outcome = outcomes.pop(0) if outcomes else {"ok": True}
            return Resp(outcome)

    mod = types.ModuleType("httpx")
    mod.AsyncClient = Client
    return mod


def run_send(r):
    extra = r.get("extra") if isinstance(r.get("extra"), dict) else {}
    chat_id = r.get("chat_id") or ""
    media = []
    for entry in (r.get("media") or []):
        if isinstance(entry, list) and entry:
            media.append([materialise(entry[0]), bool(entry[1]) if len(entry) > 1 else False])
    state = {"post": list(r.get("post") or []), "calls": [], "batch_of": {}}
    sched = Sched(r.get("estimates"))

    live = importlib.import_module("plugins.platforms.signal.standalone")
    orig_get = live.rl.get_scheduler
    live.rl.get_scheduler = lambda: sched
    sys.modules["httpx"] = fake_httpx(state)
    try:
        if r.get("httpx_available") is False:
            # The live `import httpx` would fail; make that true here.
            sys.modules["httpx"] = None
        result = asyncio.run(live._send_signal(extra, chat_id, r.get("message") or "hello", media))
    finally:
        live.rl.get_scheduler = orig_get
        sys.modules.pop("httpx", None)

    # Both early returns fire before any chunking, so nothing was formed.
    if not extra.get("account") or r.get("httpx_available") is False:
        batches = []
    else:
        paths = [p for p, _ in media if os.path.exists(p)]
        batches = [paths[i:i + MAX_ATTACHMENTS] for i in range(0, len(paths), MAX_ATTACHMENTS)] or [[]]
    return {"result": result, "batches": batches, "calls": state["calls"],
            "feedback": sched.feedback_calls, "acquired": sched.acquired}


def expected(r):
    f = r.get("func")
    live = None
    if f == "send":
        return run_send(r)
    live = importlib.import_module("plugins.platforms.signal.standalone")
    if f == "is_rate_limit":
        return bool(live.rl._is_signal_rate_limit_error(r.get("error")))
    if f == "retry_after":
        return live.rl._extract_retry_after_seconds(r.get("error"))
    if f == "send_timeout":
        return float(live.rl._signal_send_timeout(int(r.get("attachments") or 0)))
    if f == "chat_id":
        chat = r.get("chat_id") or ""
        return "group:***" if str(chat).startswith("group:") else chat
    raise AssertionError("unknown func %r" % (f,))


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
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        try:
            exp = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
