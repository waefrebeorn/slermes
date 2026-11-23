#!/usr/bin/env python3
"""Oracle for agent/auxiliary_local_runtime.py (gate auxlocal).

Live server discovery is replaced by scripted seams seeded from the fixture:
LLAMACPP_ALIASES comes from a stub endpoint module, resolve_llamacpp_endpoint
and _get_named_custom_provider return the fixture facts. The C side receives
the same resolved facts; the alias guard and the returned tuple are compared.
"""
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


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m
    return m


_pkg("agent", QUARRY + "/agent")
_pkg("hermes_cli", QUARRY + "/hermes_cli")
_pkg("hermes_cli.local_runtime", QUARRY + "/hermes_cli/local_runtime")

ep_stub = types.ModuleType("hermes_cli.local_runtime.endpoint")
ep_stub.LLAMACPP_ALIASES = frozenset({"llamacpp", "llama.cpp", "llama-cpp"})
_ep_holder = {"v": None}


def _resolve(config=None):
    return _ep_holder["v"]


ep_stub.resolve_llamacpp_endpoint = _resolve
sys.modules["hermes_cli.local_runtime.endpoint"] = ep_stub

cfg_stub = types.ModuleType("hermes_cli.config")
cfg_stub.load_config = lambda: {}
sys.modules["hermes_cli.config"] = cfg_stub

rp_stub = types.ModuleType("hermes_cli.runtime_provider")
_named = {"v": False}
rp_stub._get_named_custom_provider = lambda alias: _named["v"]
sys.modules["hermes_cli.runtime_provider"] = rp_stub

spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_local_runtime",
    QUARRY + "/agent/auxiliary_local_runtime.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.auxiliary_local_runtime"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "bare":
        continue

    ep_present = bool(r.get("ep_present", False))
    _ep_holder["v"] = ({"base_url": r.get("ep_base_url"),
                        "api_key": r.get("ep_api_key")} if ep_present else None)
    _named["v"] = bool(r.get("named", False))

    ret = live.bare_llamacpp_endpoint(r.get("provider"), r.get("base_url_in"),
                                      r.get("api_key"))
    py = None if ret is None else [str(ret[0]), str(ret[1])]
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE auxlocal: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
