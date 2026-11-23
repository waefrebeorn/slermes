#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_server_run.py (gate w12_mcptoolse_bdf454)."""
import asyncio
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
G = "w12_mcptoolse_bdf454"
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
_seed("tools.mcp_tool_common", _core=_Stub("core"),
      _get_lifecycle_seconds=lambda *a, **k: 0.0,
      _jittered=lambda x: x, _resolve_tool_timeout=lambda c: 30.0)
_seed("tools.mcp_tool_errors")
_seed("tools.mcp_tool_registration")
_seed("tools.mcp_tool_sampling")

SRC = QUARRY + "/tools/mcp_tool_server_run.py"
_spec = importlib.util.spec_from_file_location("mcp_tool_server_run_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["mcp_tool_server_run_live"] = live
_spec.loader.exec_module(live)


class _ReconnectFake(live.MCPServerRunMixin):
    def __init__(self, sd, rc):
        self._shutdown_event = asyncio.Event()
        self._reconnect_event = asyncio.Event()
        if sd:
            self._shutdown_event.set()
        if rc:
            self._reconnect_event.set()


class _RecycleFake(live.MCPServerRunMixin):
    def __init__(self, reason):
        self._reason = reason
        self.marked = None

    def _stdio_recycle_reason(self):
        return self._reason

    def _mark_stdio_recycled(self, reason):
        self.marked = reason


_FUNCS = ("reconnect_outcome", "recycle_if_due")


def live_out(rec):
    func = rec.get("func")
    if func == "reconnect_outcome":
        sd = bool(rec.get("shutdown"))
        rc = bool(rec.get("reconnect"))
        return asyncio.run(_ReconnectFake(sd, rc)._wait_for_reconnect_or_shutdown(timeout=0))
    if func == "recycle_if_due":
        reason = rec.get("reason")
        fake = _RecycleFake(reason)
        result = live.MCPServerRunMixin._recycle_if_due(fake)
        return [result, fake.marked]
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
        if rec.get("func") not in _FUNCS:
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
