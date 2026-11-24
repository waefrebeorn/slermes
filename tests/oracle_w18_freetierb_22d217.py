#!/usr/bin/env python3
"""Differential oracle for hermes_cli/free_tier_bootstrap.py (gate w18_freetierb_22d217)."""
import importlib.util, json, sys, types
from dataclasses import asdict

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

spec = importlib.util.spec_from_file_location(
    "w18_freetierb_live", Q + "/hermes_cli/free_tier_bootstrap.py")
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

_FIELDS = ("provider_configured", "inference_provider", "free_tier_account",
           "has_identity", "other_providers", "error", "failure", "finished_at")


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {str(k): _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
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
    try:
        if func == "free_tier_route":
            rec = live.SetupRecord(
                free_tier_account=bool(r.get("free_tier_account", False)),
                inference_provider=r.get("inference_provider") or "")
            py = rec.free_tier_route
        elif func == "as_payload":
            f = r.get("record") or {}
            kwargs = {k: f[k] for k in _FIELDS if k in f}
            py = live.SetupRecord(**kwargs).as_payload()
        elif func == "build_record":
            state = r.get("state")
            failure = r.get("failure") or {}
            other = bool(r.get("other", False))
            free_active = bool(r.get("free_active", False))
            is_guest = bool(r.get("is_guest", False))
            provider = r.get("inference_provider") or ""
            at = float(r.get("finished_at", 0.0))
            _seed("hermes_cli.anon_auth",
                  current_nous_state=lambda: state,
                  last_mint_failure=lambda: failure,
                  free_inference_active=lambda s: free_active,
                  is_guest_state=lambda s: is_guest)
            live._resolve_inference = lambda: provider
            py = asdict(live._build_record(other=other))
            # finished_at's default_factory was bound at class creation; inject the clock here.
            py["finished_at"] = at
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

print(f"ORACLE w18_freetierb_22d217: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
