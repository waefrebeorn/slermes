#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/models_process_fingerprint.py (gate modfinger)."""
import importlib.util, json, sys, types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/models_process_fingerprint.py"
spec = importlib.util.spec_from_file_location("modfinger_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

# The live function's local imports and os access are fed by the harness ctx:
#   providers.get_provider_profile -> profile from ctx["profile"]
#   auth_external_process.*        -> command/binary from ctx["binary"]
#   os.environ / os.path.realpath  -> ctx["env"], identity realpath
_STATE = {"profile": None, "binary": None}

_providers = types.ModuleType("providers")
def _get_profile(name):
    return _STATE["profile"]
_providers.get_provider_profile = _get_profile
sys.modules["providers"] = _providers

_aep = types.ModuleType("hermes_cli.auth_external_process")
_aep.external_process_command = lambda profile: ""
_aep.resolve_external_process_command = lambda command: _STATE["binary"]
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli"); _pkg.__path__ = []; sys.modules["hermes_cli"] = _pkg
sys.modules["hermes_cli.auth_external_process"] = _aep

live.os = SimpleNamespace(
    environ={},
    path=SimpleNamespace(realpath=lambda p: p),
)

def _profile_from(d):
    if d is None: return None
    return SimpleNamespace(
        auth_type=d.get("auth_type"),
        process_command_env_vars=d.get("process_command_env_vars", []),
        process_args_env_var=d.get("process_args_env_var", ""),
        process_command=d.get("process_command", ""),
    )

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "process_catalog_identity": continue
    ctx = r.get("ctx") or {}
    _STATE["profile"] = _profile_from(ctx.get("profile"))
    _STATE["binary"] = ctx.get("binary")
    live.os.environ = dict(ctx.get("env") or {})

    py_profile, py_parts = live.process_catalog_identity(ctx.get("provider", ""))
    expected = {
        "profile": "PROFILE" if py_profile is not None else None,
        "parts": py_parts,
    }
    cases += 1
    got = r.get("out")
    if py_parts != (got or {}).get("parts") or expected["profile"] != (got or {}).get("profile"):
        mism += 1
        print(f"MISMATCH {r} py={expected!r} c={got!r}", file=sys.stderr)
print(f"ORACLE modfinger: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
