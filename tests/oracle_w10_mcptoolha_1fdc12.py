#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_handlers.py (gate w10_mcptoolha_1fdc12)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_mcptoolha_1fdc12"
sys.path.insert(0, QUARRY)


class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        def _noop(*a, **k):
            return None
        return _noop


def _seed(name, **attrs):
    mod = _Stub(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


# Stub only the heavy top-level dependency chain; the three ported helpers are
# pure (json / f-string) and use none of these collaborators.
_seed("tools")
_seed("hermes_platform")
_seed("hermes_platform.declaration", on_change=None)
_seed("tools.registry")
_seed("tools.ansi_strip", strip_unicode_tags=lambda t: t, strip_unicode_tags_deep=lambda t: t)
_seed("tools.thread_context")
_seed("tools.mcp_tool_common", _core=_Stub("tools.mcp_tool_common._core"))
_seed("tools.mcp_tool_loop")
_seed("tools.mcp_tool_content", _MCP_HARD_RESULT_CAP_CHARS=20000)
_seed("tools.mcp_tool_errors")

SRC = QUARRY + "/tools/mcp_tool_handlers.py"
_spec = importlib.util.spec_from_file_location("mcp_tool_handlers_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_tool_handlers_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "trust_approval_key":
        return live._trust_approval_key(rec.get("server"), rec.get("tool"))
    if func == "result_is_error":
        return live._result_is_error(rec.get("result"))
    if func == "json_type_stable_equal":
        return live._json_type_stable_equal(rec.get("left"), rec.get("right"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("trust_approval_key", "result_is_error", "json_type_stable_equal"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
