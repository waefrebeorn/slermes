#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_db_connect.py (gate w16_kanbandbc_db6090)."""
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
G = "w16_kanbandbc_db6090"
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


# Top-level dependencies; none is used by the three pure helpers under test.
_seed("agent")
_seed("agent.deadline", poll_until=lambda *a, **k: False)
_seed("hermes_cli")
# kanban_db_connect ends with `from hermes_cli import kanban_db as _kb`.
_seed("hermes_cli.kanban_db", _IS_WINDOWS=False,
      _env_int=lambda *a, **k: 120000)
_seed("hermes_cli.sqlite_util", add_column_if_missing=lambda *a, **k: None)

SRC = QUARRY + "/hermes_cli/kanban_db_connect.py"
_spec = importlib.util.spec_from_file_location("kanban_db_connect_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["kanban_db_connect_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "tls":
        return live._looks_like_tls_record_at(bytes(rec.get("data") or []),
                                              int(rec.get("offset", 0)))
    if func == "ok":
        return live._integrity_messages_ok(rec.get("messages") or [])
    if func == "names":
        return live._repairable_index_names(rec.get("messages") or [])
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
        if rec.get("func") not in ("tls", "ok", "names"):
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
