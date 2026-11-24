#!/usr/bin/env python3
"""Differential oracle for gate w16_terminalto_f0d0d6 (tools/terminal_tool_backends.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


class _Dummy:
    pass


agent = _pkg("agent")
secret_mod = _mod("agent.secret_scope", get_secret=lambda k: None)
tools = _pkg("tools")
envs = _pkg("tools.environments")
for _m in ("docker", "local", "managed_modal", "modal", "ssh"):
    _mod("tools.environments." + _m, DockerEnvironment=_Dummy, LocalEnvironment=_Dummy,
         ManagedModalEnvironment=_Dummy, ModalEnvironment=_Dummy, SSHEnvironment=_Dummy)
_mod("tools.managed_tool_gateway", is_managed_tool_gateway_ready=lambda n: False)
_mod("tools.terminal_tool_config", _get_plugin_env_provider=lambda t: None)
_mod("tools.tool_backend_helpers", has_direct_modal_credentials=lambda: False,
     managed_nous_tools_enabled=lambda: False,
     nous_tool_gateway_unavailable_message=lambda x: "",
     resolve_modal_backend_state=lambda m, **kw: {})

NAME = "w16ttb_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/terminal_tool_backends.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w16_terminalto_f0d0d6"
DATA = {"secrets": {}, "vercel_installed": False}


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    cfg = inp.get("config") or {}
    DATA["secrets"] = inp.get("secrets") or {}
    DATA["vercel_installed"] = bool(inp.get("vercel_installed"))
    secret_mod.get_secret = lambda k: DATA["secrets"].get(k)
    live.importlib = types.SimpleNamespace(util=types.SimpleNamespace(
        find_spec=lambda n: (object() if (n == "vercel" and DATA["vercel_installed"]) else None)))
    live._last_unavailable_reason = None
    if func == "ssh_config":
        return live._ssh_config_from_config(cfg)
    if func == "local_config":
        return live._local_config_from_config(cfg)
    if func == "container_config":
        return live._container_config_from_config(cfg)
    if func == "resources":
        return live._resources(cfg)
    if func == "is_supported":
        return live._is_supported_vercel_runtime(inp.get("runtime", ""))
    if func == "check_vercel":
        ok = live._check_vercel(cfg)
        return {"ok": ok, "reason": live.terminal_backend_unavailable_reason()}
    if func == "ssh_pre":
        ok = live._ssh_pre(cfg)
        return {"ok": ok, "reason": live.terminal_backend_unavailable_reason()}
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
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
