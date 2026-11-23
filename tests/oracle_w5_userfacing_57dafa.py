#!/usr/bin/env python3
"""Differential oracle for pm/user_facing.py (gate w5_userfacing_57dafa).

Environment-dependent helpers are injected from each fixture so the live
module's real copies()/user_facing_path() algorithms run deterministically:
  copies:     applies, _sealed_dirs, _copy_dirs
  path:       pm.install._store_path_dirs, copies (2 calls), _python_dirs,
              _python_stub_dir
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# pm.install is only needed for the store-dir import inside user_facing_path.
pm = types.ModuleType("pm")
pm.__path__ = []
sys.modules["pm"] = pm
pminstall = types.ModuleType("pm.install")
pminstall._store_path_dirs = lambda: []
sys.modules["pm.install"] = pminstall

SRC = Q + "/pm/user_facing.py"
spec = importlib.util.spec_from_file_location("userfacing_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["userfacing_live"] = live
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
    func = r.get("func")
    try:
        if func == "copies":
            sealed = r.get("sealed") or {}
            cp = r.get("copies_map") or {}
            live.applies = lambda: bool(r.get("applies", False))
            live._sealed_dirs = lambda name: list(sealed.get(name, []))
            live._copy_dirs = lambda name: list(cp.get(name, []))
            py = live.copies(list(r.get("dirs") or []))
        elif func == "path":
            store = list(r.get("store") or [])
            ours = list(r.get("ours") or [])
            python_copies = list(r.get("python_copies") or [])
            flags = r.get("stub_flags") or {}
            pminstall._store_path_dirs = lambda: store
            calls = {"n": 0}

            def fake_copies(d):
                calls["n"] += 1
                return list(ours if calls["n"] == 1 else python_copies)

            live.copies = fake_copies
            live._python_dirs = lambda: ["__injected__"]
            live._python_stub_dir = lambda d: bool(flags.get(os.path.normcase(d), False))
            py = live.user_facing_path(r.get("path", ""))
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

print(f"ORACLE w5_userfacing_57dafa: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
