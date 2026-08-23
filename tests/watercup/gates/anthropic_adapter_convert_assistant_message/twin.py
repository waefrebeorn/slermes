"""twin for anthropic_adapter_convert_assistant_message — LIVE
agent/anthropic_adapter.py::_convert_assistant_message traced at the same
coarse rungs as the C harness around the REAL port in provider_anthropic.c.
"""
import os, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

_fn = None
try:
    import importlib.util as ilu
    spec = ilu.spec_from_file_location("wc_live_antr", os.path.join(DEV, "agent", "anthropic_adapter.py"))
    mod = ilu.module_from_spec(spec)
    sys.modules["wc_live_antr"] = mod
    spec.loader.exec_module(mod)
    _fn = mod._convert_assistant_message
except Exception:
    # source-extract the helper cluster + target fn
    src = open(os.path.join(DEV, "agent", "anthropic_adapter.py")).read()
    start = src.index("def _sanitize_tool_id")
    end = src.index("def _convert_tool_message_to_result")
    ns = {"json": json, "copy": __import__("copy")}
    exec(src[start:end], ns)
    _fn = ns["_convert_assistant_message"]

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("_convert_assistant_message")
        m = json.loads(raw)
        result = _fn(m)

        ob = m.get("anthropic_content_blocks")
        c.branch(isinstance(ob, list) and len(ob) > 0)

        rd = m.get("reasoning_details")
        rd_list = isinstance(rd, list)
        c.branch(rd_list)
        if rd_list:
            for d in rd:
                ok = isinstance(d, dict) and str(d.get("type", "") or "").strip().lower() in (
                    "thinking", "redacted_thinking")
                c.branch(ok)
                c.loop()

        content = m.get("content", "")
        c.branch(bool(content))
        c.branch(isinstance(content, list))
        tcs = m.get("tool_calls")
        has_tcs = isinstance(tcs, list) and len(tcs) > 0
        c.branch(has_tcs)
        if has_tcs:
            for tc in tcs:
                good = bool(tc) and isinstance(tc, dict) and "function" in tc
                c.branch(good)
                c.loop()
        rc = m.get("reasoning_content")
        rc_str = isinstance(rc, str)
        c.branch(rc_str)
        if rc_str:
            already = any(isinstance(b, dict) and b.get("type") in ("thinking", "redacted_thinking")
                          for b in (result["content"]))
            # guard is evaluated pre-injection; recompute from rd-preserved blocks
            preserved = []
            if rd_list:
                for d in rd:
                    if isinstance(d, dict) and str(d.get("type", "") or "").strip().lower() in (
                            "thinking", "redacted_thinking"):
                        preserved.append(d)
            already2 = any(b.get("type") in ("thinking", "redacted_thinking") for b in preserved)
            c.branch(not already2)

        blocks = result["content"]
        only_ph = (len(blocks) == 1 and blocks[0].get("type") == "text"
                   and blocks[0].get("text") == "(empty)")
        c.branch(only_ph)
        c.branch(isinstance(m.get("cache_control"), dict))

        c.valstr(json.dumps(blocks, separators=(",", ":")))
        cups.append(c.finalize())
    return cups
