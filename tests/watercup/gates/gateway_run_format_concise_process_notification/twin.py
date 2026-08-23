"""twin for gateway_run_format_concise_process_notification.

Imports LIVE gateway/run.py::_format_concise_process_notification and traces
the SAME rung ladder as the C harness (which mirrors run_gaps2.c). Inputs are
"exit_code|duration|command|output" ("-" = None for exit/duration).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_gwrun",
                                   os.path.join(DEV, "gateway", "run.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_gwrun"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _shorten(command, limit=80):
    one_line = " ".join((command or "").split())
    if len(one_line) > limit:
        one_line = one_line[: limit - 1] + "…"
    return one_line


def _build(ok, exit_code, command, duration_seconds, output):
    """Traced replica of the C harness ladder; returns (cup, text)."""
    c = Cup("gw_format_concise_process_notification")
    c.branch(ok)
    icon = "✅" if ok else "❌"
    verb = "finished" if ok else "failed (exit %s)" % exit_code
    parts_text = "%s Background task %s" % (icon, verb)
    short_cmd = _shorten(command)
    if short_cmd:
        c.branch(True)
        parts_text += " — `%s`" % short_cmd
    else:
        c.branch(False)
    if duration_seconds is not None and duration_seconds >= 0:
        c.branch(True)
        secs = int(duration_seconds)
        if secs >= 3600:
            c.branch(True)
            dur = "%dh %dm" % (secs // 3600, (secs % 3600) // 60)
        elif secs >= 60:
            c.branch(False)
            c.branch(True)
            dur = "%dm %ds" % (secs // 60, secs % 60)
        else:
            c.branch(False)
            c.branch(False)
            dur = "%ds" % secs
        parts_text += " (%s)" % dur
    else:
        c.branch(False)
    if not ok and output and output.strip():
        c.branch(True)
        tail_lines = [ln for ln in output.strip().splitlines() if ln.strip()][-5:]
        tail = "\n".join(tail_lines)
        if len(tail) > 500:
            tail = tail[-500:]
        if tail:
            c.branch(True)
            parts_text += "\n```\n%s\n```" % tail
        else:
            c.branch(False)
    else:
        c.branch(False)
    return c, parts_text


def run(inputs):
    cups = []
    fn = mod._format_concise_process_notification
    for raw in inputs:
        ec_s, dur_s, command, output = raw.split("|", 3)
        exit_code = None if ec_s == "-" else int(ec_s)
        duration = None if dur_s == "-" else float(dur_s)
        ok = exit_code is None or exit_code == 0

        c, text = _build(ok, exit_code, command, duration, output)

        live = fn("sess", command, exit_code, output, duration)
        assert live == text, (raw, repr(live), repr(text))
        c.valstr(text)
        cups.append(c.finalize())
    return cups
