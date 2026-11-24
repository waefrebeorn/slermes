#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p5 (bounded cache eviction math + workspace ids)."""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p5"

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
WANTED = {
    "timestamp_sort_key", "discard_oldest", "evict_oldest", "trim_dict_entries",
    "metadata_team_id", "workspace_event_id", "workspace_message_marker",
    "truthy_config", "is_ignored_channel", "workspace_thread_key",
    "agent_view_context_key",
}


def bare():
    return AD.__new__(AD)


def getter(mode):
    if mode == "suffix":
        return lambda e: e.split(":")[2] if e.count(":") >= 2 else ""
    return lambda e: e


def sorted_entries(entries, mode):
    return sorted(entries, key=lambda e: AD._slack_timestamp_sort_key(getter(mode)(e)))


def expected(r):
    f = r.get("func")
    if f == "timestamp_sort_key":
        if "pair" in r:
            return list(AD._slack_timestamp_sort_key(tuple(r["pair"])))
        return list(AD._slack_timestamp_sort_key(r.get("ts")))
    if f == "discard_oldest":
        entries = set(r.get("entries") or [])
        n = int(r.get("count") or 0)
        mode = r.get("mode", "identity")
        before = sorted_entries(entries, mode)
        AD._discard_oldest_by_thread_ts(entries, n, getter(mode))
        removed = [e for e in before if e not in entries]
        return {"removed": removed, "remaining": sorted_entries(entries, mode)}
    if f == "evict_oldest":
        entries = set(r.get("entries") or [])
        mode = r.get("mode", "identity")
        bare()._evict_oldest_by_ts(entries, int(r.get("cap") or 0), getter(mode))
        return sorted_entries(entries, mode)
    if f == "trim_dict_entries":
        mapping = dict(r.get("mapping") or {})
        AD._trim_oldest_dict_entries(mapping, int(r.get("max_size") or 0))
        return mapping
    if f == "metadata_team_id":
        return AD._metadata_team_id(r.get("metadata"))
    if f == "workspace_event_id":
        return AD._workspace_event_id(r.get("team_id", ""), r.get("event_id", ""))
    if f == "workspace_message_marker":
        v = AD._workspace_message_marker(r.get("team_id", ""), r.get("message_id", ""))
        return list(v) if isinstance(v, tuple) else v
    if f == "truthy_config":
        return AD._truthy_config(r.get("value"))
    if f == "is_ignored_channel":
        inst = bare()
        ignored = set(r.get("ignored") or [])
        inst._slack_ignored_channels = lambda: ignored
        return inst._is_ignored_channel(r.get("channel_id"))
    if f == "workspace_thread_key":
        v = AD._workspace_thread_key(r.get("team_id", ""), r.get("channel_id", ""),
                                     r.get("thread_ts", ""))
        return list(v) if isinstance(v, tuple) else v
    if f == "agent_view_context_key":
        v = AD._agent_view_context_key(r.get("team_id", ""), r.get("user_id", ""))
        return list(v) if isinstance(v, tuple) else v
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
