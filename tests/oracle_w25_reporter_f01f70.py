#!/usr/bin/env python3
"""Differential oracle for agent/lsp/reporter.py (gate w25_reporter_f01f70).

The live module is imported directly: it has no connection, no server and no
file handle, so the pure formatting law can be exercised verbatim.  Each JSON
line is answered by calling the LIVE function on the same inputs the C harness
read, and the two outputs are compared for exact equality.

  * ``_sanitize_field``      -- CR/LF collapse, control-char drop, code-point
                                cap, HTML escaping;
  * ``format_diagnostic``    -- severity/line/column rendering;
  * ``report_for_file``      -- severity filter, per-file cap, ``and N more``;
  * ``truncate``             -- the non-imitable elision marker;
  * the module constants.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_reporter_f01f70"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.lsp import reporter as live  # noqa: E402

WANTED = {"sanitize", "format_diagnostic", "report_for_file", "truncate", "constants"}


def sanitize(req):
    return live._sanitize_field(req.get("value"), limit=req.get("limit", live.MAX_MESSAGE_CHARS))


def format_diagnostic(req):
    return live.format_diagnostic(req.get("d"))


def report_for_file(req):
    kwargs = {}
    sev = req.get("severities")
    if isinstance(sev, list):
        kwargs["severities"] = frozenset(sev)
    if req.get("max_per_file") is not None:
        kwargs["max_per_file"] = req["max_per_file"]
    diags = req.get("diagnostics")
    return live.report_for_file(req.get("file_path"), diags,
                                **kwargs)


def truncate(req):
    return live.truncate(req.get("s"), limit=req.get("limit", live.MAX_TOTAL_CHARS))


def constants(_req):
    return {
        "severity_names": {str(k): v for k, v in live.SEVERITY_NAMES.items()},
        "default_severities": sorted(live.DEFAULT_SEVERITIES),
        "max_per_file": live.MAX_PER_FILE,
        "max_total_chars": live.MAX_TOTAL_CHARS,
        "max_message_chars": live.MAX_MESSAGE_CHARS,
        "max_code_chars": live.MAX_CODE_CHARS,
        "max_source_chars": live.MAX_SOURCE_CHARS,
    }


HANDLERS = {
    "sanitize": sanitize,
    "format_diagnostic": format_diagnostic,
    "report_for_file": report_for_file,
    "truncate": truncate,
    "constants": constants,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
