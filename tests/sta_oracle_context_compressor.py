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
import importlib.util


def _load():
    for base in sys.path:
        cand = f"{base}/agent/context_compressor.py"
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


def emit_append_text(c):
    content = c.get("content")
    text = c.get("text", "")
    prepend = bool(c.get("prepend", False))
    res = cc._append_text_to_content(content, text, prepend=prepend)
    # Serialize to compact JSON to match json_serialize output exactly.
    return {"fn": "append_text", "out": json.dumps(res, ensure_ascii=False, separators=JS)}


DISPATCH = {
    "extract_name_args": emit_name_args,
    "extract_id": emit_id,
    "content_text": emit_content_text,
    "append_text": emit_append_text,
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
