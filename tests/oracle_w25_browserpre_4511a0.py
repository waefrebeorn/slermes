#!/usr/bin/env python3
"""Differential oracle for hermes_cli/browser_preview_signins.py
(gate w25_browserpre_4511a0).

Three seams are pinned to the values a fixture supplies, everything else runs
live:

  * ``real_profile_cdp()`` / ``terminate_real_profile_browser()`` are replaced
    with the fixture's ``(cdp_url, err)`` and a no-op, so no browser is started;
  * ``_read_cookies`` either returns the fixture's raw CDP cookie list or raises
    the fixture's read error, so no websocket is opened;
  * ``sys.stdout`` for ``run_export_signins`` is a recorder whose ``isatty`` is
    the fixture's, and ``export_signins`` itself is replaced by the fixture's
    status so the CLI branch is exercised without a browser.

``_portable`` and ``export_signins`` are then compared against the C port,
including ``skipped``.
"""
import io
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_browserpre_4511a0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import browser_preview_signins as live  # noqa: E402
import tools.browser_session_real_profile as real_profile  # noqa: E402

WANTED = {"portable", "export", "run_code", "constants"}


class _Pipe(io.StringIO):
    def __init__(self, tty):
        super().__init__()
        self._tty = bool(tty)

    def isatty(self):
        return self._tty


def portable(req):
    return live._portable(req.get("cookie"))


def export(req):
    real_profile.real_profile_cdp = lambda: (req.get("cdp_url"), req.get("err"))
    real_profile.terminate_real_profile_browser = lambda: None
    read_error = req.get("read_error")
    raw = req.get("raw")
    if read_error is not None:
        def boom(_url):
            raise OSError(read_error)
        live._read_cookies = boom
    else:
        live._read_cookies = lambda _url: list(raw or [])
    return live.export_signins()


def run_code(req):
    result = req.get("result")
    result = result if isinstance(result, dict) else {}
    live.export_signins = lambda: result

    class Args:
        njson = req.get("json", False)
        json = njson

    sink = _Pipe(req.get("isatty", False))
    saved_out, saved_err = sys.stdout, sys.stderr
    sys.stdout = sink
    sys.stderr = io.StringIO()
    try:
        return live.run_export_signins(Args())
    finally:
        sys.stdout, sys.stderr = saved_out, saved_err


def constants(_req):
    return {
        "cdp_timeout_s": live._CDP_TIMEOUT_S,
        "cookie_fields": list(live._COOKIE_FIELDS),
    }


HANDLERS = {
    "portable": portable,
    "export": export,
    "run_code": run_code,
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
