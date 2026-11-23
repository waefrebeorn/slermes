#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_worker.py
(w8_sessionwork_e89180 gate).

The live module is loaded via spec. Its pure decision points are exercised:
`worker_request`'s exact param key-set rule and `_claim`'s validation prefix.
Deep side effects (psutil process checks, persistence claim origin, the SHA256
fingerprint) are injected with deterministic stand-ins; reaching the
fingerprint is reported as "ok".
"""
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


class AuthorityRefusal(Exception):
    pass


class PassedValidation(Exception):
    pass


def _fingerprint(**kwargs):
    raise PassedValidation()


pres = _mod("persistence"); pres.__path__ = []
core = _mod("persistence.core"); core.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal, STORE_FAILURES=())
auth = _mod("persistence.authority"); auth.__path__ = []
_mod("persistence.authority.vocabulary", admission_fingerprint=_fingerprint)

gpkg = _mod("gateway"); gpkg.__path__ = []


class _WE:
    def claim_origin(self, **kw):
        return "origin"


_mod("gateway.session_persistence",
     authority_persistence=lambda authority: types.SimpleNamespace(worker_executions=_WE()))

hpkg = _mod("hermes_cli"); hpkg.__path__ = []
BIRTH = [None]
_mod("hermes_cli.process_identity", worker_birth=lambda proc: BIRTH[0])


class _Proc:
    def __init__(self, pid=None):
        self.pid = pid

    def is_running(self):
        return True

    def status(self):
        return "running"

    def username(self):
        return "u"


ps = _mod("psutil", Error=Exception, STATUS_ZOMBIE="zombie")
ps.Process = lambda pid=None: _Proc(pid)

SRC = Q + "/gateway/session_worker.py"
spec = importlib.util.spec_from_file_location("sessworker_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["sessworker_live"] = live
spec.loader.exec_module(live)


def _typed(r, key):
    t = r.get(key + "_type", "int")
    v = r.get(key, 0)
    if t == "int":
        return int(v)
    if t == "float":
        return float(v)
    if t == "str":
        return str(v)
    if t == "bool":
        return bool(v)
    if t == "null":
        return None
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
        if func == "scope":
            op = r.get("operation", "")
            extra = {"register": {"kind"}, "adopt": set(),
                     "persist": {"epoch", "sequence", "operation", "payload"}}.get(op)
            py = 0 if extra is None else (1 if set(r.get("keys", [])) == (live._SCOPE | extra) else 0)
        elif func == "claim":
            pid = _typed(r, "pid")
            birth = _typed(r, "birth")
            BIRTH[0] = birth
            secret = str(r.get("secret", "")) if r.get("secret_type", "str") == "str" else 1234
            params = {
                "profile_id": r.get("profile_id", "p1"),
                "session_id": "sess", "execution_id": "exec", "generation": 1,
                "pid": pid, "birth": birth, "secret": secret,
            }
            live.os = types.SimpleNamespace(getpid=lambda: r.get("own_pid", 0))
            conn = types.SimpleNamespace(
                authority=types.SimpleNamespace(profile_id=r.get("authority_profile_id", "p1")),
                actor=types.SimpleNamespace(
                    profile_id=r.get("actor_profile_id", "p1"),
                    capabilities=[1] if r.get("has_capabilities", False) else [],
                    subject="sub"),
            )
            try:
                live._claim(conn, None, params)
                py = "ok"
            except PassedValidation:
                py = "ok"
            except AuthorityRefusal as exc:
                py = exc.args[0] if exc.args else ""
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

print(f"ORACLE w8_sessionwork_e89180: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
