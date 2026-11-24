#!/usr/bin/env python3
"""Differential oracle for hermes_cli/browser_credential.py (gate w17_browsercre_9241f4).

The web_server._SESSION_TOKEN dependency is stubbed and set per case from the
fixture's `credential` key, mirroring the injected seam in the C port.
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


hcli = _mod("hermes_cli")
hcli.__path__ = []
ws = _mod("hermes_cli.web_server", _SESSION_TOKEN="***")
hcli.web_server = ws

SRC = Q + "/hermes_cli/browser_credential.py"
spec = importlib.util.spec_from_file_location("w17_browsercre_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_browsercre_live"] = live
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
    ws._SESSION_TOKEN = r.get("credential", "")
    try:
        if func == "generation_of":
            py = live.generation_of(r.get("credential", ""))
        elif func == "current_if":
            py = live.current_if(r.get("recorded"))
        elif func == "is_current":
            py = live.is_current(r.get("recorded"))
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

print(f"ORACLE w17_browsercre_9241f4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
