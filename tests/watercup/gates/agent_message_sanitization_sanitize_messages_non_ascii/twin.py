"""twin for agent_message_sanitization_sanitize_messages_non_ascii.

Traces the LIVE agent/message_sanitization.py::
_sanitize_messages_non_ascii with the same rung ladder as the C harness
(a faithful transcription of the fixed msz_sanitize_messages_non_ascii
core in src/agent/message_sanitization_ports.c).

Inputs: a JSON array of messages (same encoding as harness.c argv).
"""
import json
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_message_sanitization",
    os.path.join(DEV, "agent", "message_sanitization.py"))
_msz = ilu.module_from_spec(_spec)
sys.modules["wc_live_message_sanitization"] = _msz
try:
    _spec.loader.exec_module(_msz)
    live_fn = _msz._sanitize_messages_non_ascii
    strip_fn = _msz._strip_non_ascii
except Exception:
    live_fn = None
    strip_fn = lambda s: s.encode("ascii", errors="ignore").decode("ascii")

_SKIP = {"content", "name", "tool_calls", "role"}


def trace(spec):
    messages = json.loads(spec)
    c = Cup("sanitize_messages_non_ascii")
    found = False
    for msg in messages:                                     # L0
        c.loop()
        if not isinstance(msg, dict):                        # B1
            c.branch(False)
            continue
        c.branch(True)

        content = msg.get("content")
        if isinstance(content, str):                         # B2
            c.branch(True)
            sanitized = strip_fn(content)
            if sanitized != content:                         # B3
                c.branch(True)
                msg["content"] = sanitized
                found = True
            else:
                c.branch(False)
        else:
            c.branch(False)
            if isinstance(content, list):                    # B4
                c.branch(True)
                for part in content:                         # L1
                    c.loop()
                    if isinstance(part, dict):               # B5
                        c.branch(True)
                        text = part.get("text")
                        if isinstance(text, str):            # B6
                            c.branch(True)
                            sanitized = strip_fn(text)
                            if sanitized != text:            # B7
                                c.branch(True)
                                part["text"] = sanitized
                                found = True
                            else:
                                c.branch(False)
                        else:
                            c.branch(False)
                    else:
                        c.branch(False)
            else:
                c.branch(False)

        name = msg.get("name")
        if isinstance(name, str):                            # B8
            c.branch(True)
            sanitized = strip_fn(name)
            if sanitized != name:                            # B9
                c.branch(True)
                msg["name"] = sanitized
                found = True
            else:
                c.branch(False)
        else:
            c.branch(False)

        tool_calls = msg.get("tool_calls")
        if isinstance(tool_calls, list):                     # B10
            c.branch(True)
            for tc in tool_calls:                            # L2
                c.loop()
                if isinstance(tc, dict):                     # B11
                    c.branch(True)
                    fn = tc.get("function", {})
                    if isinstance(fn, dict):                 # B12
                        c.branch(True)
                        fn_args = fn.get("arguments")
                        if isinstance(fn_args, str):         # B13
                            c.branch(True)
                            sanitized = strip_fn(fn_args)
                            if sanitized != fn_args:         # B14
                                c.branch(True)
                                fn["arguments"] = sanitized
                                found = True
                            else:
                                c.branch(False)
                        else:
                            c.branch(False)
                    else:
                        c.branch(False)
                else:
                    c.branch(False)
        else:
            c.branch(False)

        for key, value in list(msg.items()):                 # L3
            c.loop()
            if key in _SKIP:                                 # B15
                c.branch(True)
                continue
            c.branch(False)
            if isinstance(value, str):                       # B16
                c.branch(True)
                sanitized = strip_fn(value)
                if sanitized != value:                       # B17
                    c.branch(True)
                    msg[key] = sanitized
                    found = True
                else:
                    c.branch(False)
            else:
                c.branch(False)

    res = "1" if found else "0"
    c.valstr(res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for spec in inputs:
        _, cup = trace(spec)
        cups.append(cup)
    return cups
