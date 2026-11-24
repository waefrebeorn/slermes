#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p3 (dedupe normalization, payload dump, URL harvest)."""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p3"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

WANTED = {"unescape_entities", "normalize_dedupe", "additional_text", "serialize_blocks",
          "extract_urls"}


def expected(r):
    f = r.get("func")
    if f == "unescape_entities":
        return live._unescape_slack_entities(r.get("text"))
    if f == "normalize_dedupe":
        return live._normalize_slack_text_for_dedupe(r.get("text"), r.get("bot_uid", ""))
    if f == "additional_text":
        return live._extract_additional_text_from_slack_blocks(
            r.get("blocks"), r.get("primary_text", ""), bot_uid=r.get("bot_uid", ""))
    if f == "serialize_blocks":
        if "max_chars" in r:
            return live._serialize_slack_blocks_for_agent(r.get("blocks"), r["max_chars"])
        return live._serialize_slack_blocks_for_agent(r.get("blocks"))
    if f == "extract_urls":
        return live._extract_urls_from_slack_blocks(r.get("blocks"))
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
