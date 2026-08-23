"""twin for agent_runtime_helpers_repair_tool_call.

Imports LIVE agent/agent_runtime_helpers.py::repair_tool_call and traces the
same rung ladder as the C harness (a traced copy of the REAL port
src/tools/registry.c::repair_tool_call).  Each branch rung is computed with
LIVE's own _norm/_camel_snake/_strip_tool_suffix helpers; the final value
comes from the LIVE function itself (difflib fuzzy rung included).

Ladder: B1 empty, B2 lowered, B3 normalized, B4 camel-snake,
B5 any suffix-stripped variant.  Value: repaired name or "" for None.
Inputs avoid the documented divergence regions: no XML quote/angle chars
(LIVE-only trim step), no ALL-CAPS-internal CamelCase (regex vs boundary
split), and junk strings long enough that both fuzzy algorithms stay under
the 0.7 cutoff.
"""
import os
import sys
import re
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_arh", os.path.join(DEV, "agent", "agent_runtime_helpers.py"))
_arh = ilu.module_from_spec(_spec)
sys.modules["wc_live_arh"] = _arh
_spec.loader.exec_module(_arh)

NAMES = ["terminal", "read_file", "write_file", "web_search", "todo_tool"]


class _FakeAgent:
    valid_tool_names = frozenset(NAMES)


def _norm(s):
    return s.lower().replace("-", "_").replace(" ", "_")


def _camel_snake(s):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", s).lower()


def _strip_tool_suffix(s):
    lc = s.lower()
    for suffix in ("_tool", "-tool", "tool"):
        if lc.endswith(suffix) and len(s) > len(suffix):
            return s[: -len(suffix)].rstrip("_-")
    return None


def trace(tool_name):
    c = Cup("repair_tool_call")
    if not tool_name:                                    # B1
        c.branch(True)
        res = _arh.repair_tool_call(_FakeAgent(), tool_name)
        c.valstr(res if res else "")
        return c.finalize()
    c.branch(False)

    lowered = tool_name.lower()
    hit_lowered = lowered in NAMES                       # B2
    c.branch(hit_lowered)

    normalized = _norm(tool_name)
    hit_norm = not hit_lowered and normalized in NAMES   # B3
    c.branch(hit_norm)

    camel = _camel_snake(tool_name)
    hit_camel = not hit_norm and camel in NAMES          # B4
    c.branch(hit_camel)

    # Variant set — same construction as LIVE's cands set.
    cands = [tool_name, lowered, normalized, _camel_snake(tool_name)]
    for _ in range(2):
        extra = []
        for cand in list(dict.fromkeys(cands)):
            stripped = _strip_tool_suffix(cand)
            if stripped:
                extra.extend([stripped, _norm(stripped), _camel_snake(stripped)])
        for e in extra:
            if e not in cands:
                cands.append(e)
    hit_variant = not hit_camel and any(x and x in NAMES for x in cands)  # B5
    c.branch(hit_variant)

    res = _arh.repair_tool_call(_FakeAgent(), tool_name)
    c.valstr(res if res else "")
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
