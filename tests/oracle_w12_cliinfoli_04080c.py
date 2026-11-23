#!/usr/bin/env python3
"""Differential oracle for w12_cliinfoli_04080c (hermes_cli/cli_info_lines.py).

_t is stubbed to the identity key; toolset catalog and gateway config are
injected from the fixture so the live line builders run unmodified.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap before executing the live module by file path.
agent = _mod("agent")
agent.__path__ = []
_mod("agent.i18n", t=lambda key, **kw: key)
hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []

ALL_NAMES = []
INFOS = {}
toolsets = _mod("toolsets",
                get_all_toolsets=lambda: list(ALL_NAMES),
                get_toolset_info=lambda name: INFOS.get(name))

HOME = ""
_mod("hermes_constants", display_hermes_home=lambda: HOME)
gateway = _mod("gateway")
gateway.__path__ = []
PLATFORM = types.SimpleNamespace(TELEGRAM="telegram", DISCORD="discord",
                                 SLACK="slack", WHATSAPP="whatsapp")
LOAD_ERROR = ""
CONFIG = types.SimpleNamespace(platforms={}, get_home_channel=lambda p: None)


def _load_config():
    if LOAD_ERROR:
        raise RuntimeError(LOAD_ERROR)
    return CONFIG


_mod("gateway.config", Platform=PLATFORM, load_gateway_config=_load_config)

SRC = Q + "/hermes_cli/cli_info_lines.py"
spec = importlib.util.spec_from_file_location("w12_cliinfoli_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w12_cliinfoli_live"] = live
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
    func = r.get("func")
    try:
        if func == "box":
            py = live._box(r.get("title", ""), int(r.get("width", 0)))
        elif func == "toolsets_lines":
            sp = r.get("spec", {})
            ALL_NAMES = list(sp.get("all_names", []))
            INFOS = dict(sp.get("toolsets", {}))
            py = live.toolsets_lines(sp.get("enabled"))
        elif func == "platforms_lines":
            sp = r.get("spec", {})
            HOME = sp.get("home", "")
            LOAD_ERROR = sp.get("load_error", "")
            plats = {}
            homes = {}
            for key, val in (sp.get("config", {}).get("platforms", {})).items():
                plats[key] = types.SimpleNamespace(enabled=bool(val.get("enabled", False)))
                if "home" in val:
                    homes[key] = types.SimpleNamespace(name=val.get("home", ""))
            CONFIG = types.SimpleNamespace(
                platforms=plats,
                get_home_channel=lambda p: homes.get(p))
            py = live.platforms_lines()
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_cliinfoli_04080c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
