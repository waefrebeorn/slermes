#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/scheduler_diagnostics.py (scheddiag gate)."""
import importlib.util, json, os, pathlib, sys, tempfile, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/cron/scheduler_diagnostics.py"
spec = importlib.util.spec_from_file_location("scheddiag_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

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
    if func == "tail":
        text = r.get("text")
        if not isinstance(text, str):
            continue
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False,
                                         encoding="utf-8") as fh:
            fh.write(text)
            path = fh.name
        try:
            py = live.external_worker_stderr_tail(pathlib.Path(path))
        finally:
            os.unlink(path)
    elif func == "fmt":
        tb = r.get("traceback")
        if not isinstance(tb, str):
            continue
        # Feed a deterministic format_exception result into the LIVE wrapper.
        live.format_exception = lambda exc, tb=tb: [tb]
        py = live.format_run_error(None)
    elif func == "const":
        py = live._WORKER_STDERR_TAIL_CHARS
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE scheddiag: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
