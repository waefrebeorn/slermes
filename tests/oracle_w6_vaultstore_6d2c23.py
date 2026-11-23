#!/usr/bin/env python3
"""Differential oracle for agent/vault_store.py (noc_w6_vaultstore_6d2c23.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/agent/vault_store.py"
sys.path.insert(0, QUARRY)

# Stub the three heavy quarry imports so the LIVE module loads standalone.
_acl = types.ModuleType("hermes_dir_acl")
_acl.make_user_dir = lambda *a, **k: None
sys.modules["hermes_dir_acl"] = _acl
_const = types.ModuleType("hermes_constants")
_const.get_hermes_home = lambda: QUARRY
sys.modules["hermes_constants"] = _const
_utils = types.ModuleType("utils")
_utils.atomic_write_bytes = lambda *a, **k: None
sys.modules["utils"] = _utils

spec = importlib.util.spec_from_file_location("vault_store_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["vault_store_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", round(float(x), 9))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r["func"]
    if f == "vault_kinds":
        return list(live.VAULT_KINDS)
    if f == "login_identifier_types":
        return list(live.LOGIN_IDENTIFIER_TYPES)
    if f == "fields":
        k = r.get("kind")
        if k == "payment":
            return live.PAYMENT_FIELDS
        if k == "address":
            return live.ADDRESS_FIELDS
        return {}
    if f == "required_fields":
        return list(live.REQUIRED_FIELDS.get(r.get("kind"), ()))
    if f == "normalize_origin":
        try:
            return live.normalize_origin(r.get("value"))
        except Exception:
            return None
    if f == "normalize_otp":
        try:
            return live.normalize_otp_secret(r.get("value"))
        except Exception:
            return None
    if f == "scrub":
        return live.scrub_secret_from_text(r.get("text", ""), r.get("secret") or {})
    if f == "meta":
        return live.VaultStore._meta(r.get("rec") or {}).to_dict()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE vaultstore: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    if _canon(py) != _canon(r.get("out")):
        mism += 1
        print(f"MISMATCH vaultstore {r.get('func')} case={r.get('case')} "
              f"py={py!r} c={r.get('out')!r}", file=sys.stderr)
print(f"ORACLE vaultstore: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
