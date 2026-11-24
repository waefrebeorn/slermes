#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_idle_proof.py (gate w21_webserveri_184aef).

The live module is imported from the quarry as-is. `idle_proof` takes its two
probes as arguments, so each case is answered by a lambda returning the
fixture's value -- the same call the Desktop pool's retirement fence makes with
its real probes. `pending_human_input` reads two ledgers; both are the database
seam, so each case installs a stand-in `tui_gateway.server_requests` and
`tools.approval.pending_gateway_approval_count` for the duration of the call
(and a raising probe for the "unreadable ledger" case), then restores the real
modules.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_server_idle_proof.py"
GATE = "w21_webserveri_184aef"
_SEAM_MODULES = ("tui_gateway", "tools.approval")

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("web_server_idle_proof_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["web_server_idle_proof_live"] = live
_spec.loader.exec_module(live)


def _count(value):
    def probe():
        if value is None:
            raise RuntimeError("ledger unreadable")
        return int(value)
    return probe


def pending_live(r):
    """`pending_human_input()` with both ledgers supplied by the fixture."""
    unreadable = bool(r.get("unavailable"))
    server = types.ModuleType("tui_gateway")
    server.server_requests = types.SimpleNamespace(
        open_request_count=_count(None if unreadable else r.get("server_requests", 0)))
    approval = types.ModuleType("tools.approval")
    approval.pending_gateway_approval_count = _count(r.get("approvals", 0))
    saved = {name: sys.modules.get(name) for name in _SEAM_MODULES}
    sys.modules["tui_gateway"] = server
    sys.modules["tools.approval"] = approval
    try:
        return live.pending_human_input()
    finally:
        for name, module in saved.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module


def expected(r):
    f = r.get("func")
    if f == "idle_proof":
        turn = r.get("turn")
        pending = r.get("pending")
        return live.idle_proof(turn_probe=lambda: turn, input_probe=lambda: pending)
    if f == "pending_human_input":
        return pending_live(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"idle_proof", "pending_human_input"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
