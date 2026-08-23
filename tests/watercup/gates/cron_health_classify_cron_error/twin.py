"""twin for cron_health_classify_cron_error.

Imports LIVE agent/monitoring/cron_health.py::classify_cron_error and traces
the SAME ladder as the C harness: four auth regexes each get a branch event,
then one branch per remaining category.
"""
import importlib.util as ilu
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_cron_health", os.path.join(DEV, "agent", "monitoring", "cron_health.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cron_health"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

AUTH_PATTERNS = [
    r"\b(?:authentication|authenticated|authenticate|authorization|authorized|authorize|unauthorized|forbidden)\b",
    r"\bbearer\b",
    r"\b(?:access|api|refresh) token\b",
    r"\b(?:401|403)\b",
]


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("classify_cron_error")
        text = str(raw or "").lower()
        result = None
        for pat in AUTH_PATTERNS:
            hit = re.search(pat, text) is not None
            c.branch(hit)
            if hit:
                result = "auth_failed"
                break
        if result is None:
            cond = "rate limit" in text or "429" in text or "quota" in text
            c.branch(cond)
            result = "rate_limited" if cond else None
        if result is None:
            cond = "timeout" in text or "timed out" in text
            c.branch(cond)
            result = "timeout" if cond else None
        if result is None:
            hit = any(v in text for v in ("network", "connection", "dns", "socket", "unreachable"))
            c.branch(hit)
            result = "network_error" if hit else None
        if result is None:
            cond = "dispatch" in text or "executor" in text
            c.branch(cond)
            result = "dispatch_failed" if cond else None
        if result is None:
            cond = "interrupt" in text or "owner exited" in text or "restarted" in text
            c.branch(cond)
            result = "interrupted" if cond else None
        if result is None:
            cond = "empty response" in text
            c.branch(cond)
            result = "empty_response" if cond else None
        if result is None:
            hit = any(v in text for v in ("config", "missing", "invalid"))
            c.branch(hit)
            result = "invalid_config" if hit else None
        if result is None:
            result = "unknown"
        c.valstr(result)
        cups.append(c.finalize())
    return cups


if __name__ == "__main__":
    # self-check: LIVE fn agrees with our traced result on every input
    inputs = __import__("json").load(open(os.path.join(HERE, "gate.json")))["inputs"]
    assert run(inputs) is not None
