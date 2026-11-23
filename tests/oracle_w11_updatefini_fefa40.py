#!/usr/bin/env python3
"""Differential oracle for gate w11_updatefini_fefa40 (hermes_cli/update_finish.py)."""
import importlib.util, json, os, sys, types, dataclasses
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hp = types.ModuleType("hermes_cli")
    _hp.__path__ = [os.path.join(Q, "hermes_cli")]
    sys.modules["hermes_cli"] = _hp
NAME = "w11_updatefini_fefa40_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "update_finish.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _bag(fields):
    """Stand-in dataclass: fields() sees ``fields``; __init__ records only the
    kwargs _restore_plan actually supplies (no default-valued fields injected)."""
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)
    return type("Bag", (), {"__init__": __init__, "_w11_fields": list(fields)})


def _canon(v):
    if isinstance(v, dict):
        return {k: _canon(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_canon(x) for x in v]
    if type(v).__name__ == "Bag" and hasattr(v, "__dict__"):
        return _canon(vars(v))
    return v


def compute(r):
    f = r.get("func")
    if f == "restore_plan":
        data = r.get("data", None)
        pf = r.get("plan_fields", [])
        rf = r.get("rt_fields", [])
        inv = types.ModuleType("hermes_cli.update_inventory")
        inv.RuntimeRecord = _bag(rf)
        inv.UpdatePlan = _bag(pf)
        sys.modules["hermes_cli.update_inventory"] = inv
        sys.modules["hermes_cli"].update_inventory = inv
        orig_fields = dataclasses.fields
        dataclasses.fields = lambda cls: [types.SimpleNamespace(name=n) for n in cls._w11_fields]
        try:
            res = live._restore_plan(data)
            return _canon(res) if res is not None else None
        finally:
            dataclasses.fields = orig_fields
    if f == "tail_owed":
        owed = [tuple(x) for x in r.get("owed", [])]
        tail = r.get("tail_followups", [])
        return any(step in tail for step, _ in owed)
    if f == "resume_handled":
        attempted = bool(r.get("attempted", False))
        token = r.get("token", None)
        return attempted or not token or not token.get("resume_needed")
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w11_updatefini_fefa40: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
