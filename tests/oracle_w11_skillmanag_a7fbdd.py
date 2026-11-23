#!/usr/bin/env python3
"""Differential oracle for tools/skill_manager_scan.py (gate w11_skillmanag_a7fbdd)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_skillmanag_a7fbdd"
sys.path.insert(0, QUARRY)


class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        def _noop(*a, **k):
            return None
        return _noop


def _seed(name, **attrs):
    mod = _Stub(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


_seed("tools")
_seed("utils")
_seed("tools.skill_manager_guards", _is_background_review=lambda: False)
_seed("tools.skills_guard", _determine_verdict=lambda findings: "clean")

SRC = QUARRY + "/tools/skill_manager_scan.py"
_spec = importlib.util.spec_from_file_location("skill_manager_scan_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skill_manager_scan_live"] = live
_spec.loader.exec_module(live)


class _BytesPath:
    def __init__(self, data):
        self._data = data

    def read_bytes(self):
        if self._data is None:
            raise OSError("missing file")
        return self._data


class _FakeSkillDir:
    """skill_dir / name -> an object whose read_bytes() yields injected bytes."""
    def __init__(self, files):
        self._files = files

    def __truediv__(self, name):
        raw = self._files.get(name)
        return _BytesPath(None if raw is None else raw.encode("utf-8"))


def _finding(obj):
    return types.SimpleNamespace(
        file=obj.get("file", ""),
        line=int(obj.get("line", 0)),
        pattern_id=obj.get("pattern_id", ""),
        severity=obj.get("severity", ""),
        match=obj.get("match", ""),
    )


def live_out(rec):
    if rec.get("func") != "finding_keys":
        return None
    d = _FakeSkillDir(rec.get("files") or {})
    findings = [_finding(x) for x in (rec.get("findings") or [])]
    return [list(k) for k in live._finding_keys(d, findings)]


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") != "finding_keys":
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
