#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_client.py (gate w12_gatewaycli_0af67a)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_gatewaycli_0af67a"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


# gateway.session_contract is dependency-free; seed it explicitly so the
# live module's top-level late import never drags in the wider gateway tree.
_seed("gateway")
_seed("gateway.session_contract", CANONICAL_GATEWAY_PROTOCOL="hermes-gateway-v1")

SRC = QUARRY + "/hermes_cli/gateway_client.py"
_spec = importlib.util.spec_from_file_location("gateway_client_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_client_live"] = live
_spec.loader.exec_module(live)


class _Endpoint:
    pass


def live_out(rec):
    func = rec.get("func")
    if func == "ws":
        ep = _Endpoint()
        ep.api_origin = rec.get("origin", "")
        url, protos = live.gateway_ws_target(ep, rec.get("ticket", ""))
        return [url, protos]
    if func == "reason":
        return live._bounded_reason(rec.get("value"))
    if func == "edata":
        return live._bounded_error_data(rec.get("data"))
    if func == "rpcerr":
        exc = live.rpc_error(rec.get("error"))
        return {"reason": str(exc), "code": exc.code, "data": exc.data,
                "unknown_method": exc.unknown_method}
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("ws", "reason", "edata", "rpcerr"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
