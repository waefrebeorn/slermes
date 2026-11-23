#!/usr/bin/env python3
"""Differential oracle for hermes_cli/old_updater_deps.py (gate w15_oldupdater_cc42de)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

class _Captured(Exception):
    def __init__(self, payload):
        self.payload = payload

class _RetiredUpdaterHook(RuntimeError):
    pass

def _fake_stop(hook, error=None):
    name = "RetiredUpdaterHook" if error is None else error.__name__
    raise _Captured(name + "|" + hook)

def _fake_relaunch(token):
    if not token.get("pending"):
        return
    raise _Captured("delegate|relaunch_stopped_serves")

_old = types.ModuleType("hermes_cli._old_updater")
_old.stop_historical_updater = _fake_stop
_old.RetiredUpdaterHook = _RetiredUpdaterHook
_old.relaunch_stopped_serves = _fake_relaunch
sys.modules["hermes_cli._old_updater"] = _old

SRC = Q + "/hermes_cli/old_updater_deps.py"
spec = importlib.util.spec_from_file_location("olddeps_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["olddeps_live"] = live
spec.loader.exec_module(live)
live.stop_historical_updater = _fake_stop

CALL = {'capture_active_lazy_features': ('_capture_active_lazy_features', [], {}), 'refresh_active_lazy_features': ('_refresh_active_lazy_features', [], {}), 'refresh_active_memory_provider_dependencies': ('_refresh_active_memory_provider_dependencies', [], {}), 'npm_lockfile_changed': ('_npm_lockfile_changed', ['root'], {}), 'update_node_dependencies': ('_update_node_dependencies', [], {}), 'rebuild_desktop_after_update': ('_rebuild_desktop_after_update', ['dir'], {'had_desktop_app_before_update': True}), 'write_update_incomplete_marker': ('_write_update_incomplete_marker', [], {}), 'path_uid': ('_path_uid', ['p'], {}), 'leftover_pausable_gateway_pids': ('_leftover_pausable_gateway_pids', [[]], {}), 'ledger_manual_serve_holders': ('_ledger_manual_serve_holders', [[]], {}), 'relaunch_stopped_serves': ('_relaunch_stopped_serves', [], {}), 'orphaned_desktop_backend_pids': ('_orphaned_desktop_backend_pids', [[]], {}), 'ledger_reapable_backend_pids': ('_ledger_reapable_backend_pids', [[]], {}), 'handoff_reapable_backend_pids': ('_handoff_reapable_backend_pids', [[]], {}), 'stop_process_trees': ('_stop_process_trees', [[]], {})}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    f = r.get("func")
    if f not in CALL:
        continue
    py_name, args, kw = CALL[f]
    try:
        if f == "relaunch_stopped_serves":
            getattr(live, py_name)({"pending": bool(r.get("pending", False))})
        else:
            getattr(live, py_name)(*args, **kw)
        expected = None
    except _Captured as c:
        expected = c.payload
    cases += 1
    got = r.get("out")
    if expected != got:
        mism += 1
        print(f"MISMATCH {f} case={r.get('case')} py={expected!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w15_oldupdater_cc42de: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
