#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_maint.py (gate w15_updatecmd_d274f9)."""
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

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg


def _stub(name, **attrs):
    m = types.ModuleType(name)
    if "." in name:
        parent = name.rsplit(".", 1)[0]
        if parent not in sys.modules:
            pm = types.ModuleType(parent)
            pm.__path__ = [QUARRY + "/" + parent.replace(".", "/")]
            sys.modules[parent] = pm
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.update_cmd_common", _best_effort=lambda *a, **k: None)


class _FakeM:
    def __init__(self, is_windows):
        self._w = is_windows

    def _is_windows(self):
        return self._w


_uc = _stub("hermes_cli.update_cmd")
_uc._m = lambda: _FakeM(False)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w15_updatecmd_d274f9_live", QUARRY + "/hermes_cli/update_cmd_maint.py")


def compute(r):
    f = r.get("func")
    if f == "sqlite_partial_completion_lines":
        _uc._m = lambda: _FakeM(bool(r.get("is_windows", False)))
        return live._sqlite_partial_completion_lines(r.get("sqlite_version", ""))
    if f == "update_complete_message":
        old = live._checkout_version
        try:
            live._checkout_version = lambda: r.get("post_version")
            return live._update_complete_message(r.get("pre_version"))
        finally:
            live._checkout_version = old
    if f == "profile_skill_sync_status":
        return live._profile_skill_sync_status(r.get("r"))
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
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w15_updatecmd_d274f9: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w15_updatecmd_d274f9 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w15_updatecmd_d274f9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
