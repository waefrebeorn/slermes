#!/usr/bin/env python3
"""Differential oracle for gate w12_sessiondis_3b3fbe (gateway/session_discovery.py)."""
import asyncio
import contextlib
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_sessiondis_3b3fbe"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


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


PROFILE_OK = [True]


class Refusal(RuntimeError):
    pass


for _pkg in ("persistence", "persistence.core", "gateway", "hermes_cli", "tui_gateway"):
    if _pkg not in sys.modules:
        _seed(_pkg)
_seed("persistence.core.errors", AuthorityRefusal=Refusal)


@contextlib.contextmanager
def _scope(home):
    yield


_seed("gateway.run", _profile_runtime_scope=_scope)
_seed("gateway.session_commands", _session_workspace=lambda runner, source: "SCOPE")
_seed("gateway.session_contract", SessionRef=lambda profile, sid: NS(profile_id=profile, session_id=sid))


def _matches(profile, home):
    return PROFILE_OK[0]


_seed("hermes_cli.profiles", profile_matches_home=_matches)


class _Mgr:
    def list_plugins(self):
        return []

    def get_portable_mcp_servers(self):
        return []


_seed("hermes_cli.plugins",
      discover_plugins=lambda: None,
      get_plugin_manager=lambda: _Mgr(),
      get_plugin_toolsets=lambda: [])
_seed("tui_gateway.command_discovery",
      command_catalog=lambda scope=None: ["catalog"],
      slash_completions=lambda text="", scope=None: ["completion:" + text])

SRC = QUARRY + "/gateway/session_discovery.py"
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


def authority_of(r):
    sid = (r.get("params") or {}).get("session_id") if isinstance(r.get("params"), dict) else None
    return NS(
        profile_id=r.get("authority_profile"),
        authorize=lambda *a, **k: None,
        runner=NS(),
        sessions={sid: NS(source="src")} if sid else {},
    )


def actor_of(r):
    return NS(capabilities=r.get("capabilities") or [], profile_id=r.get("actor_profile"))


def compute(r):
    global PROFILE_OK
    PROFILE_OK[0] = bool(r.get("profile_ok", True))
    authority, actor = authority_of(r), actor_of(r)
    try:
        if r.get("func") == "discover":
            asyncio.run(live.discover_commands(authority, actor, r.get("params"),
                                               completion=bool(r.get("completion"))))
        elif r.get("func") == "list_check":
            asyncio.run(live.list_plugins(authority, actor, r.get("params")))
        else:
            return None
    except Refusal as exc:
        return exc.args[0] if exc.args else "refused"
    return "ok"


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
