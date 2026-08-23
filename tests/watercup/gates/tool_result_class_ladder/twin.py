"""twin for tool_result_class_ladder.

Traces the LIVE agent/tool_result_classification.py functions with the SAME
rung order as the C harness:
  fl: B1 mutating tool / B2 parses+dict / B3 error falsy /
      B4 tool==write_file / B5 landed signal present
  se: B1 side-effect possible
The LIVE fns' own results are asserted against the mirrored outcome.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_tool_result_classification",
    os.path.join(DEV, "agent", "tool_result_classification.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_tool_result_classification"] = _live
_spec.loader.exec_module(_live)


def _emit_false(c):
    c.valstr("0")
    return c.finalize()


def _trace_landed(tool_name, result):
    c = Cup("file_mutation_result_landed")

    # B1: mutating tool
    mutating = tool_name in _live.FILE_MUTATING_TOOL_NAMES
    c.branch(mutating)
    if not mutating:
        return _emit_false(c)

    # B2: parses into a dict
    try:
        data = json.loads(result.strip())
        is_dict = isinstance(data, dict)
    except Exception:
        data, is_dict = None, False
    c.branch(is_dict)
    if not is_dict:
        return _emit_false(c)

    # B3: data["error"] falsy
    err_truthy = bool(data.get("error"))
    c.branch(not err_truthy)
    if err_truthy:
        return _emit_false(c)

    # B4: which mutator
    is_write = (tool_name == "write_file")
    c.branch(is_write)
    if is_write:
        ok = "bytes_written" in data          # B5
    else:
        ok = data.get("success") is True      # B5
    c.branch(ok)
    c.valstr("1" if ok else "0")

    live = _live.file_mutation_result_landed(tool_name, result)
    assert live == ok, "LIVE %r != mirrored %r for %s %s" % (live, ok, tool_name, result)
    return c.finalize()


def _trace_side_effect(tool):
    c = Cup("tool_may_have_side_effect")
    se = tool not in _live.NO_EFFECT_TOOL_NAMES
    c.branch(se)
    res = "1" if se else "0"
    c.valstr(res)
    live = _live.tool_may_have_side_effect(tool)
    assert live == se
    return c.finalize()


def run(inputs):
    out = []
    for inp in inputs:
        if inp.startswith("se:"):
            out.append(_trace_side_effect(inp[3:]))
        elif inp.startswith("fl:"):
            tool, result = inp[3:].split("\t", 1)
            out.append(_trace_landed(tool, result))
        else:
            raise ValueError("bad input %r" % inp)
    return out
