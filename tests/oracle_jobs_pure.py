#!/usr/bin/env python3
"""Oracle for port_jobs_pure.c — diffs the C harness JSONL against LIVE
cron/jobs.py pure helpers.

The LIVE module import is heavy (fcntl/msvcrt probing, get_hermes_home() at
module level, croniter, hermes_cli chain), so the wanted defs are AST-extracted
and the module-level constants they read are exec'd into the namespace first
(pattern: tests/oracle_container_boot.py). No quarry import, no I/O.

Contract notes (harness stays inside the deterministic subset):
- instant helpers take fixed-offset ISO strings (no ZoneInfo names);
- _normalize_enabled_toolsets elements are scalars (str() of containers is
  Python repr, not libjson serialization);
- _seconds_after feeds exactly-representable microsecond values;
- _parse_clock_time/_natural_every_to_cron cases are ASCII (Python \\d/\\w and
  lower() are unicode-aware; the C port covers the ASCII subset).
"""
import ast
import copy
import json
import math
import os
import sys
from datetime import datetime, timedelta, UTC
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

# fromisoformat('Z') needs 3.11+; re-exec once under the quarry venv on older
# interpreters (stdin survives execv).
if sys.version_info < (3, 11) and not os.environ.get("JP_ORACLE_REEXEC"):
    for _cand in (Path.home() / "hermes-agent/.venv/Scripts/python.exe",
                  Path.home() / "hermes-agent/.venv/bin/python"):
        if _cand.is_file() and Path(sys.executable).resolve() != _cand.resolve():
            os.environ["JP_ORACLE_REEXEC"] = "1"
            os.execv(str(_cand), [str(_cand), os.path.abspath(__file__)] + sys.argv[1:])

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "cron" / "jobs.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "cron" / "jobs.py")

# C harness func name -> LIVE python function name.
PY = {
    "cronjobs_pure_classify_dispatch_lateness": "_classify_dispatch_lateness",
    "cronjobs_pure_interval_schedule": "_interval_schedule",
    "cronjobs_pure_is_recoverable_error_job": "_is_recoverable_error_job",
    "cronjobs_pure_parse_clock_time": "_parse_clock_time",
    "cronjobs_pure_natural_every_to_cron": "_natural_every_to_cron",
    "cronjobs_pure_normalize_enabled_toolsets": "_normalize_enabled_toolsets",
    "cronjobs_pure_complete_job_record": "_complete_job_record",
    "cronjobs_pure_activate_job_record": "_activate_job_record",
    "cronjobs_pure_oneshot_past_grace_error": "_oneshot_past_grace_error",
    "cronjobs_pure_reject_terminal_activation": "_reject_terminal_activation",
    "cronjobs_pure_elapsed_seconds": "_elapsed_seconds",
    "cronjobs_pure_instant_after": "_instant_after",
    "cronjobs_pure_instant_at_or_before": "_instant_at_or_before",
    "cronjobs_pure_instant_before": "_instant_before",
    "cronjobs_pure_seconds_after": "_seconds_after",
    "cronjobs_pure_timezone_offset_mismatch": "_timezone_offset_mismatch",
    "cronjobs_pure_stored_wall_clock_is_future": "_stored_wall_clock_is_future",
}
# is_terminal_job is a LIVE dependency of _reject_terminal_activation.
WANTED = set(PY.values()) | {"is_terminal_job"}
CONSTS = {"_WEEKDAY_TO_CRON_DOW", "_DAYSPEC_TO_CRON_DOW",
          "_LATE_DISPATCH_TOLERANCE_SECONDS", "ONESHOT_GRACE_SECONDS"}


def extract():
    import re as _re
    import logging as _logging
    import threading as _threading
    import contextlib as _contextlib
    import uuid as _uuid
    from typing import Optional, Dict, List, Any, Callable, Set, Tuple, Union, Collection
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "__name__": "cron.jobs",
        "re": _re, "os": os, "json": json, "sys": sys, "math": math,
        "datetime": datetime, "timedelta": timedelta, "UTC": UTC,
        "Optional": Optional, "Dict": Dict, "List": List, "Any": Any,
        "Callable": Callable, "Set": Set, "Tuple": Tuple, "Union": Union,
        "Collection": Collection,
        "logging": _logging, "logger": _logging.getLogger("cron.jobs"),
        "threading": _threading, "contextlib": _contextlib, "copy": copy,
        "Path": Path, "uuid": _uuid,
    }
    tree = ast.parse(src)
    # Module-level constants (Assign AND AnnAssign); heavy home/I/O assignments
    # raise NameError and are skipped — none of the wanted defs reads them.
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = (WANTED | CONSTS) - set(ns)
    if missing:
        print("FATAL jobs_pure: extraction missing names:", sorted(missing))
        sys.exit(1)
    return ns


def dt(s):
    return datetime.fromisoformat(s)


def expected(ns, func, inp):
    fn = PY.get(func)
    if fn is None:
        raise KeyError(func)
    f = ns[fn]
    if fn == "_classify_dispatch_lateness":
        return f(inp["lateness"], inp["grace"])
    if fn == "_interval_schedule":
        return f(inp["minutes"])
    if fn == "_is_recoverable_error_job":
        return f(inp["job"])
    if fn == "_parse_clock_time":
        t = f(inp["text"])
        return list(t) if t is not None else None
    if fn == "_natural_every_to_cron":
        return f(inp["rest"])
    if fn == "_normalize_enabled_toolsets":
        try:
            return f(inp.get("value"))
        except ValueError as e:
            return {"error": str(e)}
    if fn in ("_complete_job_record", "_activate_job_record"):
        job = copy.deepcopy(inp["job"])
        f(job)
        return job
    if fn == "_oneshot_past_grace_error":
        return {"error": str(f(inp["run_at"]))}
    if fn == "_reject_terminal_activation":
        try:
            f(inp["job"], inp["updated"], inp["job_id"])
            return None
        except ValueError as e:
            return {"error": str(e)}
    if fn == "_elapsed_seconds":
        return f(dt(inp["later"]), dt(inp["earlier"]))
    if fn in ("_instant_after", "_instant_at_or_before", "_instant_before"):
        return f(dt(inp["left"]), dt(inp["right"]))
    if fn == "_seconds_after":
        return f(dt(inp["dt"]), inp["seconds"]).isoformat()
    if fn in ("_timezone_offset_mismatch", "_stored_wall_clock_is_future"):
        return f(dt(inp["stored"]), dt(inp["current"]))
    raise KeyError(fn)


def main():
    ns = extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        try:
            want = expected(ns, func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out")
        if func == "cronjobs_pure_elapsed_seconds":
            ok = isinstance(got, (int, float)) and math.isclose(got, want, rel_tol=0.0, abs_tol=1e-9)
        else:
            ok = got == want
        if not ok:
            mism += 1
            print(f"MISMATCH {func} in={json.dumps(c.get('in'))} C={got!r} PY={want!r}")
    print(f"ORACLE jobs_pure: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
