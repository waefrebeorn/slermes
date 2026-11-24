#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/adapter.py gate
w27_adapter_74f618_p2 (Block Kit reading and rendering)."""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_74f618_p2"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)
if not os.environ.get("HERMES_HOME"):
    os.environ["HERMES_HOME"] = str(QUARRY)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import plugins.platforms.slack.adapter as live  # noqa: E402

WANTED = {
    "collect_block_mentions", "mention_detection_text", "slack_str_field",
    "render_inline_element", "render_inline_elements", "extract_text_blocks",
    "table_cell_text", "render_table_block", "slack_error_is",
}


class _FakeApiError(Exception):
    """Minimal slack_sdk SlackApiError shape: .response mapping + message."""

    def __init__(self, response):
        super().__init__("api error")
        self.response = response


def expected(r):
    f = r.get("func")
    if f == "collect_block_mentions":
        return live._collect_slack_block_mentions(r.get("blocks"))
    if f == "mention_detection_text":
        return live._slack_mention_detection_text(r.get("event") or {})
    if f == "slack_str_field":
        return live._slack_str_field(r.get("el") or {}, r.get("name", ""))
    if f == "render_inline_element":
        return live._render_slack_inline_element(r.get("el") or {})
    if f == "render_inline_elements":
        return live._render_inline_elements(r.get("elements") or [])
    if f == "extract_text_blocks":
        return live._extract_text_from_slack_blocks(r.get("blocks"))
    if f == "table_cell_text":
        return live._collect_slack_table_cell_text(r.get("value"))
    if f == "render_table_block":
        block = r.get("block") or {}
        if "max_chars" in r:
            return live._render_slack_table_block(block, r["max_chars"])
        return live._render_slack_table_block(block)
    if f == "slack_error_is":
        return live._slack_error_is(_FakeApiError(r.get("response")), r.get("code", ""))
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
