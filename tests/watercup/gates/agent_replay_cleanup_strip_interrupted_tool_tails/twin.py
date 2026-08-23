"""twin for agent_replay_cleanup_strip_interrupted_tool_tails.

Traces the LIVE agent/replay_cleanup.py::strip_interrupted_tool_tails with the
same rung order as the C harness. Semantics are LIVE: is_interrupted_tool_result
comes from the LIVE module and tool_may_have_side_effect from the LIVE
agent/tool_result_classification.py.

  B1 empty / LOOP msgs (B2 assistant+tool_calls) / LOOP tool scan (B3) /
  B4 interrupted-block / LOOP calls (B5 side-effect) / B6 recover-vs-strip /
  LOOP results (B7 keep) with V disposition / B8 orphan drop.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup


def _load(name, rel):
    spec = ilu.spec_from_file_location(name, os.path.join(DEV, rel))
    mod = ilu.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

_rc = _load("wc_live_agent_replay_cleanup", "agent/replay_cleanup.py")
_trc = _load("wc_live_agent_tool_result_classification",
             "agent/tool_result_classification.py")
is_interrupted = _rc.is_interrupted_tool_result
side_effect = _trc.tool_may_have_side_effect


def _content(msg):
    c = msg.get("content", "")
    return c if isinstance(c, str) else ""


def trace(history):
    c = Cup("strip_interrupted_tool_tails")
    if not history:
        c.branch(True)                                   # B1
        return history, c.finalize()
    c.branch(False)
    cleaned = []
    i, n = 0, len(history)
    while i < n:
        c.loop()
        msg = history[i]
        role = msg.get("role")
        asst_tc = role == "assistant" and "tool_calls" in msg
        c.branch(asst_tc)                                # B2
        if asst_tc:
            j = i + 1
            while j < n:
                c.loop()                                 # B3 loop
                is_tool = history[j].get("role") == "tool"
                c.branch(is_tool)                        # B3
                if not is_tool:
                    break
                j += 1
            tool_results = history[i + 1:j]
            any_int = any(is_interrupted(m.get("content", "")) for m in tool_results)
            block = bool(tool_results) and any_int
            c.branch(block)                              # B4
            if block:
                calls = msg.get("tool_calls") or []
                may = False
                for call in calls:                       # B5 loop
                    c.loop()
                    name = str((call.get("function") or {}).get("name") or "")
                    se = side_effect(name)
                    c.branch(se)                         # B5
                    if se:
                        may = True
                        break
                c.branch(may)                            # B6
                if may:
                    cleaned.append(msg)
                    call_names = {
                        str(call.get("id") or call.get("call_id") or ""): str(
                            (call.get("function") or {}).get("name") or "")
                        for call in calls
                    }
                    for tool_result in tool_results:     # B7 loop
                        c.loop()
                        keep = not is_interrupted(tool_result.get("content", ""))
                        c.branch(keep)                   # B7
                        if keep:
                            cleaned.append(tool_result)
                            continue
                        name = call_names.get(
                            str(tool_result.get("tool_call_id") or ""), "")
                        disp = "unknown" if side_effect(name) else "none"
                        c.valstr(disp)
                i = j
                continue
        orphan = role == "tool" and is_interrupted(msg.get("content", ""))
        c.branch(orphan)                                 # B8
        if orphan:
            i += 1
            continue
        cleaned.append(msg)
        i += 1
    return cleaned, c.finalize()


def run(inputs):
    import json as _json
    cups = []
    for raw in inputs:
        try:
            hist = _json.loads(raw)
        except Exception:
            hist = None
        _, cup = trace(hist if isinstance(hist, list) else [])
        cups.append(cup)
    return cups
