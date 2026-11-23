#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_stage.py (gate w9_updatestage_53df9d).

Environment, marker file and temp directory are injected from the fixture;
the LIVE decision functions (_ui_present_in_log, status_file,
_status_from_marker) are called directly.
"""
import importlib.util
import json
import os
import sys
import tempfile
import types
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w9_updatestage_53df9d_live", QUARRY + "/hermes_cli/update_stage.py")


def compute(r):
    f = r.get("func")
    if f == "ui_present_in_log":
        return live._ui_present_in_log(r.get("log_text", ""))
    if f == "status_file":
        marker = r.get("marker_result", "")
        old = live._status_from_marker
        live._status_from_marker = lambda: Path(marker) if marker else None
        key = live.STATUS_FILE_ENV
        had = key in os.environ
        prev = os.environ.get(key)
        os.environ[key] = r.get("exported", "")
        try:
            res = live.status_file()
        finally:
            if had:
                os.environ[key] = prev
            else:
                os.environ.pop(key, None)
            live._status_from_marker = old
        return str(res) if res is not None else None
    if f == "status_from_marker":
        text = r.get("marker_text", "")
        home = Path(tempfile.mkdtemp(prefix="noc_w9_us_home_"))
        (home / live.MARKER_NAME).write_text(text, encoding="utf-8-sig")
        tmp = tempfile.mkdtemp(prefix="noc_w9_us_tmp_")
        # Set up the candidate file the LIVE function will discover.  Parsing
        # the pid here is fixture setup only: the assertion still compares the
        # C port's derived basename against the real function's answer.
        try:
            pid = int(text.splitlines()[0].strip())
        except Exception:
            pid = None
        if pid is not None and r.get("candidate_exists", False):
            (Path(tmp) / ("hermes-update-status.%d" % pid)).write_text("x", encoding="utf-8")
        old_home, old_tf = live._process_home, live.tempfile
        live._process_home = lambda: home
        live.tempfile = types.SimpleNamespace(gettempdir=lambda: tmp)
        try:
            res = live._status_from_marker()
        finally:
            live._process_home, live.tempfile = old_home, old_tf
        return res.name if res is not None else None
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w9_updatestage_53df9d: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w9_updatestage_53df9d %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_updatestage_53df9d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
