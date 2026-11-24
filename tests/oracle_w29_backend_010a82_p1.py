#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/backend.py (gate p1).

The live module is imported by its package path (its relative imports need the
package), and only the pure decision helpers are called.  Seams: the managed
file existence check is replaced by the fixture's path, `_profile_for(home)` by
the fixture's profile, `DEFAULT_CONFIG` by the fixture's document, and no plane
call is ever made.
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_backend_010a82_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import backend as live, client  # noqa: E402
import hermes_cli.config_defaults as config_defaults  # noqa: E402

WANTED = {
    "constants", "poll_interval", "latest_config_version", "effective_ok",
    "profile_writer", "value_refusal", "write_error", "note_level",
    "capabilities", "doctor_warnings", "destination",
}


def bare():
    return live.RemoteBackend.__new__(live.RemoteBackend)


def constants():
    return {
        "poll_seconds": float(live.POLL_SECONDS),
        "min_poll_seconds": float(live.MIN_POLL_SECONDS),
        "boot_retry_delays": [float(v) for v in live.BOOT_RETRY_DELAYS],
        "max_retry_wait": float(live.MAX_RETRY_WAIT),
        "max_prepare": float(live.RemoteBackend._MAX_PREPARE),
        "max_postprocess": float(live.RemoteBackend._MAX_POSTPROCESS),
        "value_refusals": [[s, e] for s, e in sorted(live._VALUE_REFUSALS)],
    }


def latest_config_version(req):
    cfg = req.get("default_config")
    old = config_defaults.DEFAULT_CONFIG
    config_defaults.DEFAULT_CONFIG = cfg if isinstance(cfg, dict) else {}
    try:
        return live._latest_config_version()
    finally:
        config_defaults.DEFAULT_CONFIG = old


def profile_writer(req):
    w = live._profile_writer(req.get("body") or {})
    if w is live._UNKNOWN:
        return {"known": False, "value": None}
    return {"known": True, "value": w}


def write_error(req):
    resp = client.Response(status=int(req.get("status", 0)), body=req.get("body") or {},
                           etag=None, retry_after=None)
    try:
        exc = live.RemoteBackend._write_error(resp)
    except Exception as error:  # noqa: BLE001
        exc = error
    return {
        "error": type(exc).__name__,
        "code": getattr(exc, "code", ""),
        "message": str(exc),
        "path": getattr(exc, "path", None),
        "locked_by": getattr(exc, "locked_by", None),
    }


def note_level(req):
    note = req.get("note")
    note = note if isinstance(note, str) else str(note)
    if note.startswith("debug:"):
        return {"level": "debug", "message": note[len("debug:"):]}
    return {"level": "warning", "message": note}


def capabilities():
    b = bare()
    return {
        "supports_file_tooling": b.supports_file_tooling(),
        "honors_managed_config": b.honors_managed_config(),
        "protected_env_names": sorted(b.protected_env_names()),
        "deployment_env_names": sorted(b.deployment_env_names()),
        "bootstrap_env_names": sorted(b.bootstrap_env_names()),
    }


def doctor_warnings(req):
    managed = req.get("managed")
    old = live.managed_config_file
    live.managed_config_file = (lambda: Path(managed)) if managed else (lambda: None)
    try:
        out = bare().doctor_warnings(None)
    finally:
        live.managed_config_file = old
    return [list(t) for t in out]


def destination(req):
    profile = req.get("profile")

    class Fake(live.RemoteBackend):
        @staticmethod
        def _profile_for(home):
            return profile

    return Fake().destination(None)


def expected(r):
    f = r.get("func")
    if f == "constants":
        return constants()
    if f == "poll_interval":
        return live.poll_interval()
    if f == "latest_config_version":
        return latest_config_version(r)
    if f == "effective_ok":
        return live._effective_ok(r.get("body"))
    if f == "profile_writer":
        return profile_writer(r)
    if f == "value_refusal":
        return (int(r.get("status", 0)), r.get("error")) in live._VALUE_REFUSALS
    if f == "write_error":
        return write_error(r)
    if f == "note_level":
        return note_level(r)
    if f == "capabilities":
        return capabilities()
    if f == "doctor_warnings":
        return doctor_warnings(r)
    if f == "destination":
        return destination(r)
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
    if cases == 0:
        print("ORACLE %s: no cases" % GATE, file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
