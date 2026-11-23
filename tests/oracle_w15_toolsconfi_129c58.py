#!/usr/bin/env python3
"""Differential oracle for hermes_cli/tools_config_mcp.py (gate w15_toolsconfi_129c58)."""
import copy
import fnmatch
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg


def _stub(name, **attrs):
    m = types.ModuleType(name)
    if "." in name:
        parent = name.rsplit(".", 1)[0]
        if parent not in sys.modules:
            pm = types.ModuleType(parent)
            pm.__path__ = [QUARRY + "/" + parent.replace(".", "/")]
            sys.modules[parent] = pm
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.cli_output", print_error=lambda *a, **k: None,
      print_info=lambda *a, **k: None, print_success=lambda *a, **k: None,
      print_warning=lambda *a, **k: None)
_stub("hermes_cli.colors", Colors=types.SimpleNamespace(GREEN="", RED="", YELLOW="", DIM=""),
      color=lambda s, *a: s)
_stub("hermes_cli.toolset_scope", _TOOLSET_PLATFORM_RESTRICTIONS={},
      toolset_allowed_for_platform=lambda *a, **k: True)


def _matches_name_filter(tool_name, patterns):
    if not patterns:
        return False
    if tool_name in patterns:
        return True
    return any(fnmatch.fnmatchcase(tool_name, p)
               for p in patterns if ("*" in p or "?" in p or "[" in p))


_stub("tools")
_stub("tools.mcp_tool_schema", matches_name_filter=_matches_name_filter)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w15_toolsconfi_129c58_live", QUARRY + "/hermes_cli/tools_config_mcp.py")


def compute(r):
    f = r.get("func")
    if f == "preselected":
        inc = r.get("include_set")
        exc = r.get("exclude_set")
        inc_set = set(str(p) for p in inc) if isinstance(inc, list) else None
        exc_set = set(str(p) for p in exc) if exc else None
        return sorted(live._mcp_preselected(r.get("tool_names", []), inc_set, exc_set,
                                           live._mcp_match_filter()))
    if f == "apply_mcp_change":
        cfg = copy.deepcopy(r.get("config") or {})
        failed = live._apply_mcp_change(cfg, r.get("targets", []), r.get("action", "enable"))
        return {"failed": sorted(failed), "config": cfg}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w15_toolsconfi_129c58: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w15_toolsconfi_129c58 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w15_toolsconfi_129c58: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
