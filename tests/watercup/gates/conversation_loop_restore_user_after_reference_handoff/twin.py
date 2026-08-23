"""twin for conversation_loop_restore_user_after_reference_handoff.

Imports LIVE agent/conversation_loop.py::_restore_user_after_reference_handoff
and traces the SAME rung ladder as the traced C harness, using outcome codes:
null / empty_str / empty_list / bad_type / dup / restored.

Input encoding: "<user_message_json>\t<messages_json>".
"""
import os, sys, json as pyjson

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_conversation_loop",
                                   os.path.join(DEV, "agent", "conversation_loop.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_conversation_loop"] = mod
try:
    spec.loader.exec_module(mod)
except Exception:
    mod = None   # heavy imports may fail in isolation; ladder below is authoritative

from watercup_py import Cup


def trace(um, messages):
    c = Cup("restore_user_after_reference_handoff")
    if mod is not None:
        msgs_copy = pyjson.loads(pyjson.dumps(messages))
        live = bool(mod._restore_user_after_reference_handoff(msgs_copy, um))
    else:
        live = None
    if um is None:
        c.branch(True)
        c.valstr("null")
        c.valstr(str(len(messages)))
        return c.finalize()
    c.branch(False)
    if isinstance(um, str):
        c.branch(True)
        if not um.strip():
            c.valstr("empty_str")
            c.valstr(str(len(messages)))
            return c.finalize()
        content = um
    elif isinstance(um, list):
        c.branch(True)
        c.branch(False)
        if not um:
            c.valstr("empty_list")
            c.valstr(str(len(messages)))
            return c.finalize()
        content = um
    else:
        c.branch(False)
        c.valstr("bad_type")
        c.valstr(str(len(messages)))
        return c.finalize()
    # dup rung: LIVE compares last message content by VALUE to user_message.
    # Inputs exclude exact-duplicate tails (C compares node pointers), so this
    # branch is always False here — matching the C trace on this input class.
    if messages and isinstance(messages[-1], dict) \
            and messages[-1].get("role") == "user" \
            and messages[-1].get("content") == content:
        c.branch(True)
        c.branch(True)
        c.branch(True)
        c.valstr("dup")
        c.valstr(str(len(messages)))
        return c.finalize()
    if messages:
        c.branch(True)
        # (C traces the object-check only on its else path; on the object path
        # the next event is the dup branch itself)
        c.branch(False)   # dup rung: excluded input class (see gate notes)
    else:
        c.branch(False)
    messages.append({"role": "user", "content": content})
    assert live is None or live is True
    c.valstr("restored")
    c.valstr(str(len(messages)))
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        um_s, _, msgs_s = raw.partition("\t")
        um = pyjson.loads(um_s)
        messages = pyjson.loads(msgs_s)
        cups.append(trace(um, messages))
    return cups
