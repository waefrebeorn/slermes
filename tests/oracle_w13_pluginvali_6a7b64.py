#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_validate.py (gate w13_pluginvali_6a7b64)."""
import importlib.util
import json
import re
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

# Parent-package bootstrap + stub the heavy sibling imports.
cli = types.ModuleType("hermes_cli")
cli.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", cli)


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.plugin_validate_core_override", check_core_override=lambda *a, **k: None)
_stub("hermes_cli.plugin_validate_desktop", check_desktop_surface=lambda *a, **k: None)
_stub("hermes_cli.plugin_validate_private_imports", check_private_imports=lambda *a, **k: None)
_stub("hermes_cli.plugin_validate_locales", check_language_packs=lambda *a, **k: None)
_stub("hermes_cli._startup_fast", project_root_str=lambda: QUARRY)
_CFG = ("str string int integer float number bool boolean list array dict object secret").split()
_VRE = re.compile(r"^\s*(>=|<=|==|!=|>|<)\s*(.+?)\s*$")
_SEG = re.compile(r"^\d+")


def _vt(v):
    parts = re.split(r"[-+]", str(v).strip().lstrip("v"), maxsplit=1)[0].split(".")
    parts += ["0"] * (3 - len(parts))
    out = []
    for x in parts[:3]:
        m = _SEG.match(x.strip())
        if m is None:
            return None
        out.append(int(m.group(0)))
    return tuple(out)


_stub("hermes_cli.plugins_manifest",
      _CONFIG_SCHEMA_TYPES={k: () for k in _CFG},
      _VERSION_COMPARATOR_RE=_VRE, _version_tuple=_vt)

SRC = QUARRY + "/hermes_cli/plugin_validate.py"
NAME = "plugin_validate_live_w13_pluginvali"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "declared_list":
        return live._declared_list(r.get("manifest", {}), r.get("key", ""))
    if func == "requires_hermes_spec_valid":
        return live._requires_hermes_spec_valid(r.get("spec", ""))
    if func == "check_requires_env":
        rep = live.ValidationReport()
        live._check_requires_env(rep, {"requires_env": r.get("entries", [])})
        name, ok, detail = rep.checks[-1]
        return {"name": name, "ok": ok, "detail": detail}
    if func == "validation_report":
        rep = live.ValidationReport(checks=[tuple(c) for c in r.get("checks", [])])
        return {"ok": rep.ok, "failures": rep.failures, "exit_code": rep.exit_code}
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w13_pluginvali_6a7b64: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
