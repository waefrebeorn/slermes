#!/usr/bin/env python3
"""Differential oracle for tools/skills_sync_optional.py (w14_skillssync_38160e)."""
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = [Q + "/agent"]
_mod("agent.skill_utils", is_excluded_skill_path=lambda p, root=None: False)
_mod("utils", atomic_write_text=lambda *a, **kw: None)

SRC = Q + "/tools/skills_sync_optional.py"
spec = importlib.util.spec_from_file_location("tools.skills_sync_optional_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class _FakeSS:
    def __init__(self, root):
        self.root = pathlib.Path(root)

    def _skills_dir(self):
        return self.root


def _live_runtime_cache(path, skill_dir, is_dir, py_exists):
    tmp = tempfile.mkdtemp(prefix="w14ss_")
    old = os.getcwd()
    try:
        os.chdir(tmp)
        sd = pathlib.Path(skill_dir)
        sd.mkdir(parents=True, exist_ok=True)
        p = pathlib.Path(path)
        if is_dir:
            p.mkdir(parents=True, exist_ok=True)
        else:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.touch(exist_ok=True)
        if py_exists:
            py = p.with_suffix(".py")
            py.parent.mkdir(parents=True, exist_ok=True)
            py.touch(exist_ok=True)
        return live._is_runtime_cache(p, sd)
    finally:
        os.chdir(old)


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
        if func == "safe_rel_install_path":
            try:
                py = live._safe_rel_install_path(pathlib.PurePosixPath(r.get("path", "")),
                                                 pathlib.PurePosixPath(r.get("base", "")))
            except Exception as exc:
                py = {"error": type(exc).__name__}
        elif func == "hub_lock_entries":
            py = live._hub_lock_entries(r.get("data"))
        elif func == "read_hub_install_paths":
            tmp = tempfile.mkdtemp(prefix="w14sslock_")
            hub = pathlib.Path(tmp) / ".hub"
            hub.mkdir(parents=True, exist_ok=True)
            (hub / "lock.json").write_text(json.dumps(r.get("data")), encoding="utf-8")
            live._ss = lambda: _FakeSS(tmp)
            py = sorted(live._read_hub_install_paths())
        elif func == "is_runtime_cache":
            py = _live_runtime_cache(r.get("path", ""), r.get("skill_dir", ""),
                                     bool(r.get("is_dir", False)),
                                     bool(r.get("py_exists", False)))
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

print(f"ORACLE w14_skillssync_38160e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
