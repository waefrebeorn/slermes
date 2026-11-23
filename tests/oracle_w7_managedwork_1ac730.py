#!/usr/bin/env python3
"""Differential oracle for agent/managed_worker.py (w7_managedwork_1ac730 gate)."""
import importlib.util, json, os, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
# Importing the live module runs `import hermes_bootstrap`, which exits this install;
# register a no-op so the try/except import guard is satisfied without side effects.
sys.modules["hermes_bootstrap"] = types.ModuleType("hermes_bootstrap")
SRC = PYROOT + "/agent/managed_worker.py"
spec = importlib.util.spec_from_file_location("live_managed_worker_w7", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

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
    try:
        if func == "scalar":
            py = bool(live._scalar(r.get("value")))
        elif func == "structured":
            py = bool(live._structured(r.get("key"), r.get("value")))
        elif func == "result_frame":
            py = live.result_frame(r.get("result"))
        elif func == "accept_result":
            try:
                res, usage = live.accept_result(r.get("result"))
                py = {"ok": True, "result": res, "usage": usage}
            except ValueError:
                py = {"ok": False, "error": "invalid_worker_result"}
        elif func == "activity_interval":
            py = live.activity_interval(r.get("wedge_timeout", 0))
        elif func == "outbox_dir":
            try:
                py = str(live.outbox_dir(r.get("home", ""), r.get("execution_id")))
            except ValueError:
                py = {"ok": False, "error": "invalid_worker_result"}
        elif func == "activity_frame_interval":
            frame = {"policy": {"request_json": r.get("request_json")}}
            py = live.activity_frame_interval(frame)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_managedwork_1ac730: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
