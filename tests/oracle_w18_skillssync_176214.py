#!/usr/bin/env python3
"""Oracle for gate w18_skillssync_176214 (tools/skills_sync_bundled_ops.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    # tools.skills_sync_optional is imported at module load; stub its 3 names.
    dep = types.ModuleType("tools.skills_sync_optional")
    dep._ignore_runtime_cache = lambda *a, **k: None
    dep._skill_file_list = lambda *a, **k: []
    dep._ss = lambda *a, **k: None
    sys.modules["tools.skills_sync_optional"] = dep
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m


live = _load("tools.skills_sync_bundled_ops", "tools/skills_sync_bundled_ops.py")

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
    if func != "bundled_skill_for_install":
        continue
    bundled = r.get("bundled") or []
    live._bundled_state = lambda: (None, None, "/b", {n: "/b/" + n for n in bundled})
    try:
        py = live.bundled_skill_for_install(r.get("identifier", ""))
    except Exception as exc:
        print(f"SKIP: {exc}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_skillssync_176214: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
