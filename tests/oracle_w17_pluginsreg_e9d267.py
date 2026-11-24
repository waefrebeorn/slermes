#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_registrars.py (w17_pluginsreg_e9d267).

Imports the LIVE module with its one sibling import stubbed, then reads the
LIVE _NAME_NORMALIZERS and _ROWS_BY_METHOD tables to compute expectations.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

pkg = types.ModuleType("hermes_cli")
pkg.__path__ = [os.path.join(Q, "hermes_cli")]
sys.modules.setdefault("hermes_cli", pkg)
pl = types.ModuleType("hermes_cli.plugins_loader")
pl._plugin_home_scope = lambda home: (lambda: None)
pl._serialized_replacement = lambda fn: fn
sys.modules["hermes_cli.plugins_loader"] = pl

SRC = Q + "/hermes_cli/plugins_registrars.py"
spec = importlib.util.spec_from_file_location("hermes_cli.plugins_registrars", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.plugins_registrars"] = live
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
        if func == "normalize":
            name = r.get("name", "")
            mode = r.get("mode", "strip")
            key = None if mode == "none" else mode
            fn = live._NAME_NORMALIZERS.get(key)
            py = fn(name) if fn else name
        elif func == "row":
            row = live._ROWS_BY_METHOD.get(r.get("method", ""))
            if row is None:
                py = None
            else:
                o = row[6]
                py = {"kind": row[1], "registry_mod": row[2], "base_ref": row[3],
                      "label": row[4], "normalize": o.get("normalize", "strip"),
                      "article": o.get("article", "a")}
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

print(f"ORACLE w17_pluginsreg_e9d267: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
