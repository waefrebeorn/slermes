#!/usr/bin/env python3
"""Differential oracle for agent/conversation_compression_deadline.py
(gate w25_conversati_616874).

The live module is imported and its own functions are called.  Three seams are
pinned to fixed inputs instead of being read:

  * ``hermes_cli.config.load_config`` -> {} when the fixture has no cfg (the
    "no compression config on disk" case);
  * ``agent.auxiliary_client._effective_aux_timeout`` -> the fixture's
    ``aux_budget`` (the config-derived aux summary budget);
  * the commit fence is a stub exposing only ``deadline_monotonic``.
"""
import json
import os
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(Path(tempfile.gettempdir()))

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_conversati_616874"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.config as hermes_config  # noqa: E402
import agent.auxiliary_client as aux_client  # noqa: E402
from agent import conversation_compression_deadline as live  # noqa: E402

WANTED = {"resolve_timeouts", "effective_ceiling", "constants"}

_LOAD = hermes_config.load_config
_AUX = aux_client._effective_aux_timeout


def resolve_timeouts(req):
    cfg = req.get("cfg")
    budget = req.get("aux_budget")
    aux_client._effective_aux_timeout = (
        lambda task, timeout: budget if timeout is None else timeout)
    hermes_config.load_config = lambda *a, **k: {}
    try:
        idle, ceiling = live.resolve_context_compression_timeouts(cfg)
    finally:
        aux_client._effective_aux_timeout = _AUX
        hermes_config.load_config = _LOAD
    return [idle, ceiling]


def effective_ceiling(req):
    deadline = req.get("fence_deadline")
    fence = SimpleNamespace(deadline_monotonic=deadline)
    return live.effective_ceiling(fence, req.get("ceiling"), req.get("wait_started"))


def constants(_req):
    return {
        "default_context_timeout_seconds": live.DEFAULT_CONTEXT_TIMEOUT_SECONDS,
        "default_context_total_ceiling_seconds": live.DEFAULT_CONTEXT_TOTAL_CEILING_SECONDS,
        "idle_key": "context_timeout_seconds",
        "ceiling_key": "context_total_ceiling_seconds",
        "aux_task": "compression",
    }


def expected(r):
    f = r.get("func")
    return {
        "resolve_timeouts": resolve_timeouts,
        "effective_ceiling": effective_ceiling,
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
