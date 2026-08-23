"""twin for agent_shell_hooks_parse_response.

Traces the LIVE agent/shell_hooks.py::_parse_response over the shared rung
ladder of src/agent/shell_hooks_ports.c::shk_parse_response:

  B1 empty/whitespace stdout / B2 parse-fail-or-non-dict /
  B3 block directive (canonical action== or Claude-Code decision==;
     message priority resolved via LIVE _block_message) /
  B4 context pass-through / B5 exhausted.

The C port takes (stdout_json) without an event parameter and merges the two
block checks into one combined rung — the twin traces that merged ladder using
LIVE data (live json.loads + live _block_message), so semantics stay LIVE and
structure is the shared ladder.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_shell_hooks", os.path.join(DEV, "agent", "shell_hooks.py"))
_shk = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_shell_hooks"] = _shk
try:
    _spec.loader.exec_module(_shk)
    live_block_message = _shk._block_message
except Exception:  # pragma: no cover
    def live_block_message(primary, secondary):
        raw = primary or secondary
        DEFAULT = "Blocked by shell hook."
        return raw if isinstance(raw, str) and raw else DEFAULT


def _is_block_str(v):
    return isinstance(v, str) and v == "block"


def trace(stdout_text):
    c = Cup("parse_response")
    s = (stdout_text or "").strip()
    if not s:                                    # B1
        c.branch(True)
        return c.finalize()
    c.branch(False)
    try:
        data = json.loads(s)
    except Exception:
        data = None
    if not isinstance(data, dict):               # B2
        c.branch(True)
        return c.finalize()
    c.branch(False)
    action = data.get("action")
    decision = data.get("decision")
    if _is_block_str(action) or _is_block_str(decision):       # B3
        c.branch(True)
        if _is_block_str(action):
            primary, fallback = data.get("message"), data.get("reason")
        else:
            primary, fallback = data.get("reason"), data.get("message")
        _ = live_block_message(primary, fallback)
        return c.finalize()
    c.branch(False)
    context = data.get("context")
    if isinstance(context, str) and context:                   # B4 (C ladder:
        c.branch(True)                                         # non-empty string)
        return c.finalize()
    c.branch(False)                                            # B5
    return c.finalize()


def run(inputs):
    return [trace(raw) for raw in inputs]
