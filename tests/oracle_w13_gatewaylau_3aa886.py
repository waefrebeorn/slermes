#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_launchd.py (w13_gatewaylau_3aa886)."""
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


_pkg = _mod("hermes_cli", Q + "/hermes_cli")
_mod("hermes_cli.gateway_service_owner")
_pkg2 = _mod("agent", Q + "/agent")
_mod("agent.deadline", poll_until=lambda *a, **k: None)

SRC = Q + "/hermes_cli/gateway_launchd.py"
spec = importlib.util.spec_from_file_location("w13_gatewaylau_3aa886_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w13_gatewaylau_3aa886_live"] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, (set, frozenset)):
        return sorted((compat(x) for x in v), key=repr)
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    return v


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
            py = {
                "JOB_UNLOADED": sorted(live._LAUNCHD_JOB_UNLOADED_EXIT_CODES),
                "DOMAIN_UNSUPPORTED": sorted(live._LAUNCHCTL_DOMAIN_UNSUPPORTED_CODES),
                "BOOTSTRAP_EIO": live._LAUNCHCTL_BOOTSTRAP_EIO,
            }
        elif func == "label":
            live._gw = lambda: types.SimpleNamespace(_profile_suffix=lambda: r.get("suffix", ""))
            py = live.get_launchd_label()
        elif func == "unloaded":
            py = live._launchd_error_indicates_unloaded(
                types.SimpleNamespace(returncode=int(r.get("returncode", 0))))
        elif func == "unsupported":
            py = live._launchctl_domain_unsupported(int(r.get("returncode", 0)))
        elif func == "prog_args":
            py = live.launchd_program_arguments(
                list(r.get("command", [])), r.get("stdout_log", ""), r.get("stderr_log", ""))
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

print(f"ORACLE w13_gatewaylau_3aa886: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
