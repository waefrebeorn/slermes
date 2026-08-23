"""twin for toolsets_resolve_includes.

Traces the LIVE toolsets.py::resolve_toolset (include_registry=False) with the
same rung order as the C harness:
  A1 all-alias / A2 visited(diamond/cycle) / A3 not-found /
  A4 direct tools / A5 includes. V: result class.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup
import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_toolsets",
                                   os.path.join(DEV, "toolsets.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_toolsets"] = mod
spec.loader.exec_module(mod)


def resolve_trace(name, visited, acc):
    if name in {"all", "*"}:                                  # A1
        c.branch(True)
        for ts_name in mod.get_toolset_names():
            resolve_trace(ts_name, visited.copy(), acc)
        return
    c.branch(False)

    if name in visited:                                       # A2
        c.branch(True)
        return []
    c.branch(False)
    visited.add(name)

    toolset = mod.get_toolset(name, include_registry=False)
    if not toolset:                                           # A3
        c.branch(True)
        return
    c.branch(False)

    if toolset.get("tools"):                                  # A4
        c.branch(True)
        acc.update(toolset["tools"])
    else:
        c.branch(False)

    includes = toolset.get("includes", [])                    # A5
    if includes:
        c.branch(True)
        for inc in includes:
            resolve_trace(inc, visited, acc)
    else:
        c.branch(False)


def run(inputs):
    global c
    cups = []
    for name in inputs:
        c = Cup("toolsets_resolve_includes")
        acc = set()
        result = mod.resolve_toolset(name, set(), include_registry=False)
        resolve_trace(name, set(), acc)
        c.valstr("nonempty" if result else "empty")
        assert (sorted(acc) == sorted(result)) or not acc, "twin drift"
        cups.append(c.finalize())
    return cups
