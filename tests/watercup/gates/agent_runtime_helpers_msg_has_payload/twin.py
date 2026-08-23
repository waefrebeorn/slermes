"""twin for agent_runtime_helpers_msg_has_payload.

Transcribes the LIVE agent/agent_runtime_helpers.py::_msg_has_payload body
verbatim with Cup events interleaved in the SAME rung order as the C harness:

  B1 content-is-str / B2 nonblank / B3 content-is-list / per-block
  L,B4,B5,B6,(B7) / B8 content-not-in-(None,"") / B9 tool_calls /
  B10 reasoning_content / B11 reasoning|details / B12 codex_*_items /
  V result.
"""
import os
import json
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup


def _truthy(v):
    """Python `if val:` for JSON-decoded values."""
    return bool(v)


def run(inputs):
    cups = []
    for raw in inputs:
        msg = json.loads(raw)
        c = Cup("msg_has_payload")
        result = False

        def finalize():
            c.valstr("true" if result else "false")
            cups.append(c.finalize())

        if not isinstance(msg, dict):
            c.branch(True)
            finalize()
            continue
        c.branch(False)

        content = msg.get("content")

        # B1: isinstance(content, str)
        is_str = isinstance(content, str)
        c.branch(is_str)
        if is_str:
            nb = bool(content.strip())          # B2
            c.branch(nb)
            result = nb
            finalize()
            continue

        # B3: isinstance(content, list)
        is_list = isinstance(content, list)
        c.branch(is_list)
        if is_list:
            found = False
            for block in content:               # loop ticks
                c.loop()
                if isinstance(block, dict):     # B4
                    c.branch(True)
                    is_text = block.get("type") == "text"   # B5
                    c.branch(is_text)
                    if is_text:
                        t = block.get("text")
                        nb = isinstance(t, str) and bool(t.strip())  # B6
                        c.branch(nb)
                        if nb:
                            found = True
                            break
                        continue
                    found = True                # B7 other typed block
                    break
                c.branch(False)
                if block:                       # B7b truthy non-dict
                    c.branch(True)
                    found = True
                    break
                c.branch(False)
            result = found
            finalize()
            continue

        # B8: content not in (None, "")
        hit = content is not None and content != ""
        c.branch(hit)
        if hit:
            result = True
            finalize()
            continue

        # B9 tool_calls
        tc = bool(msg.get("tool_calls"))
        c.branch(tc)
        if tc:
            result = True
            finalize()
            continue

        # B10 reasoning_content nonblank str
        rcs = msg.get("reasoning_content")
        rc = isinstance(rcs, str) and bool(rcs.strip())
        c.branch(rc)
        if rc:
            result = True
            finalize()
            continue

        # B11 reasoning | reasoning_details
        r = bool(msg.get("reasoning"))
        c.branch(r)
        rd = (not r) and bool(msg.get("reasoning_details"))
        c.branch(rd)
        if r or rd:
            result = True
            finalize()
            continue

        # B12 codex_message_items | codex_reasoning_items
        cm = bool(msg.get("codex_message_items"))
        c.branch(cm)
        cri = (not cm) and bool(msg.get("codex_reasoning_items"))
        c.branch(cri)
        if cm or cri:
            result = True
            finalize()
            continue

        result = False
        finalize()
    return cups
