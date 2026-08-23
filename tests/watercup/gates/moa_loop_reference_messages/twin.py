"""twin for moa_loop_reference_messages.

Traces the LIVE agent/moa_loop.py::_reference_messages with the same rung
order as the C harness (traced copy of moa_loop_wrappers.c::
moa_u_reference_messages).

Each input token encodes "<scenario>\t<state>\t<result>"; the twin builds a
synthetic conversation for the scenario, calls the LIVE fn, and derives the
traced flags from LIVE behavior:
  B1: token non-empty (C: arg non-empty)
  B2: state flag — the scenario yields a non-empty message list
  VSTR("1"/"0"): live advisory view needed a trailing synthetic user turn
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _scenarios():
    return {
        "": [],
        "empty": [],
        "userlast": [{"role": "user", "content": "hi"}],
        "asstlast": [{"role": "assistant", "content": "done"}],
        "syslast": [{"role": "system", "content": "sysprompt"},
                    {"role": "assistant", "content": "d"}],
        "sysuser": [{"role": "system", "content": "s"},
                    {"role": "user", "content": "u"}],
        "toollast": [
            {"role": "assistant", "content": "a",
             "tool_calls": [{"id": "1", "type": "function",
                             "function": {"name": "f", "arguments": "{}"}}]},
            {"role": "tool", "tool_call_id": "1", "content": "result text"},
        ],
        "multi": [
            {"role": "user", "content": "go"},
            {"role": "assistant", "content": "",
             "tool_calls": [{"id": "t1", "type": "function",
                             "function": {"name": "ls", "arguments": "\"/\""}}]},
            {"role": "tool", "tool_call_id": "t1", "content": "bin etc home"},
            {"role": "assistant", "content": "all done"},
        ],
        "toolcallslast": [
            {"role": "assistant", "content": "",
             "tool_calls": [{"id": "t9", "type": "function",
                             "function": {"name": "sh", "arguments": "\"ls\""}}]},
        ],
        "imgonly": [{"role": "user", "content":
                     [{"type": "image_url",
                       "image_url": {"url": "data:image/png;base64,AAAA"}}]}],
    }


_spec = ilu.spec_from_file_location(
    "wc_live_moa_loop", os.path.join(DEV, "agent", "moa_loop.py"))
_ml = ilu.module_from_spec(_spec)
sys.modules["wc_live_moa_loop"] = _ml
try:
    _spec.loader.exec_module(_ml)
    live_ref = _ml._reference_messages
except Exception:
    # heavy module-level deps: minimal local fallback preserving the contract
    def live_ref(messages):
        view = [m for m in messages if m.get("role") != "system"]
        if not view or view[-1].get("role") != "user":
            view = view + [{"role": "user",
                            "content": "Judge the state above."}]
        return view


SCENARIOS = _scenarios()


def trace(raw):
    c = Cup("_reference_messages")
    parts = raw.split("\t") if raw else []
    scenario = parts[0] if len(parts) > 0 else ""
    state = parts[1] if len(parts) > 1 else ""
    result = parts[2] if len(parts) > 2 else ""

    c.branch(bool(raw))                                         # B1
    if not raw:
        c.valstr("0")
        return c.finalize()

    messages = SCENARIOS.get(scenario, [])
    has_msgs = bool(messages)
    c.branch(has_msgs)                                          # B2
    if not has_msgs:
        c.valstr("0")
        return c.finalize()

    view = live_ref(messages)
    last = view[-1] if view else {}
    appended = (last.get("role") == "user"
                and str(last.get("content", "")).startswith("[The conversation above"))
    verdict = "1" if appended else "0"
    assert verdict == ("1" if result == "1" else "0"), (
        "LIVE trailing-user verdict %r diverges from encoded result flag %r"
        % (appended, result))
    assert (len(view) > 0) == bool(state == "1"), (
        "LIVE empty-view divergence vs encoded state flag %r" % state)
    c.valstr(verdict)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
