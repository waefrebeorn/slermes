#!/usr/bin/env python3
"""Differential oracle for gateway/run_profile_reconcile.py (w17_w17_runprofile_3fb921_3fb921)."""
import ast
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


gateway = _mod("gateway"); gateway.__path__ = []
_mod("gateway.run_shutdown", _log_suppressed=lambda *a, **k: None)
hcli = _mod("hermes_cli"); hcli.__path__ = []
_mod("hermes_cli.config_backend", ConfigBackendUnavailable=type("ConfigBackendUnavailable", (Exception,), {}))
utils = _mod("utils"); utils.__path__ = []
utils.file_cache_version = lambda p, **k: None

_state = {"opted_out": False, "implicit": None}
_mod("hermes_cli.gateway_migrate_guards", auto_migration_opted_out=lambda launch: _state["opted_out"])
# AST-extract the pinned reason from the live source; drift surfaces as a mismatch.
gm_src = open(Q + "/hermes_cli/gateway_multiplex_mode.py", encoding="utf-8").read()
REASON = None
for node in ast.parse(gm_src).body:
    if isinstance(node, ast.Assign) and any(getattr(t, "id", None) == "AUTO_MIGRATION_OPTED_OUT_REASON"
                                            for t in node.targets):
        REASON = ast.literal_eval(node.value)
        break
assert REASON, "AUTO_MIGRATION_OPTED_OUT_REASON not found"
_mod("hermes_cli.gateway_multiplex_mode",
     AUTO_MIGRATION_OPTED_OUT_REASON=REASON,
     implicit_multiplex_blocker=lambda: _state["implicit"])

persistence = _mod("persistence"); persistence.__path__ = []
phk = _mod("persistence.housekeeping"); phk.__path__ = []
class ProfileIdentityCollision(ValueError):
    pass
_mod("persistence.housekeeping.profile_identity", ProfileIdentityCollision=ProfileIdentityCollision)
# Only plain exceptions and ProfileIdentityCollision are exercised; for those,
# live error_type_name(x) is exactly type(x).__name__.
_mod("storage_user_copy", error_type_name=lambda exc: type(exc).__name__)

SRC = Q + "/gateway/run_profile_reconcile.py"
spec = importlib.util.spec_from_file_location("w17_runprofile_3fb921_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_runprofile_3fb921_live"] = live
spec.loader.exec_module(live)

Mixin = live.GatewayProfileReconcileMixin
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
        if func == "served":
            self_ = types.SimpleNamespace()
            self_._served_profile_homes = {k: k for k in r.get("homes", [])}
            if r.get("active") is not None:
                self_._primary_profile_name = r["active"]
            py = Mixin.served_profile_names(self_)
        elif func == "blocker":
            _state["opted_out"] = bool(r.get("opted_out", False))
            _state["implicit"] = r.get("implicit")
            py = live._added_profile_blocker("launch")
        elif func == "error":
            msg = r.get("message", "")
            if r.get("collision", False):
                exc = ProfileIdentityCollision(msg)
            else:
                Exc = type(r.get("type_name", "Exception"), (Exception,), {})
                exc = Exc(msg)
            py = live._identity_verb_error(exc)
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

print(f"ORACLE w17_runprofile_3fb921: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
