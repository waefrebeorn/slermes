#!/usr/bin/env python3
"""Differential oracle for hermes_cli/sessions_cmd_repair_profiles.py
(gate w22_sessionscm_632617).

The live command is called with every one of its seams replaced:

  enumerate_stores     -> the fixture's store profiles
  live_gateway_homes   -> the fixture's [[profile, pid], ...]
  scan_stores          -> a fake plan (findings + repairable + apply) and a
                          fake session whose close() is recorded
  default_snapshot     -> a sentinel the fake plan ignores
  _confirm_or_exit_code-> the fixture's answer, recording (prompt, declined)

stdout/stderr are captured with redirect_stdout/redirect_stderr, so the compared
`stdout` is the exact print() stream (glyphs and all):

    out = {"exit", "stdout", "stderr", "prompts": [[prompt, declined], ...]}
"""
import importlib
import io
import json
import sys
import types
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w22_sessionscm_632617"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

cmd = importlib.import_module("hermes_cli.sessions_cmd_repair_profiles")
repair = importlib.import_module("hermes_cli.sessions_repair_profiles")
Finding = repair.Finding

WANTED = {"kind_label", "group", "report", "run"}


def finding(raw):
    raw = raw if isinstance(raw, dict) else {}
    item = Finding(kind=str(raw.get("kind") or ""), store=str(raw.get("store") or ""),
                   subject=str(raw.get("subject") or ""), detail=str(raw.get("detail") or ""),
                   action=raw.get("action"), reason=raw.get("reason"))
    if raw.get("repairable"):
        item._fix = lambda session: {}
    return item


def findings_of(r):
    rows = r.get("findings")
    return [finding(x) for x in rows] if isinstance(rows, list) else []


class FakePlan:
    EMPTY = {"snapshots": {}, "totals": {}, "failures": []}

    def __init__(self, rows, result):
        self.findings = rows
        self.repairable = [f for f in rows if f.repairable]
        self._result = result if isinstance(result, dict) else dict(FakePlan.EMPTY)
        self.snapshots = []

    def apply(self, session, snapshot=None):
        self.snapshots.append(snapshot)
        return self._result


class FakeSession:
    closed = 0

    def close(self):
        FakeSession.closed += 1


def run(r):
    rows = findings_of(r)
    raw_stores = r.get("stores") if isinstance(r.get("stores"), list) else []
    stores = [types.SimpleNamespace(profile=str(p)) for p in raw_stores]
    live = [(str(a), b) for a, b in (r.get("live") or [])] if isinstance(r.get("live"), list) else []
    prompts = []
    confirm = r.get("confirm")

    class Plan(FakePlan):
        pass

    def scan_stores(_stores, legacy_main="report", read_only=True):
        return Plan(rows, r.get("result")), FakeSession()

    saved = (cmd.enumerate_stores, cmd.live_gateway_homes, cmd.scan_stores,
             getattr(cmd, "default_snapshot", None))
    cmd.enumerate_stores = lambda: stores
    cmd.live_gateway_homes = lambda _s: live
    cmd.scan_stores = scan_stores
    cmd.default_snapshot = "SNAPSHOT-SEAM"

    sessions_cmd = sys.modules.get("hermes_cli.sessions_cmd")
    prev_confirm = None
    if sessions_cmd is None:
        sessions_cmd = types.ModuleType("hermes_cli.sessions_cmd")
        sys.modules["hermes_cli.sessions_cmd"] = sessions_cmd
    prev_confirm = getattr(sessions_cmd, "_confirm_or_exit_code", None)

    def fake_confirm(prompt, declined=None):
        prompts.append([prompt, declined])
        return confirm

    sessions_cmd._confirm_or_exit_code = fake_confirm
    args = types.SimpleNamespace(apply=bool(r.get("apply")), json=bool(r.get("json")),
                                 yes=bool(r.get("yes")), legacy_main=r.get("legacy_main"))
    out, err = io.StringIO(), io.StringIO()
    try:
        with redirect_stdout(out), redirect_stderr(err):
            code = cmd.cmd_repair_profiles(args)
    finally:
        (cmd.enumerate_stores, cmd.live_gateway_homes, cmd.scan_stores,
         cmd.default_snapshot) = saved
        if prev_confirm is None:
            if hasattr(sessions_cmd, "_confirm_or_exit_code"):
                del sessions_cmd._confirm_or_exit_code
        else:
            sessions_cmd._confirm_or_exit_code = prev_confirm
    return {"exit": code, "stdout": out.getvalue(), "stderr": err.getvalue(),
            "prompts": prompts}


def expected(r):
    f = r.get("func")
    if f == "kind_label":
        kind = r.get("kind") if isinstance(r.get("kind"), str) else ""
        return cmd._KIND_LABELS.get(kind, kind)
    if f == "group":
        grouped = cmd._group(findings_of(r))
        return {kind: [x.subject for x in rows] for kind, rows in grouped.items()}
    if f == "report":
        out = io.StringIO()
        with redirect_stdout(out):
            cmd._print_report(findings_of(r))
        return out.getvalue()
    if f == "run":
        return run(r)
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
