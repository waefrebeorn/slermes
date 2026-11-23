#!/usr/bin/env python3
"""Differential oracle for hermes_cli/connectors_cmd.py (gate w12_connectors_5682b3)."""
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
G = "w12_connectors_5682b3"
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


# _valid_alias late-imports ALIAS_PATTERN from tools.connectors.targets.
# Mirror the real constant (tools/connectors/targets.py: ALIAS_PATTERN).
_seed("tools")
_seed("tools.connectors")
_seed("tools.connectors.targets", ALIAS_PATTERN=r"^[a-z0-9][a-z0-9-]{0,31}$")

SRC = QUARRY + "/hermes_cli/connectors_cmd.py"
_spec = importlib.util.spec_from_file_location("connectors_cmd_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["connectors_cmd_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "aname":
        return live._account_name(rec.get("row") or {})
    if func == "dname":
        return live._display_name(rec.get("row") or {})
    if func == "hint":
        return live._names_hint(rec.get("app", ""), rec.get("rows") or [])
    if func == "valias":
        try:
            return {"ok": True, "alias": live._valid_alias(rec.get("alias", ""))}
        except live._CommandError as exc:
            return {"ok": False, "error": str(exc)}
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
        if rec.get("func") not in ("aname", "dname", "hint", "valias"):
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
