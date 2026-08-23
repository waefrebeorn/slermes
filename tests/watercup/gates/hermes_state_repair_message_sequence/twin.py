"""twin for hermes_state_repair_message_sequence.

Traces LIVE agent/agent_runtime_helpers.py::repair_message_sequence with the
same rung ladder as the C harness: B1 non-empty, B2 consecutive-assistant
run, B3 has tool rows, B4 consecutive-user run. V = "<repairs>|<dump>"
where each surviving message renders canonically as
"role;content;tool_call_id;tool_call_ids;finish_reason;reasoning;codex#".

Sub-contract: messages carry plain-string content only, and an assistant's
tool_calls (when present) are represented via the tool_call_ids string so
both sides share one representation. The api_content sidecar is absent from
all inputs, making LIVE's drop_stale_api_content a no-op.
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_arh", os.path.join(DEV, "agent", "agent_runtime_helpers.py"))
_arh = ilu.module_from_spec(spec)
sys.modules["wc_live_arh"] = _arh
spec.loader.exec_module(_arh)

_fn = _arh.repair_message_sequence


def _canon(m):
    if not isinstance(m, dict):
        return ";;;;;;"
    tcs = m.get("tool_calls") or []
    ids = []
    for tc in tcs:
        if isinstance(tc, dict):
            entry = ",".join(x for x in (tc.get("id"), tc.get("call_id")) if x)
            ids.append(entry)
    return ";".join([
        m.get("role") or "",
        m.get("content") or "",
        m.get("tool_call_id") or "",
        ";".join(ids),
        m.get("finish_reason") or "",
        m.get("reasoning_content") or "",
        "1" if (m.get("codex_reasoning_items") or m.get("codex_message_items")
                or m.get("finish_reason") == "incomplete") else "0",
    ]) + "#"


def trace(item):
    c = Cup("repair_message_sequence")
    try:
        msgs = json.loads(item)
    except ValueError:
        msgs = None
    ok = isinstance(msgs, list) and len(msgs) > 0 and all(isinstance(m, dict) for m in msgs)
    c.branch(ok)                                                          # B1
    if not ok:
        c.branch(False); c.branch(False); c.branch(False)                 # B2-4
        c.valstr("0|")                                                    # V
        return c.finalize()
    adj_asst = any(
        msgs[k - 1].get("role") == "assistant" and msgs[k].get("role") == "assistant"
        for k in range(1, len(msgs)))
    has_tool = any(m.get("role") == "tool" for m in msgs)
    adj_user = any(
        msgs[k - 1].get("role") == "user" and msgs[k].get("role") == "user"
        for k in range(1, len(msgs)))
    c.branch(adj_asst)                                                    # B2
    c.branch(has_tool)                                                    # B3
    c.branch(adj_user)                                                    # B4

    work = []
    for m in msgs:
        mm = dict(m)
        # The sub-contract encodes an assistant turn's tool_calls via the
        # ';'/','-joined tool_call_ids string; expand it into the dict
        # entries LIVE's merge logic actually manipulates.
        ids = mm.pop("tool_call_ids", None)
        if ids:
            tcs = []
            for entry in ids.split(";"):
                parts = entry.split(",")
                tc = {"id": parts[0]}
                if len(parts) > 1:
                    tc["call_id"] = parts[1]
                tcs.append(tc)
            mm["tool_calls"] = tcs
        work.append(mm)
    repairs = _fn(None, work)
    dump = "%d|%s" % (repairs, "".join(_canon(m) for m in work))
    c.valstr(dump)                                                        # V
    return c.finalize()


def run(inputs):
    return [trace(m) for m in inputs]
