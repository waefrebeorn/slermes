#!/usr/bin/env python3
"""Differential oracle for tui_gateway/free_tier_task_done.py
(gate w22_freetierta_e95c57).

Nothing in this module is stubbed except the one side effect it performs:

  anon    -- live `is_anonymous_agent` over a stand-in agent object carrying the
             fixture's `provider` / `api_key` (the two attributes the predicate
             reads). The JWT claim decode runs live.
  clean   -- live `is_clean_task` over the fixture's result dict.
  record  -- live `note_task_done`, with `hermes_cli.free_tier_offer
             .record_task_done` wrapped so the call is observed instead of
             written; `out` is 1 when it fired.

The report is the decision, not the write: the offer state itself is the seam.
"""
import importlib
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w22_freetierta_e95c57"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("tui_gateway.free_tier_task_done")
offer = importlib.import_module("hermes_cli.free_tier_offer")

WANTED = {"anon", "clean", "record"}


def agent_of(r):
    return SimpleNamespace(provider=r.get("provider", ""), api_key=r.get("api_key"))


def expected(r):
    f = r.get("func")
    if f == "anon":
        return 1 if _anon(r) else 0
    if f == "clean":
        return 1 if live.is_clean_task(r.get("result"), agent_of(r),
                                       user_input=bool(r.get("user_input")),
                                       display_kind=r.get("display_kind")) else 0
    if f == "record":
        fired = []
        original = offer.record_task_done
        offer.record_task_done = lambda *a, **k: fired.append(1)
        try:
            live.note_task_done(r.get("session"), r.get("result"), agent_of(r),
                                r.get("display_kind"))
        finally:
            offer.record_task_done = original
        return 1 if fired else 0
    raise AssertionError("unknown func %r" % (f,))


def _anon(r):
    from hermes_cli.anon_auth import is_anonymous_agent
    return is_anonymous_agent(agent_of(r))


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
