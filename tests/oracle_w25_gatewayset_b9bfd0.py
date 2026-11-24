#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_setting_commands.py (gate w25_gatewayset_b9bfd0).

The live module is imported and its own helpers are called: ``_mirror_busy``
runs against a stub ``cli`` that only records the mirrored attribute, and
``_unbounded`` runs with the ``gateway.session_cron_commands.cron_read`` seam
patched to the fixture's fixed answer (a read answer is ``None``).  The
connection, the socket and the gateway's handler never exist here.

Two expressions live inline in ``run()``; they are restated verbatim:
  * ``parts = (cmd or "").strip().split(None, 1)`` / ``arg = parts[1].strip()``
  * the reply-line loop (``splitlines`` + the ``_VERBATIM`` prefix rule)
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_gatewayset_b9bfd0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import gateway_setting_commands as live  # noqa: E402
import gateway.session_cron_commands as cron_commands  # noqa: E402

WANTED = {"is_setting", "split_arg", "mirror_busy", "unbounded",
          "call_command", "render", "constants"}


class _Cli:
    """The only client state these decisions touch."""

    def __init__(self):
        self.busy_input_mode = None


def is_setting(req):
    return req.get("canonical") in live.GATEWAY_SETTINGS


def split_arg(req):
    parts = (req.get("cmd") or "").strip().split(None, 1)
    return parts[1].strip() if len(parts) > 1 else ""


def mirror_busy(req):
    cli = _Cli()
    live._mirror_busy(cli, req.get("arg") if isinstance(req.get("arg"), str) else "")
    return cli.busy_input_mode


def unbounded(req):
    canonical = req.get("canonical")
    arg = req.get("arg") if isinstance(req.get("arg"), str) else ""
    seam = req.get("cron_read")
    answer = None if seam is None else seam
    real = cron_commands.cron_read
    cron_commands.cron_read = lambda words: answer
    try:
        return bool(live._unbounded(canonical, arg))
    finally:
        cron_commands.cron_read = real


def call_command(req):
    canonical = req.get("canonical") or ""
    arg = req.get("arg")
    return canonical + (" " + arg if arg else "")


def render(req):
    if req.get("stopped"):
        return None
    canonical = req.get("canonical")
    reply = req.get("reply")
    lines = str((reply or {}).get("output") or "").splitlines()
    return [f"  {line}" if line and canonical not in live._VERBATIM else line
            for line in lines]


def constants(_req):
    return {
        "gateway_settings": list(live.GATEWAY_SETTINGS),
        "busy_modes": sorted(live._BUSY_MODES),
        "verbatim": sorted(live._VERBATIM),
        "cron_command": "cron",
    }


def expected(r):
    f = r.get("func")
    return {
        "is_setting": is_setting,
        "split_arg": split_arg,
        "mirror_busy": mirror_busy,
        "unbounded": unbounded,
        "call_command": call_command,
        "render": render,
        "constants": constants,
    }[f](r)


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
