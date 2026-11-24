#!/usr/bin/env python3
"""Differential oracle for tools/bot_desktop/placement.py
(gate w24_placement_282518).

The LIVE ``resolve()`` is called, with only its four leaf seams replaced --
``hermes_cli.config.load_config_readonly``, ``tools.terminal_tool._get_env_config``,
``tools.terminal_tool_config._plugin_env_flag`` and
``hermes_cli.plugin_isolation.isolation_mode``.  Everything the gate ports
(config normalisation, the local shortcut, the stream-backend list, the
``is True`` test on the plugin flag, both refusal sentences) runs as upstream
wrote it, and no container, sandbox or environment object is created.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_placement_282518"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.config as hcfg  # noqa: E402
import tools.bot_desktop.placement as live  # noqa: E402

WANTED = {"resolve", "constants"}

# hermes_cli.plugin_isolation, tools.terminal_tool and tools.terminal_tool_config
# are import-time heavy (they resolve the Hermes home and load the environment
# registry).  They are THE seams -- the config/env/flag providers -- so they are
# stubbed as modules with exactly the names live code imports from them, and
# resolve() otherwise runs untouched.
def _stub_module(name, **attrs):
    mod = sys.modules.get(name)
    if mod is None or not getattr(mod, "_w24_stub", False):
        mod = types.ModuleType(name)
        mod._w24_stub = True
        sys.modules[name] = mod
    for key, value in attrs.items():
        setattr(mod, key, value)
    return mod


def resolve(req):
    backend = req.get("backend")
    setting = req.get("setting")
    streams = req.get("plugin_streams")
    host_isolated = req.get("host_isolated")

    saved = hcfg.load_config_readonly
    hcfg.load_config_readonly = lambda: {"bot_desktop": {"placement": setting}}
    _stub_module("tools.terminal_tool", _get_env_config=lambda: {"env_type": backend})
    _stub_module("tools.terminal_tool_config",
                 _plugin_env_flag=lambda name, flag: streams)
    _stub_module("hermes_cli.plugin_isolation", ISOLATION_HOST="host",   # upstream literal
                 isolation_mode=lambda: "host" if host_isolated else "in_process")
    try:
        p = live.resolve()
    finally:
        hcfg.load_config_readonly = saved
    return {"where": p.where, "backend": p.backend, "reason": p.reason}


def constants():
    return {
        "gateway": live.GATEWAY,
        "terminal": live.TERMINAL,
        "refused": live.REFUSED,
        "stream_backends": list(live._STREAM_BACKENDS),
    }


def expected(r):
    f = r.get("func")
    if f == "resolve":
        return resolve(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
