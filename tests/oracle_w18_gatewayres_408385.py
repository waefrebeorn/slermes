#!/usr/bin/env python3
"""Differential oracle for gate w18_gatewayres_408385 (hermes_cli/gateway_restart_env.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "gateway_restart_env.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)

CTX = {}

# Parent-package bootstrap: all late imports resolve to our deterministic fakes.
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = []
sys.modules["hermes_cli"] = _pkg
for _parent in ("agent", "tools", "tools.environments", "gateway"):
    _m = types.ModuleType(_parent)
    _m.__path__ = []
    sys.modules[_parent] = _m
_profiles = types.ModuleType("hermes_cli.profiles")
_profiles.get_profile_dir = lambda name: CTX.get("resolved_home")
sys.modules["hermes_cli.profiles"] = _profiles

_spec = importlib.util.spec_from_file_location(
    "w18_gatewayres_live", str(Path(QUARRY) / "hermes_cli" / "gateway_restart_env.py"))
mod = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = mod
_spec.loader.exec_module(mod)


def _install(rec):
    envloader = types.ModuleType("hermes_cli.env_loader")
    envloader.launch_dotenv_keys = lambda: []
    envloader.source_supplied_names = lambda: []
    sys.modules["hermes_cli.env_loader"] = envloader

    ss = types.ModuleType("agent.secret_scope")
    ss.load_env_file = lambda p: {k: "1" for k in CTX.get("supplied", [])}
    sys.modules["agent.secret_scope"] = ss

    hc = types.ModuleType("hermes_constants")
    hc.get_routing_process_hermes_home = lambda: Path("/fake")
    sys.modules["hermes_constants"] = hc

    lep = types.ModuleType("tools.environments.local_env_policy")
    lep._ADAPTER_SECRET_ENV = set(CTX.get("adapter_secrets", []))
    lep._home_adapter_secret_env = lambda: set()
    sys.modules["tools.environments.local_env_policy"] = lep

    gr = types.ModuleType("gateway.restart")
    gr.is_supervised_gateway_launch = lambda: bool(CTX.get("supervised", False))
    sys.modules["gateway.restart"] = gr

    loc = types.ModuleType("tools.environments.local")
    loc.host_gateway_child_env = lambda: dict(CTX.get("env", {}))
    sys.modules["tools.environments.local"] = loc


def _canon(x):
    if isinstance(x, (set, frozenset)):
        return ("list", sorted((_canon(v) for v in x), key=repr))
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((str(k), _canon(v)) for k, v in x.items()))
    if isinstance(x, bool):
        return ("num", 1 if x else 0)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if x is None:
        return ("null",)
    return ("other", repr(x))


def expected(rec):
    f = rec.get("func")
    if f == "argv_profile_home":
        CTX["resolved_home"] = rec.get("resolved_home")
    elif f == "launch_bot_token_names":
        CTX["supplied"] = rec.get("supplied") or []
        CTX["adapter_secrets"] = rec.get("adapter_secrets") or []
        CTX["supervised"] = rec.get("supervised", False)
    elif f == "host_watcher_env":
        CTX["env"] = rec.get("env") or {}
    # _install snapshots some values into static module attributes, so it must
    # run after CTX has been populated for this case.
    _install(rec)
    if f == "argv_profile_home":
        return mod._argv_profile_home(rec.get("argv") or [])
    if f == "launch_bot_token_names":
        return mod._launch_bot_token_names()
    if f == "host_watcher_env":
        return mod._host_gateway_watcher_env()
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s case=%s got=%r want=%r" % (
                    rec.get("func"), rec.get("case"), got, want), file=sys.stderr)
    print("ORACLE w18_gatewayres_408385: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
