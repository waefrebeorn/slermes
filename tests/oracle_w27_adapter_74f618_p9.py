#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p9 (workspace resolution, agent-view context, assistant meta)."""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p9"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

AD = live.SlackAdapter
WANTED = {"event_team_id", "team_in_install_set", "is_foreign_author", "context_channel_id",
          "extract_assistant_meta", "agent_view_event_fields", "suggested_prompts"}


def bare(installs=None):
    inst = AD.__new__(AD)
    if installs is not None:
        inst._team_clients = {t: object() for t in installs}
    else:
        inst._team_clients = {}
    inst._channel_team = {}
    return inst


def installs_of(r):
    if "installs" not in r:
        return None
    raw = r.get("installs")
    return None if raw is None else [str(t) for t in raw]


def expected(r):
    f = r.get("func")
    installs = installs_of(r)
    if f == "event_team_id":
        return AD._event_team_id(r.get("event") or {}, r.get("body"),
                                 install_set=None if installs is None else set(installs))
    if f == "team_in_install_set":
        inst = bare(installs or [])
        return inst._team_in_install_set(r.get("team_id", ""))
    if f == "is_foreign_author":
        inst = bare(installs or [])
        return inst._is_foreign_author(r.get("author_team", ""), r.get("resolved_team", ""))
    if f == "context_channel_id":
        return AD._context_channel_id(r.get("context"))
    if f == "extract_assistant_meta":
        inst = bare(installs or [])
        return inst._extract_assistant_thread_metadata(r.get("event") or {}, r.get("body"))
    if f == "agent_view_event_fields":
        inst = bare(installs or [])
        return inst._agent_view_event_fields(r.get("event") or {}, r.get("body"))
    if f == "suggested_prompts":
        inst = bare()
        inst.config = SimpleNamespace(extra={"suggested_prompts": r.get("raw")})
        title, prompts = inst._assistant_suggested_prompts()
        return [title, prompts]
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
