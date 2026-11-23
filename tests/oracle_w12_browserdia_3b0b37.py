#!/usr/bin/env python3
"""Differential oracle for tools/browser_dialog_tool.py (gate w12_browserdia_3b0b37)."""
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
G = "w12_browserdia_3b0b37"
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


for _pkg in ("tools",):
    try:
        __import__(_pkg)
    except Exception:
        _seed(_pkg)


class _Registry:
    def register(self, **kwargs):
        pass


# Bootstrap the quarry import chain triggered at module import time.
_seed("tools.registry", registry=_Registry())
_seed("tools.browser_supervisor", SUPERVISOR_REGISTRY=_Registry())

SRC = QUARRY + "/tools/browser_dialog_tool.py"
_spec = importlib.util.spec_from_file_location("browser_dialog_tool_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["browser_dialog_tool_live"] = live
_spec.loader.exec_module(live)


class _Supervisor:
    def __init__(self, result):
        self.result = result
        self.seen = None

    def respond_to_dialog(self, action, prompt_text, dialog_id):
        self.seen = (action, prompt_text, dialog_id)
        return self.result


class _SupRegistry:
    def __init__(self, rec):
        self.rec = rec
        self.sup = _Supervisor(rec.get("respond") or {})

    def get(self, key):
        return self.sup if self.rec.get("has_supervisor") else None


def live_out(rec):
    if rec.get("func") != "browser_dialog":
        return None
    live.SUPERVISOR_REGISTRY = _SupRegistry(rec)
    result = live.browser_dialog(
        action=rec.get("action", ""),
        prompt_text=rec.get("prompt_text"),
        dialog_id=rec.get("dialog_id"),
        task_id=rec.get("task_id"),
    )
    return json.loads(result)


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
        if rec.get("func") != "browser_dialog":
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
                  f"C={got!r} PY={py!r} input={rec!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
