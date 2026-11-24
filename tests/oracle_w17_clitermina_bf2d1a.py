#!/usr/bin/env python3
"""Differential oracle for gate w17_clitermina_bf2d1a (hermes_cli/cli_terminal_mixin.py)."""
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


# Parent-package bootstrap for the lazily-imported `cli` module.
agent = _pkg("agent")
_mod("agent.i18n", t=lambda key, **kw: key)
hcli = _pkg("hermes_cli")
_mod("hermes_cli.cli_render", _chrome_floor=lambda: 0, _hold_paints=lambda: None,
     _output_history_rows=lambda *a, **k: [], _release_paints=lambda: None,
     _set_chrome_floor=lambda v: None, _set_paint_gate=lambda a, f: None,
     _take_suspect_rows=lambda: 0)
_mod("hermes_constants", get_hermes_home=lambda: "/none")
cli_mod = _mod("cli", CLI_CONFIG={})

NAME = "w17ctm_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/hermes_cli/cli_terminal_mixin.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w17_clitermina_bf2d1a"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    if func == "is_eio":
        exc = types.SimpleNamespace(errno=inp.get("errno", 0))
        return bool(live._is_eio(exc))
    if func == "tmux_passthrough":
        return live._tmux_passthrough(inp.get("seq", ""))
    if func == "rebuilds_scrollback":
        cli_mod.CLI_CONFIG = inp.get("cli_config") or {}
        return bool(live.CLITerminalMixin._redraw_rebuilds_scrollback())
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
