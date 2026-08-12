"""
sta_oracle_cron_monitor.py — oracle for cron/monitor.py pure functions.

Reads the C harness output (one {"in":...,"out":...} JSON line per case) from
stdin, recomputes the expected `out` from the fixture (argv[1]) using the live
Python cron.monitor.* module, and prints MISMATCH lines or a RESULT verdict.

The runner normalizes JSON (sorted keys) before diffing, so we only need to
print the expected out per input. We re-derive `out` from the same `in` the C
side emitted, then print {"in":<same-in>,"out":<py-out>} so the normalized diff
against the C side is a clean field-by-field comparison.

Fixture format (one JSON record per line), same shape as the C harness input:
  {"fn":"hash","in":"<text>"}
  {"fn":"diff","old":"<prev>","new":"<curr>"}
  {"fn":"has","script":"<s>","url":"<u>"}
"""
import os, sys, json

# Resolve the live Python source (DEV_ROOT is the hermes-agent checkout root).
try:
    sys.path.insert(0, os.environ["ORACLE_FIXDIR"].rsplit("/", 1)[0]
                     if os.environ.get("ORACLE_FIXDIR") else os.getcwd())
except Exception:
    pass

# cron.monitor imports cron.jobs at module load; make the python root importable.
DEV = os.environ.get("DEV_ROOT") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if DEV not in sys.path:
    sys.path.insert(0, DEV)

# cron.monitor has heavy optional imports at import time in some envs; guard.
try:
    from cron import monitor as mon
except Exception as e:  # pragma: no cover - import-time fragility
    sys.stderr.write(f"oracle-cron_monitor: import failed: {e}\n")
    sys.exit(2)

fixture = sys.argv[1] if len(sys.argv) > 1 else None
cases = []
if fixture and os.path.exists(fixture):
    with open(fixture, "r", encoding="utf-8", errors="replace") as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln[0] not in "{[":
                continue
            try:
                cases.append(json.loads(ln))
            except Exception:
                continue

def expected(rec):
    fn = rec.get("fn")
    if fn == "hash":
        return mon.hash_monitor_output(rec.get("in") or "")
    if fn == "diff":
        return mon.build_monitor_diff(rec.get("old") or "", rec.get("new") or "")
    if fn == "has":
        job = {"monitor_script": rec.get("script") or "", "monitor_url": rec.get("url") or ""}
        return "true" if mon.job_has_monitor(job) else "false"
    return ""

# Read C harness output from stdin.
c_lines = []
for raw in sys.stdin:
    raw = raw.strip()
    if not raw or raw[0] != "{":
        continue
    try:
        c_lines.append(json.loads(raw))
    except Exception:
        continue

mismatches = 0
seen = 0
# Re-derive expected per case and compare against the C `out`.
exp_by_in = {}
for rec in cases:
    key = json.dumps({"in": rec.get("in", ""), "fn": rec.get("fn")}, sort_keys=True)
    exp_by_in[key] = expected(rec)

# The C harness emits {"in":<canonical>, "out":<c>}; we match by position since
# both sides consume the fixture in the same order.
for i, c in enumerate(c_lines):
    if i >= len(cases):
        break
    rec = cases[i]
    exp = expected(rec)
    got = c.get("out")
    seen += 1
    if got != exp:
        mismatches += 1
        print(f"MISMATCH case={i+1} fn={rec.get('fn')} in={rec.get('in')!r} "
              f"got={got!r} exp={exp!r}")

print(f"cron_monitor oracle: {seen} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)
