"""twin for agent_context_compressor_sanitize_tool_pairs.

Traces the LIVE agent/context_compressor.py
ContextCompressor._sanitize_tool_pairs ladder with the same rung order as the
C harness (see harness.c header). Decisions computed from the LIVE input; the
value event is a canonical encoding of the LIVE sanitized output. Input: one
JSON message array per argv item.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_cc", os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(_spec)
sys.modules["wc_live_cc"] = _cc
_spec.loader.exec_module(_cc)

_CC = _cc.ContextCompressor


def trace(item):
    c = Cup("cc_sanitize_tool_pairs")
    msgs = _json.loads(item)
    is_arr = isinstance(msgs, list)
    c.branch(is_arr)                                                  # B1

    if is_arr:
        for m in msgs:
            c.loop()
            role = m.get("role") if isinstance(m, dict) else None
            c.branch(role == "assistant")                             # B2
            if role != "assistant":
                continue
            tcs = m.get("tool_calls")
            c.branch(isinstance(tcs, list) and len(tcs) > 0)          # B3
            if not (isinstance(tcs, list) and len(tcs) > 0):
                continue
            for tc in tcs:
                c.loop()
                cid = tc.get("call_id", "") or tc.get("id", "") or "" \
                    if isinstance(tc, dict) else ""
                c.branch(bool(cid))                                   # B4

        for m in msgs:
            c.loop()
            role = m.get("role") if isinstance(m, dict) else None
            c.branch(role == "tool")                                  # B5
            if role != "tool":
                continue
            cid = m.get("tool_call_id", "")
            c.branch(bool(cid))                                       # B6

        surviving, results = set(), set()
        for m in msgs:
            if not isinstance(m, dict):
                continue
            if m.get("role") == "assistant":
                for tc in m.get("tool_calls") or []:
                    cid = (tc.get("call_id", "") or tc.get("id", "") or ""
                           if isinstance(tc, dict) else "")
                    if cid:
                        surviving.add(cid)
            elif m.get("role") == "tool":
                cid = m.get("tool_call_id", "")
                if cid:
                    results.add(cid)
        orphaned = bool(results - surviving)
        missing = bool(surviving - results)

        c.branch(orphaned)                                            # B7
        c.branch(missing)                                             # B8

        if missing:
            idx = len(msgs) - 1
            while idx >= 0:
                c.loop()
                m = msgs[idx]
                role = m.get("role") if isinstance(m, dict) else None
                c.branch(role == "tool")                              # B9
                if role != "tool":
                    break
                idx -= 1
            trailing = None
            if idx >= 0 and isinstance(msgs[idx], dict) \
                    and msgs[idx].get("role") == "assistant":
                trailing = msgs[idx]
            c.branch(trailing is not None)                            # B10

            for m in msgs:
                c.loop()
                role = m.get("role") if isinstance(m, dict) else None
                c.branch(role == "assistant")                         # B11
                if role != "assistant":
                    continue
                c.branch(m is not trailing)                           # B12
                if m is trailing:
                    continue
                tcs = m.get("tool_calls")
                c.branch(isinstance(tcs, list) and len(tcs) > 0)      # B13
                if not (isinstance(tcs, list) and len(tcs) > 0):
                    continue
                nkept = 0
                miss_set = surviving - results
                for tc in tcs:
                    c.loop()
                    cid = (tc.get("call_id", "") or tc.get("id", "") or ""
                           if isinstance(tc, dict) else "")
                    miss = bool(cid) and cid in miss_set
                    c.branch(miss)                                    # B14
                    if not miss:
                        nkept += 1
                c.branch(nkept != len(tcs))                           # B15
                if nkept == len(tcs):
                    continue
                c.branch(nkept > 0)                                   # B16
                if nkept > 0:
                    continue
                content = m.get("content")
                blank = content is None or content == "" or (
                    isinstance(content, str) and not content.strip())
                c.branch(blank)                                       # B17

    live_out = _CC._sanitize_tool_pairs(
        type("S", (), {"quiet_mode": True,
                       "_get_tool_call_id": staticmethod(
                           lambda tc: _CC._get_tool_call_id(tc))})(),
        [_json.loads(_json.dumps(x)) for x in msgs])

    n_in = len(msgs)
    roles = []
    strips = 0
    for mo in live_out:
        role = "-"
        blanked = 0
        if isinstance(mo, dict):
            role = mo.get("role") or "-"
            if mo.get("content") == "(tool call removed)":
                blanked = 1
                strips += 1
        roles.append("%s%d" % (role, blanked))
    val = "n=%d|drop=%d|strip=%d|%s" % (
        len(live_out), n_in - len(live_out), strips, ",".join(roles))
    c.valstr(val)
    return c.finalize()


def run(inputs):
    return [trace(i) for i in inputs]
