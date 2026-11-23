#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/update_cmd_common.py (updcommon gate)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/update_cmd_common.py"
spec = importlib.util.spec_from_file_location("updcommon_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["updcommon_live"] = live
spec.loader.exec_module(live)

RKEY = "hermes_cli.update_receipt"
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "record":
        calls = []
        had = RKEY in sys.modules
        prev = sys.modules.get(RKEY)
        if r.get("has_receipt"):
            mod = types.ModuleType(RKEY)
            def mk(name):
                def f(*a):
                    calls.append({"method": name, "args": list(a)})
                return f
            if r.get("has_record_reason"):
                mod.record_stop_reason = mk("record_stop_reason")
            if r.get("has_record_without"):
                mod.record_stop_without_receipt = mk("record_stop_without_receipt")
            sys.modules[RKEY] = mod
        else:
            sys.modules.pop(RKEY, None)
        try:
            live._record_stop(r.get("reason", ""),
                              without_receipt=r.get("without_receipt"))
        finally:
            if had:
                sys.modules[RKEY] = prev
            else:
                sys.modules.pop(RKEY, None)
        py = calls
    elif func == "best":
        calls = []
        orig = live.logger.debug
        def cap(*a, **k):
            calls.append([a[0], str(a[1]) if len(a) > 1 else None])
        live.logger.debug = cap
        try:
            with live._best_effort(r.get("message", "")):
                if r.get("raises"):
                    raise RuntimeError(r.get("exc", ""))
        finally:
            live.logger.debug = orig
        py = calls
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE updcommon: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
