#!/usr/bin/env python3
"""Differential oracle for w8_blueprintpl_9d3e14 -> cron/blueprint_plugins.py.

The pure validators are exercised directly.  AutomationBlueprint/BlueprintSlot
and fill_blueprint are imported at module load; scripted stand-ins with the
same field names are registered so spec_from_file_location can execute the
live source.  ValueError outcomes are wrapped as {"ok":false,...}.
"""
import importlib.util
import json
import sys
import types
from dataclasses import dataclass, field
from typing import Any

G = "w8_blueprintpl_9d3e14"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("cron")


@dataclass
class BlueprintSlot:
    name: str
    type: str
    label: str
    options: tuple = ()
    default: Any = None
    optional: bool = False


@dataclass
class AutomationBlueprint:
    key: str
    title: str
    description: str
    category: str
    schedule_template: str
    prompt_template: str
    slots: list = field(default_factory=list)
    deliver_default: str = "origin"
    skills: tuple = ()
    tags: tuple = ()
    plugin: str = ""


_mod("cron.blueprint_catalog", AutomationBlueprint=AutomationBlueprint,
     BlueprintSlot=BlueprintSlot, fill_blueprint=lambda b, v: {"schedule": "* * * * *"})

SRC = Q + "/cron/blueprint_plugins.py"
spec = importlib.util.spec_from_file_location("w8_blueprintpl_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_blueprintpl_live"] = live
spec.loader.exec_module(live)


def outcome(fn):
    try:
        return {"ok": True, "value": fn()}
    except ValueError as exc:
        return {"ok": False, "error": "ValueError", "message": str(exc)}


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


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
        if func == "text":
            py = outcome(lambda: live._text(r.get("name", "name"), r.get("value")))
        elif func == "strings":
            py = outcome(lambda: live._strings(r.get("name", "name"), r.get("value")))
        elif func == "placeholders":
            py = outcome(lambda: live._placeholders(r.get("template", ""),
                                                    r.get("what", "prompt_template")))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = canonical(r.get("out"))
    cases += 1
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={canonical(py)!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or not cases else 0)
