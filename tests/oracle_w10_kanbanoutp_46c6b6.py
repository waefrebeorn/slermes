#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_output.py (gate w10_kanbanoutp_46c6b6)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_kanbanoutp_46c6b6"
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

# kanban_output imports kanban_db + kanban_workflow at module load; `ruamel` is not
# available on this host, so seed minimal faithful stand-ins (icons mirror the real
# DEFAULT_WORKFLOW in hermes_cli/kanban_workflow.py).
_seed("hermes_cli.kanban_db", Task=object, DEFAULT_FAILURE_LIMIT=3,
      VALID_STATUSES=frozenset(), VALID_SORT_ORDERS={}, VALID_INITIAL_STATUSES=frozenset(),
      VALID_BLOCK_KINDS=frozenset())


class _Col:
    def __init__(self, key, icon):
        self.key, self.icon = key, icon


_seed("hermes_cli.kanban_workflow",
      DEFAULT_WORKFLOW=tuple(_Col(k, i) for k, i in (
          ("triage", "\u25c7"), ("todo", "\u25fb"), ("scheduled", "\u23f1"),
          ("ready", "\u25b6"), ("running", "\u25cf"), ("blocked", "\u2298"),
          ("review", "\u25ce"), ("done", "\u2713"))),
      ARCHIVED="archived")

SRC = QUARRY + "/hermes_cli/kanban_output.py"
_spec = importlib.util.spec_from_file_location("kanban_output_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["kanban_output_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "fmt_counts":
        return live._fmt_counts(rec.get("counts", {}), rec.get("empty", ""))
    if func == "status_icon":
        return live._STATUS_ICONS.get(rec.get("status"), "?")
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
        if rec.get("func") not in ("fmt_counts", "status_icon"):
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
