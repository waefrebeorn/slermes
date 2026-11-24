#!/usr/bin/env python3
"""Differential oracle for agent/agent_init_compression_notice.py
(gate w26_agentinitc_084bfa).

`_build_codex_gpt5_autoraise_notice` and `_codex_gpt55_autoraise_notice_state`
are called live.  `_emit_compression_summary` is called with a fake agent whose
compressor carries the threshold numbers the caller passes in; its stdout is
captured, so the compared value is the banner text plus the notice it stashes
for the first turn.  The once-per-profile marker is a file seam: each case
writes (or removes) the marker under a scratch HERMES_HOME to match its
declared ``seen`` flag.
"""
import contextlib
import io
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_agentinitc_084bfa"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# Per-profile marker file seam: keep it in a scratch home, never the real profile.
_SCRATCH = tempfile.mkdtemp(prefix="w26_agentinitc_")
os.environ["HERMES_HOME"] = _SCRATCH

from agent import agent_init_compression_notice as live  # noqa: E402

WANTED = {"notice", "state", "emit", "constants"}


def notice(req):
    return live._build_codex_gpt5_autoraise_notice(req.get("autoraise") or {},
                                                   req.get("context_length"))


def state(req):
    return live._codex_gpt55_autoraise_notice_state(req.get("autoraise") or {})


class _CS:
    def __init__(self, req):
        self.enabled = bool(req.get("enabled"))
        self.autoraise_notice_enabled = bool(req.get("notice_enabled"))
        self.threshold = req.get("threshold")


class _CC:
    """The active engine's own threshold numbers (a seam in the C port)."""

    def __init__(self, req):
        self.context_length = req.get("context_length")
        self.threshold_percent = req.get("threshold_percent")
        self.threshold_tokens = req.get("threshold_tokens")
        self.threshold_tokens_cap = req.get("tokens_cap")
        eff = req.get("eff_cap")
        self._effective_threshold_cap = lambda _ctx, _e=eff: _e


class _Agent:
    def __init__(self, req):
        self.quiet_mode = bool(req.get("quiet_mode"))
        self._compression_threshold_autoraised = req.get("autoraise") or None
        self.context_compressor = _CC(req)
        self.printed = []
        self._compression_warning = None
        self._compression_feasibility_checked = True

    def _safe_print(self, text, diagnostic=False):
        self.printed.append(text)


def _prepare_marker(req):
    marker = live._codex_gpt55_autoraise_notice_marker()
    if req.get("seen"):
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.write_text(live._codex_gpt55_autoraise_notice_state(req.get("autoraise") or {}),
                          encoding="utf-8")
    else:
        with contextlib.suppress(OSError):
            marker.unlink()


def emit(req):
    _prepare_marker(req)
    agent = _Agent(req)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        live._emit_compression_summary(agent, _CS(req))
    line = buf.getvalue().rstrip("\n")
    return {"warning": agent._compression_warning, "line": line or None}


def constants():
    return {
        "default_model": "gpt-5.4/5.5",
        "spark_model": "gpt-5.3-codex-spark",
        "spark_cap": "128K",
        "default_cap": "272K",
        "optout": "  Opt back out: hermes config set compression.codex_gpt55_autoraise false",
        "marker_name": live._codex_gpt55_autoraise_notice_marker().name,
        "config_key": "compression.codex_gpt55_autoraise",
    }


def expected(r):
    f = r.get("func")
    if f == "notice":
        return notice(r)
    if f == "state":
        return state(r)
    if f == "emit":
        return emit(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
