#!/usr/bin/env python3
"""Differential oracle for gateway/run_profile_home.py (gate w10_runprofile_b87e2d)."""
import hermes_constants
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/gateway/run_profile_home.py"
NAME = "run_profile_home_live_w10_runprofile"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class ProfileRouteRejected(Exception):
    def __init__(self, profile, *args):
        self.profile = profile
        super().__init__(profile)


CTX = {}

# Faked collaborators (the live module imports all of them lazily inside functions).
_fg = types.ModuleType("gateway"); _fg.__path__ = []
sys.modules["gateway"] = _fg
_fh = types.ModuleType("hermes_cli"); _fh.__path__ = []
sys.modules["hermes_cli"] = _fh

_mpr = types.ModuleType("gateway.profile_routing")
_mpr.ProfileRouteRejected = ProfileRouteRejected
sys.modules["gateway.profile_routing"] = _mpr

_msi = types.ModuleType("gateway.session_identity")


def _identity_of(source):
    ih = CTX.get("identity_home", None)
    return SimpleNamespace(runtime_home=ih) if ih is not None else None


_msi.identity_of = _identity_of
sys.modules["gateway.session_identity"] = _msi

_mrun = types.ModuleType("gateway.run")
_mrun._multiplex_profile_homes = lambda cfg: [
    (n, "/h/" + str(n)) for n in (CTX.get("served") or [])]
sys.modules["gateway.run"] = _mrun

_mp = types.ModuleType("hermes_cli.profiles")
_mp.get_active_profile_name = lambda: CTX.get("active_name", "default")
_mp.get_profile_dir = lambda name: (CTX.get("profiles") or {}).get(name, {}).get("dir", "/none")
_mp.normalize_profile_name = lambda name: (CTX.get("profiles") or {}).get(name, {}).get("norm", name)
_mp.profile_exists = lambda name: bool((CTX.get("profiles") or {}).get(name, {}).get("exists", False))
sys.modules["hermes_cli.profiles"] = _mp

hermes_constants.get_hermes_home = lambda: CTX.get("home", "/global")


def compute(r):
    func = r.get("func")
    CTX.clear()
    CTX.update(r)
    if func == "served_names":
        return sorted(live._served_names(None))
    if func == "resolve_source":
        source = SimpleNamespace(
            profile=r.get("source_profile", ""),
            platform=SimpleNamespace(value="telegram"),
            chat_id="c", guild_id="g")
        runner = SimpleNamespace(
            config=SimpleNamespace(multiplex_profiles=bool(r.get("multiplexed", False))),
            _profile_name_for_source=lambda s: r.get("derived_name", ""))
        try:
            return {"home": str(live.resolve_profile_home_for_source(runner, source))}
        except ProfileRouteRejected as exc:
            return {"reject": exc.profile}
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w10_runprofile_b87e2d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
