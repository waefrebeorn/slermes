#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profile_identity.py (gate w15_profileide_89a518)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_profileide_89a518"
sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# parent-package bootstrap + stubs for the deferred imports
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = []
sys.modules["hermes_cli"] = _hc
_gw = types.ModuleType("gateway")
_gw.__path__ = []
sys.modules["gateway"] = _gw
_persist = _mod("persistence")
_persist.ProfileIdentityCollision = type("ProfileIdentityCollision", (ValueError,), {})
_mod("storage_user_copy", error_type_name=lambda exc: type(exc).__name__)

_state = {"identified": False, "raises": False}


def identify_gateway(root):
    if _state["raises"]:
        raise RuntimeError("offline")
    return "gateway" if _state["identified"] else None


_mod("gateway.control_socket", identify_gateway=identify_gateway,
     purge_gateway_profile_identity=lambda *a: None,
     migrate_gateway_profile_identity=lambda *a: None)

SRC = QUARRY + "/hermes_cli/profile_identity.py"
_spec = importlib.util.spec_from_file_location("profile_identity_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["profile_identity_live"] = live
_spec.loader.exec_module(live)

_CLASSES = {"ValueError": ValueError, "TypeError": TypeError, "OSError": OSError,
            "RuntimeError": RuntimeError, "Exception": Exception}


def live_out(rec):
    f = rec.get("func")
    if f == "control_failure":
        return live._control_answer_failure(rec.get("answer"))
    if f == "store_error":
        cls = rec.get("class_name") or "Exception"
        msg = rec.get("message") or ""
        exc = (_persist.ProfileIdentityCollision(msg) if rec.get("collision")
               else _CLASSES.get(cls, Exception)(msg))
        return live._store_error_text(exc)
    if f == "gateway_verb":
        _state["identified"] = bool(rec.get("identified", False))
        _state["raises"] = bool(rec.get("raises", False))
        return live._gateway_accepts_profile_identity_verb("root")
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("control_failure", "store_error", "gateway_verb"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            print(f"LIVE ERROR {rec.get('func')}: {exc!r}", file=sys.stderr)
            mism += 1
            continue
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
