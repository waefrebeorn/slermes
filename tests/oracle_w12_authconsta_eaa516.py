#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_constants.py (w12_authconsta_eaa516)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/auth_constants.py"
spec = importlib.util.spec_from_file_location("auth_constants_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["auth_constants_live"] = live
spec.loader.exec_module(live)

CONST_KEYS = [k for k in vars(live)
              if (k.isupper() or (k.startswith("_") and k[1:].isupper()))
              and isinstance(getattr(live, k), (str, int, float, bool))]


def b64url(s):
    import base64
    s += "=" * ((4 - len(s) % 4) % 4)
    return base64.urlsafe_b64decode(s.encode("utf-8"))


def jwt_token(claims):
    header = b64url(json.dumps({"alg": "none"}).encode()).decode().rstrip("=")
    payload = b64url(json.dumps(claims).encode()).decode().rstrip("=")
    return header + "." + payload + ".sig"


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
        if func == "decode":
            py = live._decode_jwt_claims(r.get("token", ""))
        elif func == "error":
            e = live._provider_error_factory(r.get("provider", ""))(
                r.get("message", ""), r.get("code"),
                relogin=bool(r.get("relogin", False)))
            py = {"message": str(e), "provider": e.provider, "code": e.code,
                  "relogin_required": e.relogin_required, "retry_after": e.retry_after,
                  "retryable": e.retryable}
        elif func == "constants":
            py = {k: getattr(live, k) for k in CONST_KEYS}
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

print(f"ORACLE w12_authconsta_eaa516: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
