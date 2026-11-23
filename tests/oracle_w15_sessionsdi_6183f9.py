#!/usr/bin/env python3
"""Differential oracle for hermes_cli/sessions_discard.py (gate w15_sessionsdi_6183f9)."""
import asyncio
import contextlib
import importlib.util
import io
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_sessionsdi_6183f9"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_hc = types.ModuleType("hermes_cli")
_hc.__path__ = []
sys.modules["hermes_cli"] = _hc


class GatewayClientError(ValueError):
    pass


class FakeClient:
    def __init__(self, snapshot):
        self.snapshot = snapshot
        self.resolved = []

    async def rpc(self, method, **kwargs):
        if method == "session.resume":
            return self.snapshot
        if method == "prompt.resolve_unknown":
            self.resolved.append(kwargs.get("admission_id"))
        return None


class Connect:
    def __init__(self, client):
        self.client = client

    async def __aenter__(self):
        return self.client

    async def __aexit__(self, *args):
        return False


_client = {"value": None}
_mod("hermes_cli.gateway_client", GatewayClientError=GatewayClientError,
     connect_gateway=lambda: Connect(_client["value"]))

SRC = QUARRY + "/hermes_cli/sessions_discard.py"
_spec = importlib.util.spec_from_file_location("sessions_discard_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sessions_discard_live"] = live
_spec.loader.exec_module(live)


def confirm_prompt_live(rec):
    rows = rec.get("rows") or []
    out = io.StringIO()
    real_input = getattr(__builtins__, "input", None)
    def fake_input(prompt):
        return "n"
    if isinstance(__builtins__, dict):
        __builtins__["input"] = fake_input
    else:
        __builtins__.input = fake_input
    try:
        with contextlib.redirect_stdout(out):
            live._confirm(rec.get("session_id", ""), rows)
    finally:
        if isinstance(__builtins__, dict):
            __builtins__["input"] = real_input
        else:
            __builtins__.input = real_input
    return out.getvalue()


def discard_plan_live(rec):
    snapshot = rec.get("snapshot") or {}
    client = FakeClient(snapshot)
    _client["value"] = client
    live._confirm = lambda sid, rows: bool(rec.get("confirm", False))
    live.sys = types.SimpleNamespace(
        stdin=types.SimpleNamespace(isatty=lambda: bool(rec.get("isatty", False))),
        stderr=None)
    args = types.SimpleNamespace(session="sid", admission=rec.get("admission") or [],
                                 yes=bool(rec.get("yes", False)))
    so, se = io.StringIO(), io.StringIO()
    code = -1
    error = None
    try:
        with contextlib.redirect_stdout(so), contextlib.redirect_stderr(se):
            live.sys.stderr = sys.stderr
            code = asyncio.run(live._discard(args))
    except GatewayClientError as exc:
        error = str(exc)
        code = -1
    result = {"chosen": client.resolved, "missing": [], "code": code, "error": error,
              "lines": so.getvalue().splitlines(), "stderr_lines": se.getvalue().splitlines()}
    if result["error"] and result["error"].startswith("Not an unknown (lost) admission of this session: "):
        result["missing"] = result["error"].split(": ", 1)[1].split(", ")
    return result


def live_out(rec):
    f = rec.get("func")
    if f == "confirm_prompt":
        return confirm_prompt_live(rec)
    if f == "discard_plan":
        return discard_plan_live(rec)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("confirm_prompt", "discard_plan"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            print(f"LIVE ERROR {rec.get('func')}: {exc!r}", file=sys.stderr)
            mism += 1
            continue
        got = rec.get("out")
        if f_ := (rec.get("func") == "discard_plan") and isinstance(got, dict) and isinstance(py, dict):
            expected = {k: got.get(k) for k in ("chosen", "code", "error", "lines", "stderr_lines")}
            actual = {k: py.get(k) for k in expected}
            if actual != expected:
                mism += 1
                print(f"MISMATCH {G} case={rec.get('case')} C={expected!r} PY={actual!r}", file=sys.stderr)
            elif got.get("missing") and got.get("missing") != py.get("missing"):
                mism += 1
                print(f"MISMATCH {G} case={rec.get('case')} missing C={got.get('missing')!r} PY={py.get('missing')!r}", file=sys.stderr)
            continue
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
