#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_validation.py
(gate w22_updatecmdv_ce00f8).

The import probe runs in a subprocess; that subprocess is the seam. The live
`_critical_module_import_failures` / `_validate_critical_modules_import` are
called unmodified with `subprocess.run` replaced by a stub that returns the
fixture's stdout and return code (or raises TimeoutExpired / OSError).

The fixture's stdout carries a literal
`__HERMES_IMPORT_HEALTH_<32 lowercase hex>__` marker; the live code mints a
fresh random marker per call, so the stub rewrites the fixture's marker to the
one the live call actually generated before handing the text back. That keeps
both sides reading the same payload after the last marker, and a fixture with
no marker at all reaches the ProbeTerminated path on both sides.
"""
import importlib.util
import json
import re
import subprocess as sp
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "update_cmd_validation.py"
GATE = "w22_updatecmdv_ce00f8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("update_cmd_validation_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["update_cmd_validation_live"] = live
_spec.loader.exec_module(live)

WANTED = {"failures", "validate", "probe_failure", "modules"}

MARKER_RE = re.compile(r"__HERMES_IMPORT_HEALTH_[0-9a-f]{32}__")


def _rewrite(cmd, stdout_text):
    """Give the fixture text the marker this call actually minted."""
    text = " ".join(str(part) for part in cmd) if isinstance(cmd, (list, tuple)) else str(cmd)
    match = MARKER_RE.search(text)
    if not match:
        return stdout_text
    return MARKER_RE.sub(match.group(0), stdout_text or "")


def call(mode, stdout_text, rc, validate=False):
    if mode == "timeout":
        def run(*_a, **_k):
            raise sp.TimeoutExpired("probe", 120)
    elif mode == "oserror":
        def run(*_a, **_k):
            raise OSError("cannot spawn")
    else:
        def run(*args, **_k):
            cmd = args[0] if args else _k.get("args")
            return types.SimpleNamespace(
                stdout=_rewrite(cmd, stdout_text), returncode=int(rc or 0), stderr="")
    fake = types.SimpleNamespace(run=run, TimeoutExpired=sp.TimeoutExpired,
                                 SubprocessError=sp.SubprocessError,
                                 CompletedProcess=sp.CompletedProcess)
    saved = live.subprocess
    live.subprocess = fake
    try:
        if validate:
            ok, module, message = live._validate_critical_modules_import(str(QUARRY))
            return [ok, module, message]
        failures = live._critical_module_import_failures(str(QUARRY))
        return {str(k): [str(v[0]), str(v[1])] for k, v in failures.items()}
    finally:
        live.subprocess = saved


def expected(r):
    f = r.get("func")
    mode = r.get("mode") or "ok"
    if f == "failures":
        return call(mode, r.get("stdout"), r.get("rc"))
    if f == "validate":
        return call(mode, r.get("stdout"), r.get("rc"), validate=True)
    if f == "probe_failure":
        return {k: list(v) for k, v in
                live._probe_failure(r.get("kind"), r.get("detail")).items()}
    if f == "modules":
        return list(live._UPDATE_CRITICAL_MODULES)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
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
