#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_host.py (gate w8_pluginhost_b8d10b).

Logic under test: _import_ref(ref) = getattr(import_module(module), attr) after
`module, attr = ref.split(":")`.  import_module is stubbed to return a module
whose attributes resolve to their own names, so the observed value is `attr`;
a malformed ref (not exactly one colon) makes the unpack raise ValueError,
reported as "__VALUE_ERROR__".
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


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.plugin_host_wire", Channel=object, Opaque=object,
      PluginHostUnavailable=Exception, PluginHostUnsupported=Exception,
      declared_return=lambda *a: None, decode=lambda *a: None,
      encode=lambda *a: None, restore=lambda *a: None, signature_from=lambda *a: None)
_stub("hermes_cli._startup_fast", project_root_str=lambda: "")

LIVE_NAME = "w8_pluginhost_b8d10b_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/plugin_host.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


class AnyMod(types.ModuleType):
    def __getattr__(self, name):
        return name


class FakeImport:
    @staticmethod
    def import_module(name):
        return AnyMod(name)


# Only the live module's global `importlib` is replaced.
live.importlib = FakeImport()

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "import_ref_ex":
        continue

    try:
        py = live._import_ref(r.get("ref", ""))
    except ValueError:
        py = "__VALUE_ERROR__"

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH import_ref_ex %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_pluginhost_b8d10b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
