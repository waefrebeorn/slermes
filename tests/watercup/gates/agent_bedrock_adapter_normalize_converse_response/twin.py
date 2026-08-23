"""twin for agent_bedrock_adapter_normalize_converse_response.

Traces the LIVE agent/bedrock_adapter.py::normalize_converse_response with
the SAME rung order as the C harness:
  LOOP over content blocks: B1 text / B2 reasoning / B3 toolUse /
  B4 tool-calls finish fixup /
  VSTR(content|reasoning|ncalls|finish|pt|ct|ttot|cr|cw|model).
The value fields are read off the SimpleNamespace the LIVE fn returns.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_bedrock_adapter", os.path.join(DEV, "agent", "bedrock_adapter.py"))
_ba = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_bedrock_adapter"] = _ba
try:
    _spec.loader.exec_module(_ba)
    _live_fn = _ba.normalize_converse_response
except Exception:
    raise


def trace(response_json):
    c = Cup("brd_normalize_converse_response")
    try:
        resp = json.loads(response_json)
    except (json.JSONDecodeError, TypeError):
        resp = None

    if not isinstance(resp, dict):
        # mirrors the C port's non-object path: empty shape, no blocks
        blocks = []
        ns = None
    else:
        blocks = (((resp.get("output") or {}).get("message") or {}).get("content")) or []
        ns = _live_fn(resp)

    n_tool_calls = 0
    for block in (blocks if isinstance(resp, dict) else []):
        if not isinstance(block, dict):
            continue
        has_text = "text" in block
        c.branch(has_text)                              # B1
        if has_text:
            continue
        rc = block.get("reasoningContent")
        has_reasoning = isinstance(rc, dict) and bool(rc.get("text"))
        c.branch(has_reasoning)                         # B2
        if has_reasoning:
            continue
        c.branch(False)
        has_tu = "toolUse" in block
        c.branch(has_tu)                                # B3
        if has_tu:
            n_tool_calls += 1

    if ns is None:
        c.branch(False)                                 # B4
        c.valstr("||0||0|0|0|0|0|")
        return c.finalize()

    msg = ns.choices[0].message
    ch = ns.choices[0]
    content = msg.content or ""
    reasoning = getattr(msg, "reasoning_content", None) or ""
    tool_calls = msg.tool_calls or []
    assert len(tool_calls) == n_tool_calls
    usage = ns.usage
    fixup = len(tool_calls) > 0 and ch.finish_reason == "stop"
    c.branch(fixup)                                     # B4

    label = "%s|%s|%d|%s|%d|%d|%d|%d|%d|%s" % (
        content, reasoning, len(tool_calls), ch.finish_reason,
        usage.prompt_tokens, usage.completion_tokens, usage.total_tokens,
        usage.cache_read_input_tokens, usage.cache_creation_input_tokens,
        ns.model)
    c.valstr(label)
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
