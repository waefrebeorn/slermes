"""twin for doctor_extract_health_report.

Traces LIVE tools/computer_use/doctor.py::_extract_health_report_from_result
with the same rung order as the C harness:
  B1 isError / V1 denial text / B2 report extracted / V2 report JSON or reason.
"""
import json
import os
import sys
import io
import contextlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

import importlib.util as ilu

with contextlib.redirect_stderr(io.StringIO()):
    spec = ilu.spec_from_file_location(
        "wc_live_cua_doctor",
        os.path.join(DEV, "tools", "computer_use", "doctor.py"))
_doc = ilu.module_from_spec(spec)
sys.modules["wc_live_cua_doctor"] = _doc
spec.loader.exec_module(_doc)

_extract = _doc._extract_health_report_from_result
_Unavailable = getattr(_doc, "HealthReportUnavailable", ())


def trace(result_json):
    c = Cup("extract_health_report")
    try:
        result = json.loads(result_json)
    except ValueError:
        result = None
    if not isinstance(result, dict):
        c.branch(False)          # not an object -> protocol error path
        c.valstr("")
        c.branch(False)
        c.valstr("")
        return c.finalize()

    is_error = result.get("isError") is True
    c.branch(is_error)           # B1
    denial = ""
    if is_error:
        denial = "health_report returned isError=true"
        for item in result.get("content") or []:
            if isinstance(item, dict) and item.get("type") == "text":
                text = (item.get("text") or "").strip()
                if text:
                    denial = text
                    break
    else:
        denial = ""                  # non-error paths carry no V1 text
    c.valstr(denial)             # V1
    try:
        report = _extract(result)
        c.branch(True)           # B2
        val = json.dumps(report, separators=(",", ":"))
    except Exception:
        c.branch(False)
        val = ""
    c.valstr(val)               # V2
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
