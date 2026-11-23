#!/usr/bin/env python3
"""Differential oracle for tools/skills_hub_install.py (gate w11_skillshub_e6cf0a)."""
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
G = "w11_skillshub_e6cf0a"
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
_seed("agent")
_seed("agent.skill_utils", is_excluded_skill_path=lambda p, root=None: False)
_seed("tools.skills_guard", ScanResult=dict, content_hash=lambda p: "")
_seed("tools.skills_hub_github", GitHubAuth=dict)
_seed("tools.skills_hub_models")

SRC = QUARRY + "/tools/skills_hub_install.py"
_spec = importlib.util.spec_from_file_location("skills_hub_install_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skills_hub_install_live"] = live
_spec.loader.exec_module(live)


class _Bundle:
    def __init__(self, files):
        self.files = files


class _Source:
    def __init__(self, sid):
        self._sid = sid

    def source_id(self):
        return self._sid


def live_out(rec):
    func = rec.get("func")
    if func == "bundle_content_hash":
        return live.bundle_content_hash(_Bundle(rec.get("files") or {}))
    if func == "source_matches":
        return live._source_matches(_Source(rec.get("source_id") or ""),
                                    rec.get("source_name") or "")
    return None


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
        if rec.get("func") not in ("bundle_content_hash", "source_matches"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
