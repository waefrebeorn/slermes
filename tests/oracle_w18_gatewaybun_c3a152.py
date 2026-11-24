#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_bundled_env.py (gate w18_gatewaybun_c3a152)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], m)
    return m


_seed("hermes_cli", Q + "/hermes_cli")
gw = _seed("hermes_cli.gateway", _unit_environment_value=lambda p, n: "")

spec = importlib.util.spec_from_file_location(
    "w18_gatewaybun_live", Q + "/hermes_cli/gateway_bundled_env.py")
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {str(k): _canon(v) for k, v in x.items()}
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
    prior = r.get("prior") or {}
    env = r.get("env") or {}
    try:
        live.os = types.SimpleNamespace(environ=env)
        if func == "env_pairs":
            py = [list(p) for p in live.bundled_resource_env_pairs(prior or None)]
        elif func == "systemd_env_line":
            py = live.systemd_env_line(r.get("name", ""), r.get("value", ""))
        elif func == "systemd_bundled_env_block":
            gw._unit_environment_value = lambda p, n: prior.get(n, "")
            py = live.systemd_bundled_env_block(None)
        elif func == "launchd_bundled_env_xml":
            live._prior_launchd_env = lambda p: prior
            py = live.launchd_bundled_env_xml(None)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_gatewaybun_c3a152: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
