#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_chat_listing.py
(gate w21_gatewaycha_3d5133).

The live module is imported as-is, so `_selection` and `print_tool_listing` run
their real bodies: the explicit-toolsets split/dedupe/sort, the profile
fallback, the registry sort and grouping, the first-sentence description cut,
the column formats and the totals wording. The registry (`model_tools`), the
toolset table (`toolsets`), this profile's config and the i18n catalog are
config/registry seams and are scripted from the fixture.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_chat_listing.py"
GATE = "w21_gatewaycha_3d5133"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}


def stub(name, **attrs):
    mod = types.ModuleType(name)
    mod.__dict__.update(attrs)
    sys.modules[name] = mod
    return mod


def load_config():
    return {"agent": {"disabled_toolsets": (CASE.get("config") or {}).get("disabled_toolsets") or []}}


def _get_platform_tools(config, platform):
    return (CASE.get("config") or {}).get("platform_tools") or []


def t(key, **kwargs):
    text = (CASE.get("i18n") or {}).get(key, key)
    return text.format(**kwargs) if kwargs else text


def get_tool_definitions(*, enabled_toolsets=None, disabled_toolsets=None,
                         quiet_mode=False, skip_tool_search_assembly=False):
    return [{"function": {"name": tool.get("name"),
                          "description": tool.get("description", "")}}
            for tool in CASE.get("tools") or []]


def get_toolset_for_tool(name):
    for tool in CASE.get("tools") or []:
        if tool.get("name") == name:
            return tool.get("toolset")
    return None


def get_all_toolsets():
    return list(CASE.get("all_toolsets") or [])


def get_toolset_info(name):
    return (CASE.get("toolsets_info") or {}).get(name)


for _pkg in ("hermes_cli", "agent"):
    if _pkg not in sys.modules:
        stub(_pkg)
stub("hermes_cli.config", load_config=load_config)
stub("hermes_cli.tools_config", _get_platform_tools=_get_platform_tools)
stub("agent.i18n", t=t)
stub("model_tools", get_tool_definitions=get_tool_definitions,
     get_toolset_for_tool=get_toolset_for_tool)
stub("toolsets", get_all_toolsets=get_all_toolsets, get_toolset_info=get_toolset_info)
if "hermes_cli.gateway_chat_listing" not in sys.modules:
    pass

_spec = importlib.util.spec_from_file_location("gateway_chat_listing_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_chat_listing_live"] = live
_spec.loader.exec_module(live)


def expected(r):
    global CASE
    CASE = r
    args = types.SimpleNamespace(toolsets=r.get("toolsets"),
                                 list_tools=bool(r.get("list_tools")))
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = live.print_tool_listing(args)
    return {"lines": buf.getvalue().splitlines(), "rc": rc}


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
        if not isinstance(r, dict) or r.get("func") != "render":
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live raise is a harness bug
            mism += 1
            print("MISMATCH render case=%r live-error=%r" % (r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH render case=%r\n  py=%r\n   c=%r"
                  % (r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
