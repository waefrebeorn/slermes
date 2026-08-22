#!/usr/bin/env python3
"""sta_oracle_skillevaluator_scan.py — Python twin for the C harness
tests/t_port_skillevaluator_scan.c (tools/skillevaluator_scan.py port).

Reads the same fixture (argv[1]) and emits one JSON line per op, mirroring
the C harness exactly so outputs can be diffed. Imports the REAL module.
"""
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent          # slermes root
DEV = REPO.parent                                      # hermes-agent-dev
sys.path.insert(0, str(DEV))

import tools.skillevaluator_scan as ses  # noqa: E402


def finding_from(d):
    return ses.Tier1Finding(
        check=str(d.get("check_name", "")),
        validator="unknown",
        severity=str(d.get("severity", "info")).lower(),
        message=str(d.get("message", "")),
        file=str(d.get("file_path", "")),
        line=int(d.get("line_number") or 0),
        suggestion=str(d.get("suggestion", "")),
    )


def main():
    fixture = sys.argv[1]
    ops = json.load(open(fixture))["ops"]
    for op in ops:
        kind = op.get("kind", "")
        if kind == "is_secrets_class":
            out = op["check"] in ses.SECRETS_CLASS_CHECKS
            print(json.dumps({"out": out}))
        elif kind == "location":
            f = ses.Tier1Finding(check="", validator="", severity="",
                                 message="", file=op["file"], line=op["line"])
            print(json.dumps({"out": f.location()}))
        elif kind == "parse_format":
            report = ses._parse_report(op["report"])
            print(json.dumps({"out": ses.format_tier1_report(report, op.get("limit", 10))}))
        elif kind == "parse_meta":
            report = ses._parse_report(op["report"])
            meta = {
                "available": report.available,
                "passed": report.passed,
                "n_findings": len(report.findings),
                "incomplete": list(report.incomplete_checks),
                "findings": [
                    {"check": f.check, "validator": f.validator,
                     "severity": f.severity, "message": f.message,
                     "file": f.file, "line": f.line,
                     "suggestion": f.suggestion}
                    for f in report.findings
                ],
            }
            print(json.dumps({"out": meta}))
        elif kind == "run_scan":
            # Mirror the C harness's injected hooks with monkeypatching.
            ses.scanner_available = lambda: bool(op.get("avail"))

            class FakeTimeout(Exception):
                pass

            def fake_run(cmd, capture_output=True, text=True, timeout=None, **kw):
                if op.get("timeout_expire"):
                    raise ses.subprocess.TimeoutExpired(cmd, timeout)
                if op.get("launch_fail"):
                    raise OSError(op.get("launch_err", "launch failed"))
                outdir = cmd[cmd.index("-o") + 1]
                for name, val in (op.get("files") or {}).items():
                    p = Path(outdir) / name
                    if isinstance(val, str):
                        p.write_text(val, encoding="utf-8")
                    else:
                        p.write_text(json.dumps(val), encoding="utf-8")
                return None

            real_run = ses.subprocess.run
            ses.subprocess.run = fake_run
            try:
                report = ses.run_tier1_scan(op.get("skill_dir", "."), op.get("timeout", 5))
            except FakeTimeout:
                report = None
            finally:
                ses.subprocess.run = real_run
            meta = {
                "available": report.available,
                "passed": report.passed,
                "error": report.error or "",
                "n_findings": len(report.findings),
                "incomplete": list(report.incomplete_checks),
            }
            print(json.dumps({"out": meta}))
        else:
            print(json.dumps({"out": None}))


if __name__ == "__main__":
    main()
