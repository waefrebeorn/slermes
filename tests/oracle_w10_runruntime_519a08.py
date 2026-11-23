#!/usr/bin/env python3
"""Differential oracle for gateway/run_runtime_init.py (gate w10_runruntime_519a08)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/gateway/run_runtime_init.py"
NAME = "run_runtime_init_live_w10_runruntime"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

# Fake the lazily-imported gateway.run surface; the sentinel is a private object.
_fg = types.ModuleType("gateway"); _fg.__path__ = []
sys.modules["gateway"] = _fg
_SENTINEL = object()
_mr = types.ModuleType("gateway.run")
for _n in ("Any", "Dict", "Optional", "OrderedDict", "Platform"):
    setattr(_mr, _n, object)
_mr.threading = types.SimpleNamespace(Lock=lambda: object())
_mr._SESSION_DB_UNPINNED = _SENTINEL
sys.modules["gateway.run"] = _mr
_fp = types.ModuleType("gateway.platforms"); _fp.__path__ = []
sys.modules["gateway.platforms"] = _fp
_fh = types.ModuleType("gateway.platforms.helpers")
_fh.prompt_id_counter = lambda: 7
sys.modules["gateway.platforms.helpers"] = _fh


def compute(r):
    func = r.get("func")
    if func == "persistence":
        class FakeRunner:
            def __init__(self):
                if r.get("pinned"):
                    self._session_db_pinned = "PIN"
                if r.get("has_store"):
                    self.session_store = types.SimpleNamespace(
                        persistence=r.get("store_persistence"))

            def _open_session_db_for_active_scope(self):
                return r.get("scope_result")

        return live.GatewayRuntimeInitMixin.persistence.fget(FakeRunner())
    if func == "profile_name":
        class FakeRunner:
            def __init__(self):
                self.config = types.SimpleNamespace(
                    multiplex_profiles=bool(r.get("multiplexed", False)))

            def _active_profile_name(self):
                return r.get("launch", "")

        fake = FakeRunner()
        live.GatewayRuntimeInitMixin._init_runtime_caches(fake)
        return {"primary": fake._primary_profile_name,
                "kanban": fake._kanban_notifier_profile}
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
print("ORACLE w10_runruntime_519a08: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
