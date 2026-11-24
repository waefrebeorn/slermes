#!/usr/bin/env python3
"""Differential oracle for agent/lsp/sync_diagnostics.py (gate w25_syncdiagno_87bd05).

The live module is imported and driven through a fake LSP client: ``asyncio`` is
real, ``request`` runs for real, and only ``_send_request_with_retry`` is pinned
to the fixture's per-command reply (or its TimeoutError / LSPRequestError /
LSPProtocolError).  ``_from_tsserver`` and ``supported`` are called directly, so
what is compared is the support gate, the tsserver -> LSP mapping and the
failure ladder -- no server, socket or clock.
"""
import asyncio
import json
import sys
import time
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_syncdiagno_87bd05"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.lsp import sync_diagnostics as live  # noqa: E402
from agent.lsp.protocol import LSPProtocolError, LSPRequestError  # noqa: E402

WANTED = {"supported", "from_tsserver", "request", "constants"}


class FakeClient:
    """The transport seam: the fixture's replies, in command order."""

    def __init__(self, server_id, unsupported, script):
        self.server_id = server_id
        self.script = list(script)
        if unsupported:
            setattr(self, live._UNSUPPORTED, True)
        self.calls = []

    def _flag(self):
        return bool(getattr(self, live._UNSUPPORTED, False))

    async def _send_request_with_retry(self, method, params, timeout=None):
        self.calls.append({"method": method, "params": params, "timeout": timeout})
        entry = self.script.pop(0) if self.script else {"kind": "ok", "reply": None}
        kind = entry.get("kind", "ok") if isinstance(entry, dict) else "ok"
        if kind == "timeout":
            raise TimeoutError()
        if kind == "request_error":
            raise LSPRequestError(-32601, "unsupported command")
        if kind == "protocol_error":
            raise LSPProtocolError("connection lost")
        return entry.get("reply") if isinstance(entry, dict) else None


def _truthy(value):
    return bool(value)


def supported(req):
    client = FakeClient(req.get("server_id"), _truthy(req.get("unsupported")), [])
    return live.supported(client)


def from_tsserver(req):
    return live._from_tsserver(req.get("item"))


def request(req):
    spec = req.get("req")
    spec = spec if isinstance(spec, dict) else {}
    replies = spec.get("replies")
    script = replies if isinstance(replies, list) else [{"kind": "ok", "reply": None}]
    timeout = spec.get("timeout")
    timeout = float(timeout) if isinstance(timeout, (int, float)) and not isinstance(timeout, bool) else 0.0
    client = FakeClient(spec.get("server_id"), _truthy(spec.get("unsupported")), script)
    try:
        diagnostics = asyncio.run(live.request(client, "/src/x.ts", timeout=timeout))
    except TimeoutError as exc:
        return {"status": "timeout", "message": str(exc), "unsupported": client._flag()}
    if diagnostics is None:
        return {"status": "none", "unsupported": client._flag()}
    return {"status": "diagnostics", "diagnostics": diagnostics, "unsupported": client._flag()}


def constants():
    return {
        "ts_servers": sorted(live._TS_SERVERS),
        "ts_commands": list(live._TS_COMMANDS),
        "ts_severity": dict(live._TS_SEVERITY),
        "unsupported_attr": live._UNSUPPORTED,
    }


def expected(r):
    f = r.get("func")
    if f == "supported":
        return supported(r)
    if f == "from_tsserver":
        return from_tsserver(r)
    if f == "request":
        return request(r)
    if f == "constants":
        return constants()
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
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
