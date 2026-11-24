#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_version_stamp.py (gate w26_configvers_b21168).

The config file is the seam: ``config_exists``, ``read_config_doc`` and the
parse-failure warning are replaced per case with the values a real home would
produce (missing file, malformed YAML, empty document, list root, a mapping
with or without ``_config_version``).  Everything compared comes from the live
module: the stamp coercion, both readers' (stamp, latest) tuples, the
too-new message and the module's constants.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_configvers_b21168"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import config_version_stamp as live  # noqa: E402
import hermes_cli.config as cli_config  # noqa: E402
import hermes_cli.config_backend as config_backend  # noqa: E402
from hermes_cli.config import InvalidUserConfigError  # noqa: E402

WANTED = {"coerce", "read_stamp", "check_version", "newer_message", "constants"}

LATEST = live._coerce_config_version(live.DEFAULT_CONFIG.get("_config_version", 1)) or 1


def _wire(state):
    """Install the config-file seam for one case."""
    cli_config.get_config_path = lambda: Path("C:/w26/config.yaml")
    config_backend.config_exists = lambda path: bool(state.get("exists"))
    cli_config._warn_config_parse_failure = lambda path, exc: None

    def read_doc(path):
        if state.get("parse_error"):
            raise ValueError("mapping values are not allowed here")
        return state.get("doc")

    config_backend.read_config_doc = read_doc
    return bool(state.get("raise"))


def coerce(req):
    return live._coerce_config_version(req.get("value"))


def read_stamp(req):
    strict = _wire(req)
    try:
        return list(live.read_config_version_stamp(raise_on_parse_error=strict))
    except InvalidUserConfigError:
        return {"error": "InvalidUserConfigError"}


def check_version(req):
    strict = _wire(req)
    try:
        return list(live.check_config_version(raise_on_parse_error=strict))
    except InvalidUserConfigError:
        return {"error": "InvalidUserConfigError"}


def newer_message(req):
    return live.newer_stamp_message(int(req["stamp"]))


def constants():
    return {
        "latest_version": LATEST,
        "legacy_fallback_version": 1,
        "version_key": "_config_version",
        "error_class": "InvalidUserConfigError",
    }


def expected(r):
    f = r.get("func")
    if f == "coerce":
        return coerce(r)
    if f == "read_stamp":
        return read_stamp(r)
    if f == "check_version":
        return check_version(r)
    if f == "newer_message":
        return newer_message(r)
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
