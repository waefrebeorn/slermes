#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_rollback.py (gate w15_gatewayrol_519c95).

Stubs agent.i18n.t as a deterministic formatter and hermes_cli.gateway_client.
Live _diff's printed lines are captured one-per-print and compared to the C array.
"""
import builtins
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _t(key, **kw):
    if kw:
        return key + "|" + ",".join(f"{k}={v}" for k, v in sorted(kw.items()))
    return key


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parent, _, child = name.rpartition(".")
    if parent and parent in sys.modules:
        setattr(sys.modules[parent], child, m)
    return m


agent = _mod("agent")
agent.__path__ = [Q + "/agent"]
hcli = _mod("hermes_cli")
hcli.__path__ = [Q + "/hermes_cli"]
_mod("agent.i18n", t=_t)
_mod("hermes_cli.gateway_client", GatewayClientError=Exception)

SRC = Q + "/hermes_cli/gateway_rollback.py"
spec = importlib.util.spec_from_file_location("hermes_cli.gateway_rollback", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.gateway_rollback"] = live
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
        if func == "checkpoint_rows":
            py = live._checkpoint_rows(r.get("result"))
        elif func == "diff":
            captured = []
            original = builtins.print

            def _capture(*args, **kwargs):
                captured.append(" ".join(str(a) for a in args))

            builtins.print = _capture
            try:
                live._diff(r.get("result"))
            finally:
                builtins.print = original
            py = captured
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

print(f"ORACLE w15_gatewayrol_519c95: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
