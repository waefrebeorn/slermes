#!/usr/bin/env python3
"""Differential oracle for hermes_cli/isolated_scope.py (gate w12_isolatedsc_454cd5)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_isolatedsc_454cd5"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    return mod


class _Home:
    def __init__(self, v):
        self.v = v

    def resolve(self):
        return self.v


KC = _seed("hermes_constants")

SRC = QUARRY + "/hermes_cli/isolated_scope.py"
_spec = importlib.util.spec_from_file_location("isolated_scope_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["isolated_scope_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "filter":
        live._LAUNCH_PROFILE = rec.get("scope")
        items = rec.get("items") or []
        key = rec.get("key")
        if key:
            return list(live.scope_to_launch_profile(items, name_of=lambda it: it.get(key)))
        return list(live.scope_to_launch_profile(items))
    if func == "record":
        live._LAUNCH_PROFILE = None
        KC.get_default_hermes_root = lambda: _Home(rec.get("default_root", ""))
        KC.get_process_hermes_home = lambda: _Home(rec.get("home", ""))
        KC.profile_name_for_home = lambda home: rec.get("profile", "")
        result = live.record_launch_scope(
            isolated=bool(rec.get("isolated", False)),
            desktop_owned=bool(rec.get("desktop_owned", False)))
        # isolated_launch_profile() mirrors the pinned value.
        assert live.isolated_launch_profile() == result
        return result
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
        if rec.get("func") not in ("filter", "record"):
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
