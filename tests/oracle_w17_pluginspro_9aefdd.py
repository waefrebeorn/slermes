#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_provenance.py (w17_pluginspro_9aefdd).

classify: mirrors the LIVE 2x2 using live.ProvenanceClass values (the module has
no standalone function taking rows+dirs). git_origin: calls the LIVE
_git_origin_url with git resolution stubbed off and the fixture's config text
written into a real .git/config.
"""
import importlib.util
import json
import os
import pathlib
import sys
import tempfile
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap + sibling stub so the module (and its call-time
# import of _resolve_git_executable) load without the real CLI environment.
pkg = types.ModuleType("hermes_cli")
pkg.__path__ = [os.path.join(Q, "hermes_cli")]
sys.modules.setdefault("hermes_cli", pkg)
pc = types.ModuleType("hermes_cli.plugins_cmd")
pc._resolve_git_executable = lambda: None
sys.modules["hermes_cli.plugins_cmd"] = pc

SRC = Q + "/hermes_cli/plugins_provenance.py"
spec = importlib.util.spec_from_file_location("hermes_cli.plugins_provenance", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.plugins_provenance"] = live
spec.loader.exec_module(live)


def classify(rows, dirs):
    out = []
    for d in dirs or []:
        name = d.get("name", "")
        has_git = bool(d.get("has_git", False))
        row = (rows or {}).get(name)
        row = row if isinstance(row, dict) else None
        if row and has_git:
            klass = live.ProvenanceClass.GIT.value
        elif row and not has_git:
            klass = live.ProvenanceClass.DRIFT.value
        elif has_git and not row:
            klass = live.ProvenanceClass.SELF_CLONED.value
        else:
            klass = live.ProvenanceClass.MANUAL.value
        out.append({"name": name, "klass": klass,
                    "saved_update_url": (row or {}).get("update_url") or None})
    return out


def git_origin(text):
    with tempfile.TemporaryDirectory() as d:
        gd = pathlib.Path(d) / ".git"
        gd.mkdir()
        with open(str(gd / "config"), "w", encoding="utf-8", newline="") as fh:
            fh.write(text)
        return live._git_origin_url(pathlib.Path(d))


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
        if func == "classify":
            py = classify(r.get("rows", {}), r.get("dirs", []))
        elif func == "git_origin":
            py = git_origin(r.get("config", ""))
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

print(f"ORACLE w17_pluginspro_9aefdd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
