#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_render.py (w8_clirender_3bf599 gate).

Heavy display dependencies are stubbed; environment/TTY-dependent seams are
injected from each fixture line so the live function stays deterministic.
"""
import importlib.util
import json
import os
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/cli_render.py"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = [QUARRY + "/agent"]
_mod("agent.i18n", t=lambda key, **kw: key)
_mod("agent.think_scrubber", THINK_TAG_NAMES=("think", "analysis"))
hcpkg = _mod("hermes_cli")
hcpkg.__path__ = [QUARRY + "/hermes_cli"]
_mod("hermes_cli.banner", format_banner_version_label=lambda: "v")
rich = _mod("rich")
rich.__path__ = []
_mod("rich.console", Console=object)
_mod("rich.text", Text=object)
# Late-bound `from cli import ...` seams used by the two light-mode helpers.
cli = _mod("cli")

spec = importlib.util.spec_from_file_location("clirender_w8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["clirender_w8_live"] = live
spec.loader.exec_module(live)

cli._TRUE_RE = live._TRUE_RE
cli._FALSE_RE = live._FALSE_RE
cli._LIGHT_DEFAULT_TERM_PROGRAMS = live._LIGHT_DEFAULT_TERM_PROGRAMS
cli._LIGHT_MODE_REMAP_UPPER = live._LIGHT_MODE_REMAP_UPPER
cli._luminance_from_hex = live._luminance_from_hex
CUR = {}
cli._detect_light_mode = lambda: bool(CUR.get("light", False))
cli._query_osc11_background = lambda: CUR.get("osc_bg")


def compute(r):
    f = r["func"]
    if f == "luminance":
        return live._luminance_from_hex(r.get("value", ""))
    if f == "limit":
        return live._coerce_output_history_limit(r.get("value"))
    if f == "prepend":
        return live._prepend_note_to_message(r.get("message"), r.get("note"))
    if f == "content":
        return live._assistant_content_as_text(r.get("content"))
    if f == "remap":
        CUR["light"] = bool(r.get("light", False))
        return live._maybe_remap_for_light_mode(r.get("hex", ""))
    if f == "detect":
        CUR["env"] = r.get("env") or {}
        CUR["osc_bg"] = r.get("osc_bg")
        old = os.environ
        os.environ = CUR["env"]
        try:
            return live._detect_light_mode_uncached()
        finally:
            os.environ = old
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
        print(f"ORACLE clirender: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH clirender {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE clirender: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
