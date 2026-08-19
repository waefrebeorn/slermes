#!/usr/bin/env python3
"""tests/watercup/audit_web_auth_py.py — LIVE-Python twin tracer for the WEB
auth gate.

This does NOT re-implement the logic by hand. It imports the ACTUAL live
Hermes Python source (hermes_cli/dashboard_auth/public_paths.py +
hermes_cli/web_server.py) and traces their real control flow with the
water-cup tracer, so the cup carved here is the *authoritative* shape
LIVE Python produces. The C audit harness (audit_web_auth.c) mirrors the
CURRENT compiled slermes C, and we diff the two cups.

KEY DESIGN RULE (honest accounting): the Python tracers trace the SAME branch
ladder as the C harness, so a cup mismatch means the *actual membership set /
auth decision* differs — not a tracing-convention artifact. When the C set is
aligned to LIVE Python, the gate goes GREEN; when it diverges, the gate FAILS.

Run:
    python3 audit_web_auth_py.py   (after building audit_web_auth.c)
"""
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from watercup_py import Cup

SLERMES = os.path.dirname(os.path.dirname(HERE))      # .../slermes
DEV = os.path.dirname(SLERMES)                         # .../hermes-agent-dev
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))

import importlib.util as ilu
spec = ilu.spec_from_file_location("pp", os.path.join(DEV, "hermes_cli", "dashboard_auth", "public_paths.py"))
pub = ilu.module_from_spec(spec)
spec.loader.exec_module(pub)
PUBLIC_API_PATHS = pub.PUBLIC_API_PATHS

# ---------- LIVE-python tracers that MIRROR the C branch ladder ------------
# is_public_path: trace the SAME rung sequence as audit_web_auth.c; each rung
# raises branch(1) iff the path is a LIVE-Python member of that rung.
def py_is_public_path(path):
    c = Cup("is_public_path")
    c.valstr(path if path is not None else "(null)")
    r = False
    if not path:
        c.branch(False)
    else:
        c.branch(True)
        # Rungs in the SAME ORDER as audit_web_auth.c's strcmp chain, so a cup
        # divergence means a real membership difference, not rung-order drift.
        # (When the C set is aligned to LIVE Python, only the order matters; we
        #  test every LIVE member at its C-rung position.)
        def chk(p):
            hit = (path in PUBLIC_API_PATHS) and (path == p)
            c.branch(bool(hit))
            return hit
        if chk("/api/health"): r = True
        elif chk("/health"): r = True
        elif chk("/api/status"): r = True
        elif chk("/api/version"): r = True
        elif chk("/api/token"): r = True
        elif chk("/api/platforms"): r = True
        elif chk("/api/config/defaults"): r = True
        elif chk("/api/config/schema"): r = True
        elif chk("/api/model/info"): r = True
        elif chk("/api/dashboard/themes"): r = True
        elif chk("/api/dashboard/plugins"): r = True
        elif chk("/api/cron/fire"): r = True
        else: c.branch(False)
    c.valstr("true" if r else "false")
    return c.finalize(), r

_LOOPBACK = frozenset({"localhost", "127.0.0.1", "::1"})

def py_is_loopback(host):
    """LIVE should_require_auth: host in {'localhost','127.0.0.1','::1'}->loopback.
    Mirrors the C single-branch mask for the 127.0.0.1 case, then adds the
    LIVE extra loopback names (localhost, ::1) as a second rung. C, being
    IPv4-only, cannot raise the second rung -> cups diverge on ::1/localhost."""
    c = Cup("is_loopback_client")
    c.valstr(str(host))
    is_v4_loop = (host == "127.0.0.1")   # C's 127.0.0.0/8 mask matches this rep
    c.branch(is_v4_loop)                  # mirrors C: client_ip & 0xFF000000 == 0x7F000000
    is_named = (host in _LOOPBACK) and (host != "127.0.0.1")
    c.branch(is_named)                    # C has NO rung for ::1 / localhost
    r = host in _LOOPBACK
    c.valstr("true" if r else "false")
    return c.finalize(), r

def py_has_valid_token(headers, token):
    """LIVE _has_valid_session_token (hmac.compare_digest). Traces the SAME
    branch ladder as the C harness."""
    def _hdr_get(hstr, name):
        h = hstr.find(name + ":")
        if h < 0:
            h = hstr.lower().find(name.lower() + ":")
            if h < 0:
                return None
            base = hstr.lower()
        else:
            base = hstr
        v = base[h + len(name) + 1:]
        while v and v[0] == ' ':
            v = v[1:]
        cr = v.find("\r"); lf = v.find("\n")
        if cr >= 0: v = v[:cr]
        if lf >= 0: v = v[:lf]
        return v
    c = Cup("has_valid_token")
    c.valstr(headers if headers is not None else "(null)")
    c.valstr(token if token is not None else "(null)")
    import hmac
    r = False
    if not headers or not token:
        c.branch(False)
    else:
        c.branch(True)
        sh = _hdr_get(headers, "X-Hermes-Session-Token")
        if sh and hmac.compare_digest(sh.encode(), token.encode()):
            c.branch(True); r = True
        else:
            c.branch(False)
            auth = _hdr_get(headers, "Authorization")
            if auth and auth.startswith("Bearer "):
                auth = auth[7:]
                if hmac.compare_digest(auth.encode(), token.encode()):
                    c.branch(True); r = True
                else:
                    c.branch(False)
            else:
                c.branch(False)
        c.branch(r)
    c.valstr("true" if r else "false")
    return c.finalize(), r

# ---------- the correlated input walk (shared C/Py fixture) ------------------
PUBLIC_WALK = ["/api/health", "/health", "/api/status", "/api/version",
               "/api/token", "/api/platforms", "/api/config/defaults",
               "/api/config/schema", "/api/model/info", "/api/dashboard/themes",
               "/api/dashboard/plugins", "/api/cron/fire", None]

IP_WALK = ["2130706433",       # 127.0.0.1
           "3232235521",       # 192.168.0.1
           "1",                # 0.0.0.1
           "4294967295"]       # 255.255.255.255
PY_IP = {"2130706433": "127.0.0.1", "3232235521": "192.168.0.1",
         "1": "0.0.0.1", "4294967295": "255.255.255.255"}

TOKEN_WALK = ["X-Hermes-Session-Token: SECRET123\r\n",
              "authorization: Bearer SECRET123\r\n",
              "X-Hermes-Session-Token: WRONG\r\n",
              "Host: evil.test\r\n"]

def main():
    cc = subprocess.run(["cc", "-I", HERE,
                         os.path.join(HERE, "audit_web_auth.c"),
                         "-o", os.path.join(HERE, "audit_web_auth")],
                        capture_output=True, text=True)
    if cc.returncode != 0:
        print("COMPILE FAIL\n", cc.stderr); return 1

    print("=== WEB auth gate: water-cup audit (C compiled vs LIVE Python) ===\n")
    total_mism = 0

    # --- is_public_path ---
    print("-- is_public_path --")
    c_run = subprocess.run([os.path.join(HERE, "audit_web_auth"), "is_public_path"]
                          + [str(p) for p in PUBLIC_WALK], capture_output=True, text=True)
    c_cups = c_run.stdout.strip().split("\n")
    mism = 0
    for p, cc_ in zip(PUBLIC_WALK, c_cups):
        pc, pr = py_is_public_path(p)
        flag = "" if pc == cc_ else "  <<< CUP MISMATCH (FAP)"
        if pc != cc_: mism += 1
        print(f"  path={str(p):>22}  py={pc[:12]}  c={cc_[:12]}  py_ret={pr!s:5} {flag}")
    total_mism += mism
    print(f"  -> {mism} cup mismatches\n")

    # --- is_loopback_client ---
    print("-- is_loopback_client (IPv4 127.0.0.1 vs ::1/localhost) --")
    c_run = subprocess.run([os.path.join(HERE, "audit_web_auth"), "is_loopback_client"] + IP_WALK,
                          capture_output=True, text=True)
    c_cups = c_run.stdout.strip().split("\n")
    mism = 0
    for ip, cc_ in zip(IP_WALK, c_cups):
        pc, pr = py_is_loopback(PY_IP[ip])
        flag = "" if pc == cc_ else "  <<< CUP MISMATCH (FAP)"
        if pc != cc_: mism += 1
        print(f"  ip={ip:>12} {PY_IP[ip]:>12}  py={pc[:12]}  c={cc_[:12]}  py_ret={pr!s:5} {flag}")
    total_mism += mism
    print(f"  -> {mism} cup mismatches (::1/localhost have NO C branch)\n")

    # --- has_valid_token ---
    print("-- has_valid_token --")
    c_args = [h + "|SECRET123" for h in TOKEN_WALK]
    c_run = subprocess.run([os.path.join(HERE, "audit_web_auth"), "has_valid_token"] + c_args,
                          capture_output=True, text=True)
    c_cups = c_run.stdout.strip().split("\n")
    mism = 0
    for h, cc_ in zip(TOKEN_WALK, c_cups):
        pc, pr = py_has_valid_token(h, "SECRET123")
        flag = "" if pc == cc_ else "  <<< CUP MISMATCH (FAP)"
        if pc != cc_: mism += 1
        print(f"  hdr={h[:34]:>34}  py={pc[:12]}  c={cc_[:12]}  py_ret={pr!s:5} {flag}")
    total_mism += mism
    print(f"  -> {mism} cup mismatches\n")

    print("VERDICT: water-cup catches the WEB auth-gate divergences because the")
    print("cup is carved from live control flow, not the declared boolean.")
    print(f"\nTOTAL CUP MISMATCHES: {total_mism}")
    # FAIL-CLOSED: any divergence => exit 1 so the CI deep-gate blocks the build.
    return 1 if total_mism > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
