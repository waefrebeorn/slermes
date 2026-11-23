#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_parser.py (gate w10_kanbanpars_0b0fba)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_kanbanpars_0b0fba"
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


try:
    import hermes_cli  # noqa: F401
except Exception:
    _seed("hermes_cli")

# kanban_parser imports kanban_db / dispatch / notify for help strings. `ruamel`
# is unavailable on this host, so seed faithful-enough stand-ins; the tested
# helpers depend on none of their behaviour.
_seed("hermes_cli.kanban_db", DEFAULT_FAILURE_LIMIT=3, DEFAULT_CLAIM_TTL_SECONDS=900,
      VALID_STATUSES=[],
      VALID_INITIAL_STATUSES=[], VALID_SORT_ORDERS={}, VALID_BLOCK_KINDS=[])
_seed("hermes_cli.kanban_db_dispatch", DEFAULT_FAILURE_LIMIT=3)
_seed("hermes_cli.kanban_db_notify", _NOTIFY_DELIVERY_MODES=[])

SRC = QUARRY + "/hermes_cli/kanban_parser.py"
_spec = importlib.util.spec_from_file_location("kanban_parser_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["kanban_parser_live"] = live
_spec.loader.exec_module(live)


def _jsonable(value):
    # Tuples in argparse specs become JSON arrays; dict key order is irrelevant.
    return json.loads(json.dumps(value))


def live_out(rec):
    func = rec.get("func")
    if func == "nonnegative_int":
        try:
            return {"ok": str(live._nonnegative_int(rec.get("value", "")))}
        except Exception as exc:
            return {"err": str(exc)}
    if func == "bulk_ids":
        return _jsonable(live._bulk_ids(rec.get("verb", "")))
    if func == "reason":
        return _jsonable(live._reason(rec.get("help", "")))
    if func == "json_flag":
        return _jsonable(live._json_flag(**rec.get("kw", {})))
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
        if rec.get("func") not in ("nonnegative_int", "bulk_ids", "reason", "json_flag"):
            continue
        cases += 1
        py = live_out(rec)
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
