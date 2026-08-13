#!/usr/bin/env python3
"""Oracle for the pure, DB-free helpers of hermes_state.py ported to C.

Reads a JSON cases file (each case = {"op": ..., "arg": ...}), recomputes the
function from the reference logic in hermes_state.py, and emits one JSON line
per case. The C harness emits the same shape; the runner diffs them.

The reference logic is inlined VERBATIM from hermes_state.py (not a re-import)
because importing the full module drags in heavy transitive deps that can
resolve to a stale path. The inlined bodies are byte-faithful to the source:
  - _DISK_FULL_MARKERS / is_disk_full_error
  - classify_persistence_error
  - _STALE_TOOL_CALL_MARKER_RE / _is_stale_tool_call_marker_message
  - _strip_stale_tool_call_markers
"""
import json
import re
import sys

# ── Reference logic copied verbatim from hermes_state.py ────────────────
_DISK_FULL_MARKERS = (
    "no space left on device",
    "not enough space",
    "database or disk is full",  # SQLITE_FULL
    "disk full",
    "full disk",
    "enospc",
)


def is_disk_full_error(exc):
    if exc is None:
        return False
    text = exc if isinstance(exc, str) else str(exc)
    lowered = text.lower()
    return any(marker in lowered for marker in _DISK_FULL_MARKERS)


def classify_persistence_error(exc_or_str):
    if exc_or_str is None:
        return "unknown"
    text = str(exc_or_str).lower()
    if (
        "locked" in text
        or "busy" in text
        or "being compressed" in text
        or "compression lease" in text
    ):
        return "locked"
    if (
        is_disk_full_error(exc_or_str)
        or "disk" in text
        or "readonly" in text
        or "read-only" in text
    ):
        return "disk"
    return "unknown"


_STALE_TOOL_CALL_MARKER_RE = re.compile(r"^\[[A-Za-z_][A-Za-z0-9_.-]*\]$")


def _is_stale_tool_call_marker_message(msg):
    if not isinstance(msg, dict):
        return False
    if msg.get("role") != "assistant":
        return False
    if not msg.get("tool_calls"):
        return False
    content = msg.get("content")
    if not isinstance(content, str):
        return False
    return bool(_STALE_TOOL_CALL_MARKER_RE.fullmatch(content.strip()))


def _strip_stale_tool_call_markers(messages):
    for msg in messages:
        if _is_stale_tool_call_marker_message(msg):
            msg["content"] = ""
    return messages


def main():
    cases_path = sys.argv[1]
    with open(cases_path, encoding="utf-8") as f:
        cases = json.load(f)

    for c in cases:
        op = c.get("op")
        arg = c.get("arg")
        if op == "is_disk_full_error":
            out = bool(is_disk_full_error(arg))
            print(json.dumps({"fn": "is_disk_full_error",
                              "arg": arg, "out": 1 if out else 0}))
        elif op == "classify_persistence_error":
            out = classify_persistence_error(arg)
            print(json.dumps({"fn": "classify", "arg": arg, "out": out}))
        elif op == "is_stale_marker":
            obj = json.loads(arg) if isinstance(arg, str) and arg else {}
            out = _is_stale_tool_call_marker_message(obj)
            print(json.dumps({"fn": "is_stale", "arg": arg,
                              "out": 1 if out else 0}))
        elif op == "strip_stale":
            msgs = json.loads(arg) if isinstance(arg, str) and arg else []
            repaired = 0
            for m in msgs:
                if _is_stale_tool_call_marker_message(m):
                    m["content"] = ""
                    repaired += 1
            print(json.dumps({"fn": "strip", "out_count": repaired,
                              "out": msgs}))
        else:
            print(json.dumps({"fn": "unknown", "out": None}))


if __name__ == "__main__":
    main()
