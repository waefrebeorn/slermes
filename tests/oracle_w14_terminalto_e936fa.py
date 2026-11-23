#!/usr/bin/env python3
"""Differential oracle for Hermes tools/terminal_tool_config.py (gate w14_terminalto_e936fa).

All stdlib-only at module level; no stubs are needed. The scope-aware _tenv()
read is patched from the fixture's `value` field so the live _tenv_bool is
exercised without mutating the process environment (Windows CRT setenv("")
unsets; Python os.environ[name]="" keeps it).
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/terminal_tool_config.py"
spec = importlib.util.spec_from_file_location("w14_terminalto_e936fa_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w14_terminalto_e936fa_live"] = live
spec.loader.exec_module(live)


def _tenv_stub(name, default=""):
    # last fixture's value is installed per case below
    return _tenv_stub.value


_tenv_stub.value = ""
live._tenv = _tenv_stub

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
        if func == "profile_container_key":
            py = live.profile_container_key(r.get("profile"))
        elif func == "is_host_cwd":
            py = live._is_host_cwd(r.get("path", ""))
        elif func == "is_windows_drive_path":
            py = live._is_windows_drive_path(r.get("path", ""))
        elif func == "host_path_key":
            py = live._host_path_key(r.get("path", ""))
        elif func == "translate":
            py = live.translate_mounted_host_path(
                r.get("path"), r.get("host_root"), r.get("container_root"))
        elif func == "is_unusable_cwd":
            py = live._is_unusable_container_cwd(r.get("cwd", ""))
        elif func == "cwd_follows_host_mount":
            py = live.cwd_follows_host_mount(r.get("cwd", ""), r.get("mount", ""))
        elif func == "tenv_bool":
            _tenv_stub.value = r.get("value", "")
            py = live._tenv_bool("TERMINAL_X", r.get("value", ""))
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

print(f"ORACLE w14_terminalto_e936fa: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
