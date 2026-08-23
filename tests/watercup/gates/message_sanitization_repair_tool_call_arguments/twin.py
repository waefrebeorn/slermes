"""twin for message_sanitization_repair_tool_call_arguments.

Traces the LIVE agent/message_sanitization.py::_repair_tool_call_arguments
with the same rung order as the C harness. Semantics are LIVE: the parse
oracle is json.loads (strict=False for pass 0, strict for later passes), the
trailing-comma regex and bounded trim loop mirror the LIVE source, and the
escape pass calls the LIVE _escape_invalid_chars_in_json_strings.

  B1 empty / B2 "None" / B3 strict-parse (pass 0) /
  LOOP trim (B4 parses, B5 chop '}', B6 chop ']') / B7 repaired /
  B8 escape pass / B9 fallback "{}".
"""
import os, sys, json as _json, importlib.util as ilu

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


_msz = _load("wc_live_agent_message_sanitization", "agent/message_sanitization.py")
_escape_ctrl = _msz._escape_invalid_chars_in_json_strings

import re as _re
_TRAILING_COMMA = _re.compile(r',\s*([}\]])')


def trace(raw_args):
    c = Cup("repair_tool_call_arguments")
    raw_stripped = raw_args.strip() if isinstance(raw_args, str) else ""

    if not raw_stripped:                                     # B1
        c.branch(True)
        return "{}", c.finalize()
    c.branch(False)

    if raw_stripped == "None":                               # B2
        c.branch(True)
        return "{}", c.finalize()
    c.branch(False)

    try:                                                     # B3 (pass 0)
        parsed = _json.loads(raw_stripped, strict=False)
        reserialised = _json.dumps(parsed, separators=(",", ":"))
        c.branch(True)
        return reserialised, c.finalize()
    except (_json.JSONDecodeError, TypeError, ValueError):
        c.branch(False)

    fixed = raw_stripped
    fixed = _TRAILING_COMMA.sub(r'\1', fixed)
    open_curly = fixed.count('{') - fixed.count('}')
    open_bracket = fixed.count('[') - fixed.count(']')
    if open_curly > 0:
        fixed += '}' * open_curly
    if open_bracket > 0:
        fixed += ']' * open_bracket

    for _ in range(50):                                      # trim loop
        c.loop()
        try:
            _json.loads(fixed)
            c.branch(True)                                   # B4
            break
        except _json.JSONDecodeError:
            c.branch(False)
        if fixed.endswith('}') and fixed.count('}') > fixed.count('{'):
            c.branch(True)                                   # B5
            fixed = fixed[:-1]
        elif fixed.endswith(']') and fixed.count(']') > fixed.count('['):
            c.branch(False)                                  # B5
            c.branch(True)                                   # B6
            fixed = fixed[:-1]
        else:
            c.branch(False)                                  # B5
            c.branch(False)                                  # B6
            break

    try:
        _json.loads(fixed)
        c.branch(True)                                       # B7
        return fixed, c.finalize()
    except _json.JSONDecodeError:
        c.branch(False)

    try:                                                     # B8 (pass 4)
        escaped = _escape_ctrl(fixed)
        use = False
        if escaped != fixed:
            _json.loads(escaped)
            use = True
        c.branch(use)
        if use:
            return escaped, c.finalize()
    except (_json.JSONDecodeError, TypeError, ValueError):
        c.branch(False)

    c.branch(True)                                           # B9 fallback
    return "{}", c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        _, cup = trace(raw)
        cups.append(cup)
    return cups
