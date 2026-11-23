#!/usr/bin/env python3
"""Differential oracle for hermes_cli/old_updater_main.py (gate w15_oldupdater_e72031)."""
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
    raise _Captured("delegate|relaunch_stopped_serves")

_old = types.ModuleType("hermes_cli._old_updater")
_old.stop_historical_updater = _fake_stop
_old.RetiredUpdaterHook = _RetiredUpdaterHook
_old.relaunch_stopped_serves = _fake_relaunch
sys.modules["hermes_cli._old_updater"] = _old

SRC = Q + "/hermes_cli/old_updater_main.py"
spec = importlib.util.spec_from_file_location("oldmain_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["oldmain_live"] = live
spec.loader.exec_module(live)
live.stop_historical_updater = _fake_stop

CALL = {'desktop_stamp_path': ('_desktop_stamp_path', [], {}), 'expected_windows_pe_machines': ('_expected_windows_pe_machines', [], {}), 'hermes_exe_shims': ('_hermes_exe_shims', ['dir'], {}), 'insert_python_pin': ('_insert_python_pin', [[]], {}), 'interpreter_scripts_dir': ('_interpreter_scripts_dir', [], {}), 'load_installable_optional_extras': ('_load_installable_optional_extras', [], {}), 'parse_pe_machine': ('_parse_pe_machine', ['p'], {}), 'quarantine_running_hermes_exe': ('_quarantine_running_hermes_exe', ['dir'], {'failed_out': None}), 'repair_broken_lazy_refresh_imports': ('_repair_broken_lazy_refresh_imports', [[], []], {}), 'run_install_with_heartbeat': ('_run_install_with_heartbeat', [[]], {}), 'run_package_only_install': ('_run_package_only_install', [[]], {}), 'run_quarantined_install': ('_run_quarantined_install', [[]], {}), 'run_with_idle_timeout': ('_run_with_idle_timeout', [[], 'cwd'], {}), 'self': ('_self', [], {}), 'verify_console_scripts_installed': ('_verify_console_scripts_installed', [[]], {}), 'verify_core_dependencies_installed': ('_verify_core_dependencies_installed', [[]], {}), 'web_ui_build_needed': ('_web_ui_build_needed', ['dir'], {}), 'windows_native_machine': ('_windows_native_machine', [], {}), 'windows_shim_in_process_chain': ('_windows_shim_in_process_chain', [], {}), 'detect_broken_lazy_refresh_imports': ('_detect_broken_lazy_refresh_imports', [[]], {'env': None}), 'resolve_install_target_python': ('_resolve_install_target_python', [[], None], {}), 'restore_quarantined_exes': ('_restore_quarantined_exes', [[]], {}), 'write_web_ui_build_stamp': ('_write_web_ui_build_stamp', ['root', 'dir'], {})}

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
        if f == "__none__":
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
print(f"ORACLE w15_oldupdater_e72031: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
