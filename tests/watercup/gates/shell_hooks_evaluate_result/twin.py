"""twin for shell_hooks_evaluate_result.

Traces the LIVE agent/shell_hooks.py::_evaluate_result ladder with the SAME
rung order as the C harness. Each input encodes
``event|fail_closed|error|timed_out|returncode|stdout|stderr``; the traced
outcome label is asserted against the LIVE fn's return on every input.
"""
import dataclasses
import importlib.util as ilu
import json as _json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_shell_hooks", os.path.join(DEV, "agent", "shell_hooks.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_shell_hooks"] = mod
spec.loader.exec_module(mod)

_evaluate_result = mod._evaluate_result
_BLOCKING_EVENTS = mod._BLOCKING_EVENTS

from watercup_py import Cup


def parse_directive(stdout):
    """Mirror of LIVE _parse_response restricted to shared C semantics:
    returns None | "block" | "other"."""
    stdout = (stdout or "").strip()
    if not stdout:
        return None
    try:
        data = _json.loads(stdout)
    except _json.JSONDecodeError:
        return None
    if not isinstance(data, dict):
        return None
    if data.get("action") == "block" or data.get("decision") == "block":
        return "block"
    ctx = data.get("context")
    if isinstance(ctx, str) and ctx.strip():
        return "other"
    return None


def trace_one(raw):
    parts = raw.split("|")
    while len(parts) < 7:
        parts.append("")
    event, fc_s, error, to_s, rc_s, out_s, err_s = parts[:7]
    fail_closed = fc_s == "1"
    timed_out = to_s == "1"
    returncode = int(rc_s or 0)
    spec = mod.ShellHookSpec(event=event, command="hook-cmd", fail_closed=fail_closed)

    c = Cup("evaluate_result")
    blocking_event = event in _BLOCKING_EVENTS
    c.branch(blocking_event)                                   # B1
    fc = fail_closed and blocking_event
    c.branch(fc)                                               # B2

    def finish(outcome):
        c.valstr(outcome)
        return c.finalize()

    if error:                                                   # B3 true
        c.branch(True)
        outcome = "block" if fc else "none"
        r = {"error": error, "timed_out": False, "returncode": 0,
             "stdout": "", "stderr": "", "elapsed_seconds": 0.0}
        got = _evaluate_result(spec, r)
        assert ((got is None) == (outcome == "none")) and (
            got is None or got.get("action") == "block"), raw
        return finish(outcome)
    c.branch(False)

    if timed_out:                                               # B4 true
        c.branch(True)
        outcome = "block" if fc else "none"
        r = {"error": "", "timed_out": True, "returncode": 0,
             "stdout": "", "stderr": "", "elapsed_seconds": 9.9}
        got = _evaluate_result(spec, r)
        assert ((got is None) == (outcome == "none")) and (
            got is None or got.get("action") == "block"), raw
        return finish(outcome)
    c.branch(False)

    stderr = err_s.strip()
    c.branch(bool(stderr))                                      # B5

    diag = {"error": "", "timed_out": False, "returncode": returncode,
            "stdout": out_s, "stderr": err_s, "elapsed_seconds": 0.5}

    if returncode == 2 and blocking_event:                      # B6 true
        c.branch(True)
        parsed = parse_directive(out_s)
        c.branch(parsed is not None)                            # B7
        got = _evaluate_result(spec, diag)
        assert isinstance(got, dict) and got.get("action") == "block", raw
        return finish("block")
    c.branch(False)

    c.branch(returncode != 0)                                   # B8

    parsed = parse_directive(out_s)
    c.branch(parsed is not None)                                # B9

    if parsed is None and fc and out_s:
        c.branch(True)                                          # B10
        try:
            data = _json.loads(out_s)
            valid_json = isinstance(data, dict)
        except _json.JSONDecodeError:
            valid_json = False
        c.branch(valid_json)                                    # B11
        got = _evaluate_result(spec, diag)
        if not valid_json:
            assert isinstance(got, dict) and got.get("action") == "block", raw
            return finish("block")
        assert got is None, raw
        return finish("none")

    c.branch(False)
    got = _evaluate_result(spec, diag)
    if parsed is None:
        assert got is None, raw
        return finish("none")
    if parsed == "block":
        assert isinstance(got, dict) and got.get("action") == "block", raw
        return finish("block")
    assert isinstance(got, dict), raw
    return finish("other")


def run(inputs):
    return [trace_one(x) for x in inputs]


if __name__ == "__main__":
    inputs = __import__("json").load(open(os.path.join(HERE, "gate.json")))["inputs"]
    print("\n".join(run(inputs)))
