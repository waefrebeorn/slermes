#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_session_mixin.py (gate w16_clisession_f92c0b)."""
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
G = "w16_clisession_f92c0b"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
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


# The mixin imports several CLI-only modules at top level; none is used by the
# pure helpers under test, so seed dependency-free stand-ins.
_seed("agent")
_seed("agent.i18n", t=lambda *a, **k: "")
_seed("hermes_constants", get_hermes_home=lambda: "/tmp/hermes")
_seed("session_rules")
_seed("session_rules.session_ids", new_session_id=lambda: "s")
_seed("rich")
_seed("rich.console", Console=object)
_seed("rich.markup", escape=lambda s: s)

SRC = QUARRY + "/hermes_cli/cli_session_mixin.py"
_spec = importlib.util.spec_from_file_location("cli_session_mixin_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cli_session_mixin_live"] = live
_spec.loader.exec_module(live)


class _FakeDB:
    def __init__(self, tip):
        self.tip = tip

    def get_compression_tip(self, session_id):
        return self.tip


class _FakeSelf:
    pass


def live_out(rec):
    func = rec.get("func")
    if func == "squash":
        return live._squash(rec.get("text", ""), int(rec.get("limit", 120)))
    if func == "undo":
        return live.CLISessionMixin._undo_content_to_text(rec.get("content"))
    if func == "title":
        obj = _FakeSelf()
        obj.session_id = rec.get("session_id", "")
        obj._session_db = None if not rec.get("db", True) else _FakeDB(rec.get("tip"))
        return live.CLISessionMixin._title_session_id(obj)
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
        if rec.get("func") not in ("squash", "undo", "title"):
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
