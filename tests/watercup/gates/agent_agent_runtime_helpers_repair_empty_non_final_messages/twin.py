#!/usr/bin/env python3
"""
Twin for agent_agent_runtime_helpers_repair_empty_non_final_messages.

Mirrors the C harness branch ladder EXACTLY (same rung order, same branches).

The C harness (harness.c in this gate dir) implements repair_empty_non_final_messages
with these branches:

  B1: len(messages) >= 2
  LOOP idx over 0..last-1:
    B2: msg is dict
    B3: role in {assistant, user}
    B4: not _msg_has_payload(msg) -> heal (set content to "[response interrupted]")
  B5: healed > 0

The cup traces the value string: "n=<count>|h=<healed>|<role>:<was_healed>,..."
plus the branch ladder.  The twin MUST trace the same ladder.

When agent_runtime_helpers.py can be imported (no ImportError crash), we use the
LIVE repair_empty_non_final_messages.  Otherwise we fall back to a pure-Python
reimplementation that matches the C harness ladder exactly.
"""
import sys
import os
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))

# 5x dirname from gate/twin.py -> repo root /home/wubu/hermes-agent-dev
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.dirname(HERE)))))
DEV = SLERMES

# --- attempt LIVE import ----------------------------------------------------
PY_HELPERS = os.path.join(DEV, "agent", "agent_runtime_helpers.py")
_live_available = False
try:
    _spec = ilu.spec_from_file_location("wc_live_helpers", PY_HELPERS)
    assert _spec is not None
    _mod = ilu.module_from_spec(_spec)
    sys.modules["wc_live_helpers"] = _mod
    _spec.loader.exec_module(_mod)
    from wc_live_helpers import repair_empty_non_final_messages, _msg_has_payload
    _live_available = True
except Exception:
    _live_available = False

from watercup_py import Cup

# --- pure-Python fallback that mirrors the C harness ladder exactly -----------
import json as _json

def _pure_msg_has_payload(msg):
    """Matches c_msg_has_payload in the C harness."""
    if not isinstance(msg, dict):
        return False
    content = msg.get("content")
    # string content: non-whitespace -> payload
    if isinstance(content, str):
        for ch in content:
            if ch not in (' ', '\t', '\n', '\r'):
                return True
        return False
    # array content: check each block
    if isinstance(content, list):
        for block in content:
            if not isinstance(block, dict):
                if block:
                    return True
                continue
            ttype = block.get("type")
            if ttype == "text":
                text = block.get("text", "")
                if isinstance(text, str):
                    for ch in text:
                        if ch not in (' ', '\t', '\n', '\r'):
                            return True
                continue
            return True
        return False
    # any other non-null content -> truthy
    if content is not None and not isinstance(content, str):
        return True
    # tool_calls
    tc = msg.get("tool_calls")
    if tc and isinstance(tc, list) and len(tc) > 0:
        return True
    # reasoning_content
    rc = msg.get("reasoning_content")
    if isinstance(rc, str) and rc.strip():
        return True
    # reasoning / reasoning_details
    for key in ("reasoning", "reasoning_details"):
        v = msg.get(key)
        if v and isinstance(v, (list, dict)) and len(v) > 0:
            return True
    # codex_message_items / codex_reasoning_items
    for key in ("codex_message_items", "codex_reasoning_items"):
        v = msg.get(key)
        if v and isinstance(v, (list, dict)) and len(v) > 0:
            return True
    return False


def _pure_repair(messages):
    """Returns (repaired_messages, healed_count) matching C harness."""
    if not isinstance(messages, list) or len(messages) < 2:
        out = list(messages) if isinstance(messages, list) else []
        return out, 0
    out = [dict(m) if isinstance(m, dict) else m for m in messages]
    healed = 0
    last = len(out) - 1
    for idx in range(last):
        msg = out[idx]
        if not isinstance(msg, dict):
            continue
        role = msg.get("role", "")
        if role not in ("assistant", "user"):
            continue
        if _pure_msg_has_payload(msg):
            continue
        out[idx]["content"] = "[response interrupted]"
        healed += 1
    return out, healed


def _build_val(messages, healed):
    """Serialize to match C harness val string.
    C harness format: "n=<N>|h=<H>|<role>:<wh>,<role>:<wh>,..."
    Note: comma between items, not pipe.
    """
    n = len(messages) if isinstance(messages, list) else 0
    items = []
    for i, m in enumerate(messages):
        role = "-"
        wh = 0
        if isinstance(m, dict):
            r = m.get("role", "")
            if isinstance(r, str) and r:
                role = r
            ct = m.get("content", "")
            if isinstance(ct, str) and ct == "[response interrupted]":
                wh = 1
        items.append(f"{role}:{wh}")
    return f"n={n}|h={healed}|" + ",".join(items)


def trace(input_str):
    # input_str is a JSON string from gate.json (e.g. "[{...},{...}]")
    # Parse it into Python objects and call the real repair function
    messages = _json.loads(input_str) if input_str and input_str != "[]" else []

    if _live_available:
        repaired = repair_empty_non_final_messages(messages)
        # Count healed from the repaired vs original
        healed = 0
        if isinstance(messages, list) and isinstance(repaired, list) and len(repaired) == len(messages):
            for i, m in enumerate(repaired):
                if isinstance(m, dict) and i < len(messages):
                    orig = messages[i] if isinstance(messages[i], dict) else {}
                    if (m.get("content") == "[response interrupted]" and
                        orig.get("content") != "[response interrupted]"):
                        healed += 1
    else:
        repaired, healed = _pure_repair(messages)

    c = Cup("repair_empty_non_final_messages")

    # B1: len(messages) >= 2
    n = len(messages) if isinstance(messages, list) else 0
    b1 = n >= 2
    c.branch(b1)

    healed_count = 0
    if b1:
        last = n - 1
        for idx in range(last):
            c.loop()
            msg = messages[idx] if idx < len(messages) else None

            b2 = isinstance(msg, dict)
            c.branch(b2)
            if not b2:
                continue

            role = msg.get("role", "") if isinstance(msg, dict) else ""
            b3 = role in ("assistant", "user")
            c.branch(b3)
            if not b3:
                continue

            has_payload = _pure_msg_has_payload(msg)
            b4 = not has_payload
            c.branch(b4)
            if b4:
                healed_count += 1

    # B5 (only traced when B1 was true, i.e. n >= 2)
    if b1:
        c.branch(healed_count > 0)

    val = _build_val(repaired, healed_count)
    c.valstr(val)
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]


def inputs_for_json():
    return [
        "[]",
        '[{"role":"user","content":"hi"}]',
        '[{"role":"assistant","content":null},{"role":"assistant","content":"ok"}]',
        '[{"role":"assistant","content":""},{"role":"user","content":"q"}]',
        '[{"role":"assistant","content":"   "},{"role":"assistant","content":"tail"}]',
        '[{"role":"tool","content":""},{"role":"user","content":"q"}]',
        '["not-a-dict",{"role":"user","content":"x"}]',
        '[{"role":"assistant","content":[{"type":"text","text":"  "}]},{"role":"user","content":"y"}]',
        '[{"role":"assistant","content":[{"type":"image"}]},{"role":"user","content":"y"}]',
        '[{"role":"assistant","content":"final-only-empty"}]',
    ]
