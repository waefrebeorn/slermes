#!/usr/bin/env python3
"""Differential oracle for gate w18_gatewayhos_f57858 (hermes_cli/gateway_host_record.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "gateway_host_record.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)

_SPEC = importlib.util.spec_from_file_location(
    "w18_gatewayhos_live", str(Path(QUARRY) / "hermes_cli" / "gateway_host_record.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _install(kind, sim):
    """Install a fake `gateway` package so the live late-import is deterministic."""
    for k in ("gateway", "gateway.host_rendezvous", "gateway.host_attach"):
        sys.modules.pop(k, None)
    gw = types.ModuleType("gateway")
    gw.__path__ = []  # submodule import fails unless explicitly registered below
    sys.modules["gateway"] = gw
    if sim == "import_error":
        return None
    if kind == "discard":
        hr = types.ModuleType("gateway.host_rendezvous")
        hr.ROLE_GATEWAY = "gateway"

        def discard_dead_record(role):
            if sim == "call_error":
                raise RuntimeError("simulated call failure")
            return True if sim == "ok_true" else False

        hr.discard_dead_record = discard_dead_record
        sys.modules["gateway.host_rendezvous"] = hr
        return None
    state = {"called": False}
    ha = types.ModuleType("gateway.host_attach")

    def invalidate_host_gateway_cache():
        state["called"] = True
        if sim == "call_error":
            raise RuntimeError("simulated call failure")

    ha.invalidate_host_gateway_cache = invalidate_host_gateway_cache
    sys.modules["gateway.host_attach"] = ha
    return state


def expected(rec):
    f = rec.get("func")
    sim = rec.get("sim")
    if f == "discard_dead_host_record":
        _install("discard", sim)
        return bool(mod._discard_dead_host_record())
    if f == "forget_host_gateway_probe":
        state = _install("forget", sim)
        mod._forget_host_gateway_probe()
        return bool(state["called"]) if state is not None else False
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if got != want:
            mism += 1
            print("MISMATCH %s sim=%s got=%r want=%r" %
                  (rec.get("func"), rec.get("sim"), got, want), file=sys.stderr)
    print("ORACLE w18_gatewayhos_f57858: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
