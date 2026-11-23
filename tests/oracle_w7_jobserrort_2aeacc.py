#!/usr/bin/env python3
"""Differential oracle for cron/jobs_error_text.py (gate w7_jobserrort_2aeacc).

The live module is loaded via importlib.util.spec_from_file_location with the
agent.redact collaborator injected as a deterministic stand-in (SECRET ->
[REDACTED], raise on RAISE_REDACTOR). The live sanitize_persisted_error and
scrub_retained_stamps bodies are what actually run for every case.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg


def redact_sensitive_text(text, *, force=False, redact_url_credentials=False, **kw):
    if "RAISE_REDACTOR" in text:
        raise RuntimeError("redactor down")
    return text.replace("SECRET", "[REDACTED]")


redact_mod = types.ModuleType("agent.redact")
redact_mod.redact_sensitive_text = redact_sensitive_text
sys.modules["agent.redact"] = redact_mod

SRC = QUARRY + "/cron/jobs_error_text.py"
spec = importlib.util.spec_from_file_location("w7_jobserrort_live", SRC)
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
    if func not in ("sanitize", "scrub"):
        continue
    cases += 1
    try:
        if func == "sanitize":
            py = live.sanitize_persisted_error(r.get("error"))
        else:
            job = json.loads(json.dumps(r.get("job") or {}))
            live.scrub_retained_stamps(job)
            py = job
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_jobserrort_2aeacc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
