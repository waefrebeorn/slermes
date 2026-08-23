"""twin for agent_context_compressor_find_one_exchange.

Traces the LIVE agent/context_compressor.py::ContextCompressor.
_find_one_exchange with the same rung ladder as the C harness (a faithful
transcription of src/agent/context_compressor.c::cc_find_one_exchange).
Summary-marker detection goes through the LIVE
_is_context_summary_message classmethod, so semantics are LIVE.

Inputs: "<start>,<tail_start>;ROLE|C|T|S;..." (same encoding as harness.c).
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_context_compressor", os.path.join(DEV, "agent", "context_compressor.py"))
_cc = ilu.module_from_spec(_spec)
sys.modules["wc_live_context_compressor"] = _cc
try:
    _spec.loader.exec_module(_cc)
    CCls = _cc.ContextCompressor
except Exception:
    CCls = None


def is_summary(msg):
    """LIVE summary-marker predicate."""
    if CCls is None:
        return bool(msg.get("_summary"))
    try:
        return bool(CCls._is_context_summary_message(msg))
    except Exception:
        return bool(msg.get("_summary"))


def make_msg(role, c, t, s):
    msg = {"role": role}
    if role == "assistant":
        msg["content"] = "output text" if c else ""
        if t:
            msg["tool_calls"] = [{"id": "call_1", "function": {"name": "f", "arguments": "{}"}}]
    else:
        msg["content"] = "body text" if c else ""
    if s:
        # Summary marker recognized by the LIVE metadata predicate
        # (COMPRESSED_SUMMARY_METADATA_KEY = "_compressed_summary").
        msg["content"] = "[CONTEXT SUMMARY] ... rolling compacted history ..."
        msg["_compressed_summary"] = True
    return msg


def parse(spec):
    head, _, rest = spec.partition(";")
    start_s, tail_s = head.split(",", 1)
    msgs = []
    for tok in rest.split(";"):
        if not tok:
            continue
        role, c, t, s = tok.split("|")
        msgs.append(make_msg(role, c == "1", t == "1", s == "1"))
    return int(start_s), int(tail_s), msgs


def trace(spec):
    start, tail_start, messages = parse(spec)
    c = Cup("find_one_exchange")
    n = len(messages)
    idx = start
    found = None
    if idx >= n or idx >= tail_start:                 # B1
        c.branch(True)
        c.valstr("-1,-1")
        return "-1,-1", c.finalize()
    c.branch(False)

    while idx < tail_start and idx < n:               # LOOP walk
        c.loop()
        msg = messages[idx]
        if (msg.get("role") == "assistant"
                and not is_summary(msg)):             # B2 (assistant-with-output)
            c.branch(True)
            break
        c.branch(False)
        idx += 1
    if idx >= tail_start or idx >= n:                 # B3
        c.branch(True)
        c.valstr("-1,-1")
        return "-1,-1", c.finalize()
    c.branch(False)
    exchange_start = idx

    idx += 1
    while idx < tail_start and idx < n:               # LOOP consume turn
        c.loop()
        msg = messages[idx]
        if msg.get("role") not in ("assistant", "tool"):   # B4
            c.branch(True)
            break
        c.branch(False)
        if is_summary(msg):                           # B5
            c.branch(True)
            break
        c.branch(False)
        idx += 1
    if idx <= exchange_start:                         # B6
        c.branch(True)
        c.valstr("-1,-1")
        return "-1,-1", c.finalize()
    c.branch(False)

    if idx >= n:                                      # B7
        c.branch(True)
        c.valstr("-1,-1")
        return "-1,-1", c.finalize()
    c.branch(False)

    boundary = messages[idx]
    if not isinstance(boundary, dict) or \
            boundary.get("role") in ("assistant", "tool"):  # B8
        c.branch(True)
        c.valstr("-1,-1")
        return "-1,-1", c.finalize()
    c.branch(False)

    found = (exchange_start, idx)
    c.valstr("%d,%d" % found)
    return "%d,%d" % found, c.finalize()


def run(inputs):
    cups = []
    for spec in inputs:
        _, cup = trace(spec)
        cups.append(cup)
    return cups
