#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_db_context.py (gate w16_kanbandbc_e9b464)."""
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
G = "w16_kanbandbc_e9b464"
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


# Origin module is reached late-bound via _kb; none of the pure helpers use it.
_seed("hermes_cli")
_seed("hermes_cli.kanban_db")

SRC = QUARRY + "/hermes_cli/kanban_db_context.py"
_spec = importlib.util.spec_from_file_location("kanban_db_context_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["kanban_db_context_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "age":
        ts = rec.get("ts", "")
        if ts is None:
            ts = ""
        return live._relative_age(ts, int(rec["now"]))
    if func == "cap":
        return live._ctx_cap(rec.get("text", ""), int(rec.get("limit", 4096)))
    if func == "meta":
        return live._ctx_metadata_line(rec.get("metadata"))
    if func == "tail":
        kept, note = live._ctx_tail(rec.get("items") or [], int(rec.get("cap", 10)),
                                    rec.get("noun", "item"))
        return [kept, note]
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
        if rec.get("func") not in ("age", "cap", "meta", "tail"):
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
