#!/usr/bin/env python3
"""Oracle for gate w18_processreg_e1509d (tools/process_registry_access.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m


live = _load("tools.process_registry_access", "tools/process_registry_access.py")


def install_stubs(r):
    owns = {(a, b) for a, b in (r.get("owns") or [])}

    def _owns_result(a, b):
        return (a, b) in owns

    mod = types.ModuleType("tools.process_registry_results")
    mod._owns_result = _owns_result
    sys.modules["tools.process_registry_results"] = mod

    # caller_session_key() seam + process_registry.get() seam
    live.caller_session_key = lambda: r.get("caller_key", "") or ""

    class _Reg:
        @staticmethod
        def get(sid):
            if not r.get("exists", True):
                return None
            s = types.SimpleNamespace(
                owner_task_id=r.get("session", {}).get("owner_task_id"),
                task_id=r.get("session", {}).get("task_id"),
                session_key=r.get("session", {}).get("session_key"))
            s.id = r.get("full_id", sid)
            return s

    regmod = types.ModuleType("tools.process_registry")
    regmod.process_registry = _Reg()
    sys.modules["tools.process_registry"] = regmod
    if "tools" in sys.modules:
        setattr(sys.modules["tools"], "process_registry", regmod)


def make_session(d):
    return types.SimpleNamespace(
        owner_task_id=d.get("owner_task_id"),
        task_id=d.get("task_id"),
        session_key=d.get("session_key"))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    install_stubs(r)
    try:
        if func == "session_action_allowed":
            s = make_session(r.get("session") or {})
            py = live.session_action_allowed(s, r.get("caller_task", ""), r.get("caller_session", ""))
        elif func == "resolve_actionable":
            py = list(live.resolve_actionable(r.get("session_id", ""), r.get("task_id", "")))
        else:
            continue
    except Exception as exc:
        print(f"SKIP {func}: {exc}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_processreg_e1509d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
