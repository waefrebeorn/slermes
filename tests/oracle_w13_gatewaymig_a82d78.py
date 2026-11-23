#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_migrate.py (w13_gatewaymig_a82d78)."""
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
_pkg2 = _mod("agent", Q + "/agent")
_mod("agent.deadline", poll_until=lambda *a, **k: None)
_mod("hermes_cli.profile_channels", credential_env_keys=lambda: {})
_mod("hermes_cli.url_utils", format_url_host=lambda h: h)
_mod("hermes_cli.profiles", profile_is_parked=lambda p: False)
_mod("gateway", path=Q + "/gateway")


class Platform(str):
    @property
    def value(self):
        return str(self)


_mod("gateway.config", Platform=Platform)

SRC = Q + "/hermes_cli/gateway_migrate.py"
spec = importlib.util.spec_from_file_location("w13_gatewaymig_a82d78_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w13_gatewaymig_a82d78_live"] = live
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
            py = {"MANIFEST_NAME": live.MANIFEST_NAME, "MIGRATE_COMMAND": live.MIGRATE_COMMAND,
                  "SERVED_WAIT_SECONDS": live._SERVED_WAIT_SECONDS}
        elif func == "service_label":
            py = live._service_label((r.get("kind", ""), bool(r.get("system", False))))
        elif func == "remove_verb":
            py = live._remove_verb((r.get("kind", ""), False))
        elif func == "service_dict":
            py = compat(live._service_dict((r.get("kind", ""), bool(r.get("system", False)))))
        elif func == "key_names":
            sys.modules["hermes_cli.profile_channels"].credential_env_keys = lambda: dict(r.get("env_keys", {}))
            py = live._credential_key_names(r.get("platform_value", ""))
        elif func == "listener_url":
            pv = r.get("platform_value", "")
            extra = {}
            if r.get("extra_host"):
                extra["host"] = r["extra_host"]
            if r.get("extra_port"):
                extra["port"] = r["extra_port"]
            cfg = types.SimpleNamespace(platforms={Platform(pv): types.SimpleNamespace(extra=extra)})
            py = live._listener_url(cfg, pv, r.get("profile", ""))
        elif func == "manifest_secondaries":
            py = compat(live._manifest_secondaries(r.get("manifest") or {}))
        elif func == "recorded_services":
            py = compat(live._recorded_services(r.get("rec") or {}))
        elif func == "recorded_user":
            py = live._recorded_run_as_user(r.get("rec") or {})
        elif func == "target":
            py = compat(live._target_from_manifest(r.get("manifest") or {}))
        elif func == "not_yet_served":
            parked = set(r.get("parked") or [])
            sys.modules["hermes_cli.profiles"].profile_is_parked = lambda p: str(p) in parked
            py = live._manifest_not_yet_served(r.get("manifest"), r.get("live_served"))
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

print(f"ORACLE w13_gatewaymig_a82d78: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
