#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/inline_picker.py (gate
w23_inlinepick_18ba58).

The command/skill registry is the only OS-adjacent seam in this module:

  * ``hermes_cli.commands_platforms`` is a stub package whose
    ``telegram_bot_commands()`` / ``_collect_gateway_skill_entries()`` answer
    the fixture's rows (and raise when the fixture says that source failed);
  * for the ``collect`` cases that pin "the registry import itself raised", the
    stub is removed from ``sys.modules`` and replaced with ``None``, which makes
    the live ``from ... import ...`` raise ``ImportError`` exactly like a
    missing registry would.

``filter_catalog`` / ``build_inline_results`` are then exercised live:
``build_inline_results`` calls ``collect_inline_catalog`` internally, so the
fixture's catalog is injected by patching that module global for the duration
of the call (the same list the C harness hands its own builder).
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "inline_picker.py"
GATE = "w23_inlinepick_18ba58"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("inline_picker_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["inline_picker_live"] = live
_spec.loader.exec_module(live)

WANTED = {"page_size", "cache_time", "collect", "filter", "build"}


def registry_module(world):
    """The commands registry the fixture describes."""

    def telegram_bot_commands():
        if world.get("bot_error"):
            raise RuntimeError("core command collection failed")
        return [tuple(row) for row in (world.get("bot") or [])]

    def _collect_gateway_skill_entries(platform=None, max_slots=None,
                                       reserved_names=None, desc_limit=None,
                                       sanitize_name=None):
        if world.get("skills_error"):
            raise RuntimeError("skill/plugin collection failed")
        return [tuple(row) for row in (world.get("skills") or [])], set()

    mod = types.ModuleType("hermes_cli.commands_platforms")
    mod.telegram_bot_commands = telegram_bot_commands
    mod._collect_gateway_skill_entries = _collect_gateway_skill_entries
    mod._sanitize_telegram_name = lambda name: name
    return mod


def collect(world):
    world = world if isinstance(world, dict) else {}
    import hermes_cli  # real package; only its attribute is swapped

    saved_attr = getattr(hermes_cli, "commands_platforms", None)
    saved_mod = sys.modules.get("hermes_cli.commands_platforms", "ABSENT")
    if world.get("registry", True):
        mod = registry_module(world)
        sys.modules["hermes_cli.commands_platforms"] = mod
        hermes_cli.commands_platforms = mod
    else:
        # None in sys.modules makes the import raise ImportError.
        sys.modules["hermes_cli.commands_platforms"] = None
        hermes_cli.commands_platforms = None
    try:
        return live.collect_inline_catalog()
    finally:
        if saved_mod == "ABSENT":
            sys.modules.pop("hermes_cli.commands_platforms", None)
        else:
            sys.modules["hermes_cli.commands_platforms"] = saved_mod
        if saved_attr is None:
            try:
                del hermes_cli.commands_platforms
            except AttributeError:
                pass
        else:
            hermes_cli.commands_platforms = saved_attr


def filt(world):
    world = world if isinstance(world, dict) else {}
    return live.filter_catalog(world.get("catalog") or [], world.get("term"))


def build(world):
    world = world if isinstance(world, dict) else {}
    catalog = world.get("catalog") or []
    saved = live.collect_inline_catalog
    live.collect_inline_catalog = lambda: catalog
    try:
        if "page_size" in world:
            results, next_offset = live.build_inline_results(
                world.get("query"), world.get("offset", ""),
                page_size=world.get("page_size"))
        else:
            results, next_offset = live.build_inline_results(
                world.get("query"), world.get("offset", ""))
    finally:
        live.collect_inline_catalog = saved
    return {"results": results, "next_offset": next_offset}


def expected(r):
    f = r.get("func")
    if f == "page_size":
        return live.PAGE_SIZE
    if f == "cache_time":
        return live.CACHE_TIME_SECONDS
    if f == "collect":
        return collect(r.get("world"))
    if f == "filter":
        return filt(r.get("world"))
    if f == "build":
        return build(r.get("world"))
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
