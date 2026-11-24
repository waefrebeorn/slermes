#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_cli_commands.py (gate w25_gatewaycli_8b715b).

The live handlers run for real; their seams are fixed answers -- the gateway
connection (its rollback.session_diff payload, its handoff rows), the two-phase
handoff wait, the terminal printers and the i18n lookup.  What is compared is the
decision those answers make: which branch /diff session takes, where /handoff
parses its platform, and whether run() answers the command here at all.
"""
import json
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_gatewaycli_8b715b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.cli_commands_session_tools as tools  # noqa: E402
import hermes_cli.gateway_side as gateway_side  # noqa: E402
from hermes_cli import gateway_cli_commands as live  # noqa: E402
from hermes_cli.gateway_client import GatewayClientError  # noqa: E402

# ``run``'s refusal path prints through ``cli._cprint``, its terminal seam.  The
# real CLI package is not imported here: one frozen printer stands in for it.
cli_module = sys.modules.get("cli")
if cli_module is None:
    cli_module = types.ModuleType("cli")
    sys.modules["cli"] = cli_module
if not hasattr(cli_module, "_cprint"):
    cli_module._cprint = lambda text: None

WANTED = {"diff", "handoff", "run", "constants"}


class Connection:
    """The gateway connection with one fixed answer per verb."""

    def __init__(self, answer):
        self.answer = answer
        self.calls = []

    def call(self, verb, **kwargs):
        self.calls.append([verb, kwargs])
        return self.answer


def diff(req):
    saved = (tools._t, tools._lines, tools._pr, tools.print_session_diff)
    printed = []
    shown = []
    tools._t = lambda key, **kwargs: key
    tools._lines = lambda text: [text]
    tools._pr = lambda *args: printed.append(list(args))
    tools.print_session_diff = lambda cli, result, stat_only=False: shown.append(
        {"stat_only": stat_only, "result": result})
    try:
        handled = live._diff(SimpleNamespace(session_id="sess-1"), Connection(req.get("result")),
                             req.get("cmd") or "")
    finally:
        tools._t, tools._lines, tools._pr, tools.print_session_diff = saved
    if not handled:
        return {"handled": handled, "branch": "none", "stat_only": None}
    if printed:
        return {"handled": handled, "branch": "not_enabled", "stat_only": None}
    return {"handled": handled, "branch": "session_diff", "stat_only": shown[0]["stat_only"]}


def handoff(req):
    saved = (tools._t, tools._cp)
    cp = []
    tools._t = lambda key, **kwargs: key
    tools._cp = lambda *args: cp.append(list(args))
    cli = SimpleNamespace(session_id="abcdef123456",
                          _agent_running=req.get("agent_running", False),
                          _handoff_wait=lambda *a, **k: req.get("kept", False))
    try:
        kept = live._handoff(cli, Connection({"home_name": req.get("home_name")}),
                             req.get("cmd") or "")
    finally:
        tools._t, tools._cp = saved
    blob = " ".join(str(arg) for group in cp for arg in group)
    if "handoff.usage" in blob:
        result = "USAGE"
    elif "shared.agent_busy" in blob:
        result = "BUSY"
    elif kept is live.EXIT_SESSION:
        result = "EXIT"
    else:
        result = "KEPT"
    return {"result": result,
            "platform": tools._command_arg(req.get("cmd") or "", lower=True),
            "lines": sum(len(group) for group in cp)}


def run(req):
    saved = (dict(live._ROUTES), gateway_side.connection_for, cli_module._cprint)
    printed = []
    route = req.get("route")

    def answer(cli, connection, cmd):
        if route == "raise":
            exc = GatewayClientError(req.get("error") or "")
            detail = req.get("detail")
            exc.data = {"detail": detail} if detail is not None else None
            raise exc
        if route == "exit":
            return live.EXIT_SESSION
        return route == "true"

    if req.get("canonical") in live._ROUTES:
        live._ROUTES[req["canonical"]] = answer
    gateway_side.connection_for = lambda cli: object() if req.get("conn") else None
    cli_module._cprint = lambda text: printed.append(text)
    try:
        got = live.run(SimpleNamespace(session_id="sess-1"), req.get("canonical"),
                       req.get("cmd"))
    finally:
        live._ROUTES.clear()
        live._ROUTES.update(saved[0])
        gateway_side.connection_for = saved[1]
        cli_module._cprint = saved[2]
    if got is live.EXIT_SESSION:
        result = "EXIT_SESSION"
    else:
        result = "TRUE" if got else "FALSE"
    return {"result": result, "printed": printed[0] if printed else None}


def constants():
    return {"routes": list(live._ROUTES), "refused": sorted(live.REFUSED)}


def expected(r):
    f = r.get("func")
    if f == "diff":
        return diff(r)
    if f == "handoff":
        return handoff(r)
    if f == "run":
        return run(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
