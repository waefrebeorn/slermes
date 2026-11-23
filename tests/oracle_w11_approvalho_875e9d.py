#!/usr/bin/env python3
"""Differential oracle for w11_approvalho_875e9d (tools/approval_home_fold.py).

hermes_platform.host.facts and the os.environ-backed values are stubbed per
case so the pure path-folding logic is compared without touching this process.
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


pkg = _mod("hermes_platform")
pkg.__path__ = []
host = _mod("hermes_platform.host")
host.__path__ = []
facts = _mod("hermes_platform.host.facts", os_family=lambda: "posix")
host.facts = facts

SRC = Q + "/tools/approval_home_fold.py"
spec = importlib.util.spec_from_file_location("w11_approvalho_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_approvalho_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    win32 = bool(r.get("win32", False))
    facts.os_family = (lambda: "win32") if win32 else (lambda: "posix")
    env = {}
    for k, name in (("home_env", "HOME"), ("local_appdata", "LOCALAPPDATA"),
                    ("appdata", "APPDATA"), ("userprofile", "USERPROFILE")):
        if k in r:
            env[name] = r[k]
    expand_home = r.get("expand_home", "")
    live.os = types.SimpleNamespace(
        environ=env,
        path=types.SimpleNamespace(expanduser=lambda p: expand_home,
                                   realpath=lambda p: r.get("real_home", "")))
    func = r.get("func")
    try:
        if func == "msys_drive_spellings":
            py = live._msys_drive_spellings(r.get("path", ""))
        elif func == "path_below":
            py = live._path_below(r.get("root", ""), r.get("path", ""))
        elif func == "home_spellings":
            py = live._home_spellings(r.get("paths", []), r.get("command", ""))
        elif func == "fold_home_prefixes":
            py = live._fold_home_prefixes(r.get("command", ""), r.get("paths", []),
                                          r.get("replacement", ""))
        elif func == "rewrite_user_home_ex":
            py = live._rewrite_resolved_user_home(r.get("command", ""))
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
print(f"ORACLE w11_approvalho_875e9d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
