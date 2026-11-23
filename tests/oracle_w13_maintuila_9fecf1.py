#!/usr/bin/env python3
"""Differential oracle for gate w13_maintuila_9fecf1 (hermes_cli/main_tui_launch.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_maintuila_9fecf1"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


_seed("hermes_cli")
# Empty module => `from hermes_cli.oneshot import _normalize_toolsets` raises ImportError,
# exercising the documented fallback branch of _normalize_tui_toolsets.
_seed("hermes_cli.oneshot")

SRC = QUARRY + "/hermes_cli/main_tui_launch.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    args = types.SimpleNamespace(**(r.get("args") or {}))
    if func == "bypass":
        return live._bypass_chat_launch(args)
    if func == "split_comma":
        return live._split_comma_items(r.get("items"), split_non_str=bool(r.get("split_non_str", True)))
    if func == "unforwarded_flags":
        return [flag for name, flag in live._TUI_UNFORWARDED.items()
                if getattr(args, name, None)]
    if func == "heap_mb":
        limit = r.get("limit") or 0
        live._read_cgroup_memory_limit = lambda: limit
        default = r.get("default_mb")
        return live._resolve_tui_heap_mb(default if default is not None else 8192)
    if func == "normalize_toolsets":
        return live._normalize_tui_toolsets(r.get("toolsets"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
