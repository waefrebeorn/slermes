"""twin for anthropic_adapter_sanitize_replay_block.

Traces the LIVE agent/anthropic_adapter.py::_sanitize_replay_block ladder
with the SAME rung order as the C harness, and asserts the traced outcome
against the LIVE fn's actual return on every input.
"""
import importlib.util as ilu
import json as _json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_anthropic_adapter", os.path.join(DEV, "agent", "anthropic_adapter.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_anthropic_adapter"] = mod
spec.loader.exec_module(mod)
_sanitize = mod._sanitize_replay_block

from watercup_py import Cup


def trace_one(raw):
    c = Cup("sanitize_replay_block")
    b = None
    try:
        parsed = _json.loads(raw)
        if isinstance(parsed, dict):
            b = parsed
    except (_json.JSONDecodeError, ValueError):
        pass

    c.branch(b is not None)                                    # B1
    if b is None:
        outcome = "drop"
        got = _sanitize(b)  # None input → LIVE isinstance check fails → None
        assert got is None, raw
        c.valstr(outcome)
        return c.finalize()

    btype = b.get("type")
    t = btype if isinstance(btype, str) else ""

    if t == "text":                                            # B2
        c.branch(True)
        tv = b.get("text")
        nonblank = isinstance(tv, str) and bool(tv.strip())
        c.branch(nonblank)                                     # B3
        cits = b.get("citations")
        c.branch(isinstance(cits, list) and len(cits) > 0)     # B4
        cc = b.get("cache_control")
        c.branch(isinstance(cc, dict))                         # B5
        outcome = "kept:text" if nonblank else "drop"
        got = _sanitize(b)
        if nonblank:
            assert isinstance(got, dict) and got.get("type") == "text", raw
        else:
            assert got is None, raw
        c.valstr(outcome)
        return c.finalize()
    c.branch(False)

    if t == "thinking":                                        # B6
        c.branch(True)
        sig = b.get("signature")
        sigok = isinstance(sig, str) and bool(sig)
        c.branch(sigok)                                        # B7
        got = _sanitize(b)
        assert isinstance(got, dict) and got.get("type") == "thinking", raw
        c.valstr("kept:thinking")
        return c.finalize()
    c.branch(False)

    if t == "redacted_thinking":                               # B8
        c.branch(True)
        has_data = b.get("data") is not None
        c.branch(has_data)                                     # B9
        got = _sanitize(b)
        if has_data:
            assert isinstance(got, dict), raw
            outcome = "kept:redacted_thinking"
        else:
            assert got is None, raw
            outcome = "drop"
        c.valstr(outcome)
        return c.finalize()
    c.branch(False)

    if t == "tool_use":                                        # B10
        c.branch(True)
        cc = b.get("cache_control")
        c.branch(isinstance(cc, dict))                         # B11
        got = _sanitize(b)
        assert isinstance(got, dict) and got.get("type") == "tool_use", raw
        c.valstr("kept:tool_use")
        return c.finalize()
    c.branch(False)

    if t == "image":                                           # B12
        c.branch(True)
        src_ok = isinstance(b.get("source"), dict)
        c.branch(src_ok)                                       # B13
        got = _sanitize(b)
        if src_ok:
            assert isinstance(got, dict), raw
            outcome = "kept:image"
        else:
            assert got is None, raw
            outcome = "drop"
        c.valstr(outcome)
        return c.finalize()
    c.branch(False)

    assert _sanitize(b) is None, raw
    c.valstr("drop")
    return c.finalize()


def _looks_json(raw):
    try:
        _json.loads(raw)
        return True
    except Exception:
        return False


def run(inputs):
    return [trace_one(x) for x in inputs]


if __name__ == "__main__":
    inputs = __import__("json").load(open(os.path.join(HERE, "gate.json")))["inputs"]
    print("\n".join(run(inputs)))
