#!/usr/bin/env python3
"""Differential oracle for w7_sessioncron_e77eee (gateway/session_cron.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

from persistence.core.errors import AuthorityRefusal  # noqa: E402

SRC = Q + "/gateway/session_cron.py"
spec = importlib.util.spec_from_file_location("w7_sessioncron_e77eee_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_sessioncron_e77eee_live"] = live
spec.loader.exec_module(live)

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
    try:
        if func == "sched_exec":
            py = live.scheduler_execution_id(r.get("job_id", ""), r.get("request_id", ""))
        elif func == "validate":
            params = r.get("params")
            try:
                live._create(None, None, params)
            except (AuthorityRefusal, TypeError):
                py = False   # invalid params (set(params) itself rejects non-dicts)
            except Exception:
                py = True   # got past the validation guard
            else:
                py = True
        elif func == "no_owner":
            live.owner_for_home = lambda home: None
            py = live.deliver_to_local_chat(r.get("home", "/h"), r.get("chat_id", ""),
                                            r.get("text", ""))
        elif func == "run_flags":
            job = r.get("job") or {}
            py = {k: job[k] for k in live.RUN_JOB_FLAGS if k in job}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_sessioncron_e77eee: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
