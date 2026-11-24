#!/usr/bin/env python3
"""Differential oracle for agent/conversation_compression_reply_anchor.py
(gate w26_conversati_9bd9e4).

Every function under test is the live module's own:

  * ``_same_visible_content`` / ``_tool_call_ids`` / ``_tool_result_id`` /
    ``_reused_tool_call_ids`` / ``_reply_insertion_index`` called directly;
  * ``_ensure_compressed_keeps_last_assistant_reply`` called on the fixture
    transcript with only the token ESTIMATOR replaced (a measured cost surface,
    supplied as ``rough``), so the placement, twin, alternation and no-growth
    decisions are the real code.  The mutated candidate and the returned anchor
    are both compared.
"""
import json
import os
import sys
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_conversati_9bd9e4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from agent import conversation_compression_reply_anchor as live  # noqa: E402
from agent.conversation_compression import _message_text  # noqa: E402
from agent.conversation_compression_scaffolding import _SYNTHETIC_USER_FLAGS  # noqa: E402
import agent.conversation_loop as _loop  # noqa: E402
import agent.context_compressor as _cc  # noqa: E402
import session_rules.turns as _turns  # noqa: E402

WANTED = {"same_content", "tool_ids", "result_id", "reused_ids", "insertion_index",
          "anchor", "constants"}

_SYN_TEXTS = [
    _cc.COMPRESSION_CONTINUATION_USER_CONTENT, _cc._LEGACY_COMPRESSION_CONTINUATION_USER_CONTENT,
    _cc.MAX_ITERATIONS_SUMMARY_REQUEST, _loop._CODEX_ACK_CONTINUATION_NUDGE, _loop._CODEX_INCOMPLETE_NUDGE,
    _loop._DEGENERATE_FINAL_NUDGE, _loop._DROPPED_TOOLCALL_NUDGE_CONTENT, _loop._EMPTY_TOOL_RESPONSE_NUDGE,
    _loop._LENGTH_CONTINUATION_NETWORK_STUB, _loop._LEGACY_LENGTH_CONTINUATION_NETWORK_STUB,
    _loop._LENGTH_CONTINUATION_OUTPUT_LIMIT,
]


def same_content(req):
    left, right = req.get("left"), req.get("right")
    right_text = req.get("right_text")
    if not isinstance(right_text, str):
        right_text = _message_text(right).strip()
    return bool(live._same_visible_content(left, right, right_text))


def tool_ids(req):
    message = req.get("message")
    return sorted(live._tool_call_ids(message if isinstance(message, dict) else {}))


def result_id(req):
    message = req.get("message")
    return live._tool_result_id(message if isinstance(message, dict) else {})


def reused_ids(req):
    messages = req.get("messages")
    return sorted(live._reused_tool_call_ids(messages if isinstance(messages, list) else []))


def insertion_index(req):
    followers = req.get("followers")
    followers = followers if isinstance(followers, list) else []
    compressed = req.get("compressed")
    compressed = compressed if isinstance(compressed, list) else []
    reused = frozenset(req.get("reused_ids") or ())
    if reused:
        return live._reply_insertion_index(followers, compressed, reused_ids=reused)
    return live._reply_insertion_index(followers, compressed)


def anchor(req):
    original = req.get("original_messages")
    original = original if isinstance(original, list) else []
    compressed = req.get("compressed")
    compressed = compressed if isinstance(compressed, list) else []
    rough = req.get("rough") or {}
    seq = [rough.get("in", 0), rough.get("out", 0), rough.get("anchor", 0)]
    state = {"i": 0}

    def fake_estimate(messages):
        i = state["i"]
        state["i"] = i + 1
        return seq[i] if i < len(seq) else 0

    with mock.patch.object(live, "estimate_messages_tokens_rough", fake_estimate):
        placed = live._ensure_compressed_keeps_last_assistant_reply(original, compressed)
    return {"anchor": placed, "compressed": compressed}


def constants():
    return {
        "db_persisted_marker": _cc._DB_PERSISTED_MARKER,
        "summary_prefix_count": 2 + len(_turns._HISTORICAL_SUMMARY_PREFIXES),
        "merged_summary_delimiter": _turns._MERGED_SUMMARY_DELIMITER,
        "legacy_summary_prefix": _turns.LEGACY_SUMMARY_PREFIX,
        "scaffolding_flags": list(_SYNTHETIC_USER_FLAGS),
        "synthetic_turn_text_count": len(set(_SYN_TEXTS)),
    }


def expected(r):
    f = r.get("func")
    if f == "same_content":
        return same_content(r)
    if f == "tool_ids":
        return tool_ids(r)
    if f == "result_id":
        return result_id(r)
    if f == "reused_ids":
        return reused_ids(r)
    if f == "insertion_index":
        return insertion_index(r)
    if f == "anchor":
        return anchor(r)
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
