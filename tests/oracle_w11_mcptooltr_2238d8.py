#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_transport.py (gate w11_mcptooltr_2238d8)."""
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
G = "w11_mcptooltr_2238d8"
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


_seed("tools")
_seed("utils", normalize_proxy_url=lambda u: u)
_seed("agent")
_seed("hermes_cli")
_seed("agent.proxy_bypass", is_loopback_host=lambda h: False, should_bypass_proxy=lambda u: False)
_seed("tools.mcp_tool_errors")
_seed("tools.mcp_tool_lifecycle")
_seed("hermes_cli.process_identity")
_seed("tools.mcp_tool_common", _core=_Stub("tools.mcp_tool_common._core"))
_seed("tools.mcp_tool_node_abi")

SRC = QUARRY + "/tools/mcp_tool_transport.py"
_spec = importlib.util.spec_from_file_location("mcp_tool_transport_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_tool_transport_live"] = live
_spec.loader.exec_module(live)


class Resp:
    def __init__(self, ct="", status=200):
        self.headers = {"content-type": ct}
        self.status_code = status


def live_out(rec):
    func = rec.get("func")
    if func == "content_type_base":
        return live._content_type_base(Resp(rec.get("content_type") or ""))
    if func == "is_2xx":
        return live._is_2xx(Resp(status=int(rec.get("status", 0))))
    if func == "present":
        return live._present(**(rec.get("kwargs") or {}))
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
        if rec.get("func") not in ("content_type_base", "is_2xx", "present"):
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
