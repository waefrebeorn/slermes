#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_runtime.py (w13_gatewayrun_de650a)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg = _mod("gateway", Q + "/gateway")
_mod("gateway.drain_control", RESTART_WAIT_REASON="restart_wait")

SRC = Q + "/hermes_cli/gateway_runtime.py"
spec = importlib.util.spec_from_file_location("w13_gatewayrun_de650a_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w13_gatewayrun_de650a_live"] = live
spec.loader.exec_module(live)
live._canonical_home = lambda h: h


def compat(v):
    if isinstance(v, (set, frozenset)):
        return sorted((compat(x) for x in v), key=repr)
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


def ser_discovery(d):
    ep = d.endpoint
    return {
        "state": d.state, "reason_code": d.reason_code, "detail": d.detail,
        "endpoint": None if ep is None else {
            "profile_id": ep.profile_id, "instance_id": ep.instance_id,
            "authority_epoch": ep.authority_epoch, "runtime_protocol": ep.runtime_protocol,
            "api_origin": ep.api_origin, "supervisor": ep.supervisor,
            "capabilities": sorted(ep.capabilities), "control_home": ep.control_home,
            "code_sha": ep.code_sha,
        },
    }


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
        if func == "constants":
            py = {"DEFAULT_ENSURE_TIMEOUT": live.DEFAULT_ENSURE_TIMEOUT,
                  "RESTART_WAIT_REASON": sys.modules["gateway.drain_control"].RESTART_WAIT_REASON,
                  "REPLACE_FAILED_DETAIL": live._REPLACE_FAILED_DETAIL,
                  "RETIRE": sorted(live._RETIRING)}
        elif func == "attachable":
            d = types.SimpleNamespace(
                state=r.get("state", ""), reason_code=r.get("reason_code"),
                endpoint=object() if r.get("has_endpoint", False) else None)
            py = live.attachable(d)
        elif func == "control_home":
            ep = types.SimpleNamespace(control_home=r.get("control_home") or None)
            py = str(live.control_home_for(r.get("home", ""), ep))
        elif func == "endpoint":
            d = live._endpoint(r.get("payload") or {}, r.get("home", ""),
                               r.get("control_home") or None)
            py = compat(ser_discovery(d))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = compat(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_gatewayrun_de650a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
