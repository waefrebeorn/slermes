"""twin for message_sanitization_reapply_reasoning_echo — LIVE agent/message_sanitization.py::reapply_reasoning_echo."""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_msgsan", os.path.join(DEV, "agent", "message_sanitization.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_msgsan"] = mod
try:
    spec.loader.exec_module(mod)
    _fn = mod.reapply_reasoning_echo
except Exception:
    # source-extracted exec of the two pure fns + deps
    src = open(os.path.join(DEV, "agent", "message_sanitization.py")).read()
    ns = {"_REASONING_ECHO_RULES": []}
    s1 = src.index("def apply_reasoning_content_policy")
    s2 = src.index("def reapply_reasoning_echo")
    e2 = src.index("# ---------------------------------------------------------------------------\n# Image / multimodal parts")
    exec(src[s1:e2], ns)
    _fn = ns["reapply_reasoning_echo"]

from watercup_py import Cup


def decode(tok):
    role, rest = tok.split("|", 1)
    rc_s, rest = rest.split("|", 1)
    rs_s, tc_s = rest.split("|", 1)
    msg = {"role": role}
    if rc_s != "-":
        msg["reasoning_content"] = "" if rc_s == "~" else rc_s
    if rs_s != "-":
        msg["reasoning"] = "" if rs_s == "~" else rs_s
    if tc_s == "1":
        msg["tool_calls"] = [{"id": "t1"}]
    return msg


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("reapply_reasoning_echo")
        pad_s, _, msgs_s = (raw or "").partition("\t")
        pad = pad_s == "1"
        toks = [t for t in msgs_s.split(";") if t]
        api_messages = [decode(t) for t in toks]
        import copy
        expected = _fn(copy.deepcopy(api_messages), pad)
        changed = 0
        for msg in api_messages:
            if msg.get("role") != "assistant":
                c.branch(False)
                continue
            c.branch(True)
            has_rc = bool(msg.get("reasoning_content"))
            has_key = "reasoning_content" in msg
            if pad:
                c.branch(True)
                c.branch(has_rc)
            else:
                c.branch(False)
                c.branch(has_key)
        c.valstr(str(expected))
        cups.append(c.finalize())
    return cups
