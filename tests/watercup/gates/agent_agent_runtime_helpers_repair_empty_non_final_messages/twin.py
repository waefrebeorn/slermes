"""twin for agent_agent_runtime_helpers_repair_empty_non_final_messages.

Traces the LIVE agent/agent_runtime_helpers.py::repair_empty_non_final_messages
ladder with the same rung order as the C harness:
  B1 len>=2 / LOOP idx<last: B2 dict, B3 role in {assistant,user},
  B4 not _msg_has_payload -> heal / B5 healed>0.
Decisions use the LIVE _msg_has_payload; the repaired array comes from the
LIVE function. Input: JSON array of message objects per argv item.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_arh", os.path.join(DEV, "agent", "agent_runtime_helpers.py"))
_arh = ilu.module_from_spec(_spec)
sys.modules["wc_live_arh"] = _arh
_spec.loader.exec_module(_arh)

_live_fn = _arh.repair_empty_non_final_messages
_has_payload = _arh._msg_has_payload
ROLES = ("assistant", "user")


def trace(messages):
    c = Cup("repair_empty_non_final_messages")
    live_out = _live_fn(messages)
    healed = sum(
        1 for a, b in zip(messages, live_out)
        if isinstance(a, dict) and isinstance(b, dict) and a is not b
        and b.get("content") == "[response interrupted]")
    if not messages or len(messages) < 2:             # B1 (incl. empty)
        c.branch(False)
    else:
        c.branch(True)
        last = len(messages) - 1
        for idx, msg in enumerate(messages):          # LOOP
            if idx >= last:
                break
            c.loop()
            if not isinstance(msg, dict):             # B2
                c.branch(False)
                continue
            c.branch(True)
            if msg.get("role") not in ROLES:          # B3
                c.branch(False)
                continue
            c.branch(True)
            if _has_payload(msg):                     # B4
                c.branch(False)
                continue
            c.branch(True)
        # B5: healed count from comparing LIVE output identity
        c.branch(healed > 0)
    parts = []
    for m in live_out:
        role = "-"
        wh = 0
        if isinstance(m, dict):
            role = m.get("role") or "-"
            if isinstance(m.get("content"), str) and \
                    m["content"] == "[response interrupted]":
                wh = 1
        parts.append("%s:%d" % (role, wh))
    c.valstr("n=%d|h=%d|" % (len(live_out), healed) + ",".join(parts))
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        cups.append(trace(_json.loads(item)))
    return cups
