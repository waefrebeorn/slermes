#!/usr/bin/env python3
"""
sta_oracle_context_compressor.py — oracle for t_port_context_compressor.c.

Recomputes each case from the LIVE agent/context_compressor.py and emits one
JSON line per case (same shape as the C harness). The runner diffs them
byte-for-byte. All emitted JSON uses compact separators (",", ":") to match
libjson's serialization exactly.
"""
import json
import sys
import os
import importlib.util


def _load():
    # Candidate roots for the LIVE Python source (the slermes C tree and the
    # real Hermes package are in separate directories). Prefer the canonical
    # checkout so the oracle compares against real upstream behavior.
    candidates = [
        "/home/wubu/.hermes/hermes-agent",
        "/home/wubu/hermes-agent-dev",
        os.getcwd(),
    ]
    for base in candidates:
        if not base:
            continue
        cand = f"{base}/agent/context_compressor.py"
        if not os.path.isfile(cand):
            continue
        try:
            spec = importlib.util.spec_from_file_location("live_cc", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.context_compressor as mod  # type: ignore
    return mod


cc = _load()
JS = (",", ":")  # compact separators -> matches libjson


def emit_name_args(c):
    tc = c.get("tool_call")
    name, args = cc._extract_tool_call_name_and_args(tc)
    return {"fn": "extract_name_args", "name": name, "args": args}


def emit_id(c):
    tc = c.get("tool_call")
    return {"fn": "extract_id", "out": cc._extract_tool_call_id(tc)}


def emit_content_text(c):
    content = c.get("content")
    return {"fn": "content_text", "out": cc._content_text_for_contains(content)}


def emit_fresh_compaction_message_copy(c):
    msg = c.get("message")
    res = cc._fresh_compaction_message_copy(msg)
    return {"fn": "fresh_compaction_message_copy",
            "out": json.dumps(res, ensure_ascii=False, separators=JS) if res is not None else "null"}


def emit_has_compressed_summary_metadata(c):
    msg = c.get("message")
    return {"fn": "has_compressed_summary_metadata",
            "out": bool(cc.ContextCompressor._has_compressed_summary_metadata(msg))}


def emit_starts_with_summary_prefix(c):
    text = c.get("text", "")
    return {"fn": "starts_with_summary_prefix",
            "out": bool(cc.ContextCompressor._starts_with_summary_prefix(text))}


def emit_classify_summary_content(c):
    text = c.get("text", "")
    r = cc.ContextCompressor.classify_summary_content(text)
    return {"fn": "classify_summary_content", "out": (r if r is not None else "null")}


def emit_is_context_summary_content(c):
    text = c.get("text", "")
    return {"fn": "is_context_summary_content",
            "out": bool(cc.ContextCompressor._is_context_summary_content(text))}


def emit_is_compaction_summary_message(c):
    msg = c.get("message")
    return {"fn": "is_compaction_summary_message",
            "out": bool(cc.is_compaction_summary_message(msg))}


def emit_append_text(c):
    content = c.get("content")
    text = c.get("text", "")
    prepend = bool(c.get("prepend", False))
    res = cc._append_text_to_content(content, text, prepend=prepend)
    return {"fn": "append_text", "out": json.dumps(res, ensure_ascii=False, separators=JS)}


def emit_skill_pruned_marker(c):
    name = c.get("name", "")
    return {"fn": "skill_pruned_marker", "out": cc._skill_pruned_marker(name)}


def emit_extract_pruned_skill_names(c):
    text = c.get("text", "")
    return {"fn": "extract_pruned_skill_names",
            "out": cc._extract_pruned_skill_names(text or "")}


def emit_reinject_pruned_skill_markers(c):
    summary = c.get("summary", "")
    skills = c.get("skills", []) or []
    return {"fn": "reinject_pruned_skill_markers",
            "out": cc._reinject_pruned_skill_markers(summary, list(skills))}


def emit_strip_persistence_markers(c):
    messages = c.get("messages")
    if messages is None:
        return {"fn": "strip_persistence_markers", "rc": -1, "out": []}
    rc = cc._strip_persistence_markers(messages)
    return {"fn": "strip_persistence_markers", "rc": rc, "out": messages}


DISPATCH = {
    "extract_name_args": emit_name_args,
    "extract_id": emit_id,
    "content_text": emit_content_text,
    "append_text": emit_append_text,
    "skill_pruned_marker": emit_skill_pruned_marker,
    "extract_pruned_skill_names": emit_extract_pruned_skill_names,
    "reinject_pruned_skill_markers": emit_reinject_pruned_skill_markers,
    "strip_persistence_markers": emit_strip_persistence_markers,
    "fresh_compaction_message_copy": emit_fresh_compaction_message_copy,
    "has_compressed_summary_metadata": emit_has_compressed_summary_metadata,
    "starts_with_summary_prefix": emit_starts_with_summary_prefix,
    "classify_summary_content": emit_classify_summary_content,
    "is_context_summary_content": emit_is_context_summary_content,
    "is_compaction_summary_message": emit_is_compaction_summary_message,
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_context_compressor.py <cases.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        fn = DISPATCH.get(op)
        out = fn(c) if fn else {"fn": op}
        sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=JS) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
