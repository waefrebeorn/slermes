#!/usr/bin/env python3
"""Differential oracle for gateway/session_hosted_members.py (w18_sessionhos_a3f189).

Every fixture line is replayed against the LIVE module and compared field for
field with the C11 harness output on stdin.

Seams: only the two live-I/O wrappers are replaced --
  * gateway.session_authorities.local_bots  -> the fixture's bot snapshot
  * gateway.run._load_gateway_config        -> the fixture's config object
Everything else (served_profile_name, hermes_home_key, IDENTIFIER_RE,
AuthorityRefusal._one_line, owner_scope, the module's logger) is the real code.
"""
import importlib.util
import json
import logging
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Pin the process home so the default-root identity served_profile_name compares
# against is a fixture path (shape rule: '<root>/profiles/<name>' -> name, every
# other home -> 'default') instead of this box's real AppData root. Without it a
# transient filesystem hiccup in get_default_hermes_root()/marker probing reads as
# 'default' for a profiles/<name> home and false-fails the oracle.
os.environ["HERMES_HOME"] = "C:/w18hos-homes/hostroot"

from hermes_constants import hermes_home_key  # noqa: E402


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


SA = _load("w18_sa_live", Q + "/gateway/session_authorities.py")
HRC = _load("w18_hrc_live", Q + "/gateway/hosted_rooms_common.py")
LIVE = _load("w18_sessionhos_a3f189_live", Q + "/gateway/session_hosted_members.py")

# The live module imports these lazily; install the real modules (with the two
# I/O seams swapped) under their real names.
GRUN = types.ModuleType("gateway.run")
CONFIG = [{}]
GRUN._load_gateway_config = lambda: CONFIG[0]
BOTS = [[]]
SA.local_bots = lambda runner: list(BOTS[0])

_gw = types.ModuleType("gateway")
_gw.__path__ = []
_gw.session_authorities = SA
_gw.hosted_rooms_common = HRC
_gw.run = GRUN
sys.modules["gateway"] = _gw
sys.modules["gateway.session_authorities"] = SA
sys.modules["gateway.hosted_rooms_common"] = HRC
sys.modules["gateway.run"] = GRUN

AuthorityRefusal = LIVE.AuthorityRefusal


class Capture(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.NOTSET)
        self.rows = []

    def emit(self, record):
        args = record.args or ()
        if isinstance(args, tuple) and len(args) >= 3:
            self.rows.append([str(args[0]), str(args[2])])
        else:
            self.rows.append([record.getMessage(), ""])


CAP = Capture()
LIVE.logger.handlers = [CAP]
LIVE.logger.setLevel(logging.WARNING)
LIVE.logger.propagate = False


class Authority:
    def __init__(self, profile_id):
        self.profile_id = Path(profile_id or "")
        self.runner = None
        self.retiring = False


def _bots(r, auth):
    out = []
    for b in r.get("bots") or []:
        home = Path(b.get("home") or "")
        owner = auth if b.get("is_self") else Authority(str(home))
        out.append(SA.LocalBot(b.get("name") or "", home, owner,
                               b.get("problem"), bool(b.get("retiring"))))
    return out


def _setup(r):
    auth = Authority(r.get("profile_id", ""))
    BOTS[0] = _bots(r, auth)
    CONFIG[0] = r.get("config") if r.get("config") is not None else {}
    return auth


def _homes(homes):
    return {k: str(v) for k, v in (homes or {}).items()}


def _refused(refused):
    return {k: [v[0], v[1], v[2]] for k, v in (refused or {}).items()}


def compute(r):
    func = r.get("func")
    if func == "member_roster":
        auth = _setup(r)
        CAP.rows.clear()
        homes, refused = LIVE.member_roster(auth, siblings=bool(r.get("siblings", True)))
        return {"homes": _homes(homes), "refused": _refused(refused),
                "reports": list(CAP.rows)}
    if func == "served":
        auth = _setup(r)
        CAP.rows.clear()
        own, homes, refused = LIVE._served(auth)
        return {"own": own, "homes": _homes(homes), "refused": _refused(refused)}
    if func == "configured":
        auth = _setup(r)
        CAP.rows.clear()
        profiles = LIVE._configured(auth, r.get("own", ""))
        return {"profiles": dict(profiles or {}), "reports": list(CAP.rows)}
    if func == "config_entry":
        served = {k: Path(v) for k, v in (r.get("served") or {}).items()}
        seated = dict(r.get("seated") or {})
        home, reason, detail = LIVE._config_entry(r.get("name"), r.get("value"),
                                                  served, seated)
        return {"home": str(home) if home is not None else None,
                "reason": reason, "detail": detail}
    if func == "report":
        CAP.rows.clear()
        LIVE._report(r.get("own", ""), r.get("field", ""), r.get("value"),
                     r.get("detail", ""))
        return {"logged": bool(CAP.rows)}
    if func == "refuse_unseatable":
        members = r.get("members")
        refused = {k: tuple(v) for k, v in (r.get("refused") or {}).items()}
        try:
            LIVE.refuse_unseatable(members, refused)
        except AuthorityRefusal as exc:
            return {"raised": True, "reason": exc.reason,
                    "field": exc.field, "detail": exc.detail}
        return {"raised": False}
    if func == "path_str":
        return str(Path(r.get("value", "")))
    if func == "home_key":
        return hermes_home_key(r.get("value", ""))
    if func == "served_profile_name":
        return SA.served_profile_name(r.get("value", ""))
    if func == "identifier_ok":
        return bool(HRC.IDENTIFIER_RE.fullmatch(r.get("name", "")))
    raise SystemExit("unknown func %r" % (func,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:  # noqa: BLE001 - a live crash is a real mismatch
        mism += 1
        print("MISMATCH %s case=%s py=EXC(%r) c=%r"
              % (r.get("func"), r.get("case"), exc, r.get("out")), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r"
              % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w18_sessionhos_a3f189: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
