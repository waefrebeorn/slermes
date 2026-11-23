#!/usr/bin/env python3
"""Differential oracle for w13_ownerforwa_a9e844 (tui_gateway/owner_forward.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tui_gateway/owner_forward.py"
sys.path.insert(0, QUARRY)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [QUARRY + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg

spec = importlib.util.spec_from_file_location("tui_gateway.owner_forward", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.owner_forward"] = live
spec.loader.exec_module(live)


class VE(Exception):
    pass


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def compute(r):
    f = r.get("func")
    if f == "forwardable":
        ref = types.SimpleNamespace(**(r.get("refusal") or {}))
        return live.forwardable(ref, dict(r.get("session") or {}),
                                r.get("text"), dict(r.get("params") or {}))
    if f == "discover_owner":
        def disc(home, timeout=None):
            return types.SimpleNamespace(state=r.get("state"), endpoint=r.get("endpoint"))
        fake_cli = types.ModuleType("hermes_cli")
        fake_cli.__path__ = []
        fake_rt = types.ModuleType("hermes_cli.gateway_runtime")
        fake_rt.discover_gateway_endpoint = disc
        old = {k: sys.modules.get(k) for k in ("hermes_cli", "hermes_cli.gateway_runtime")}
        sys.modules["hermes_cli"] = fake_cli
        sys.modules["hermes_cli.gateway_runtime"] = fake_rt
        try:
            ep = live.discover_owner(Path("."))
            return {"endpoint": canon(ep)}
        except live.OwnerUnavailable as exc:
            return {"error": str(exc)}
        finally:
            for k, v in old.items():
                if v is None:
                    sys.modules.pop(k, None)
                else:
                    sys.modules[k] = v
    if f == "payload_for":
        fields = r.get("contract_fields")
        valid = bool(r.get("valid"))
        events = {}
        if isinstance(fields, dict):
            for et, fl in fields.items():
                if fl is None:
                    payload_model = None
                else:
                    def mk_validate(ok=valid):
                        def _v(shaped):
                            if not ok: raise VE("invalid")
                        return _v
                    payload_model = types.SimpleNamespace(
                        model_fields=set(fl or []), model_validate=mk_validate())
                events[et] = types.SimpleNamespace(payload=payload_model)
        reg = types.ModuleType("tui_gateway.contracts.registry")
        reg.EVENTS = events
        contracts = types.ModuleType("tui_gateway.contracts")
        contracts.__path__ = []
        pd = types.ModuleType("pydantic")
        pd.ValidationError = VE
        old = {k: sys.modules.get(k) for k in
               ("tui_gateway.contracts", "tui_gateway.contracts.registry", "pydantic")}
        sys.modules["tui_gateway.contracts"] = contracts
        sys.modules["tui_gateway.contracts.registry"] = reg
        sys.modules["pydantic"] = pd
        try:
            return live._payload_for(r.get("event_type", ""), r.get("payload"))
        finally:
            for k, v in old.items():
                if v is None:
                    sys.modules.pop(k, None)
                else:
                    sys.modules[k] = v
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"ORACLE w13_ownerforwa_a9e844: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH w13_ownerforwa_a9e844 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_ownerforwa_a9e844: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
