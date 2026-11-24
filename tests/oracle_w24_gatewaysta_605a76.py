#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_status_notes.py
(gate w24_gatewaysta_605a76).

The live module is called, with only its seams replaced by values:

  * ``hermes_cli.gateway_runtime.discover_gateway_endpoint`` is monkeypatched to
    return the case's observed verdict (or to raise OSError, the "bad home"
    path), so ``_incompatible_owner_verdict`` runs its real comparison and its
    real f-string -- nothing is dialled;
  * ``hermes_cli.profiles.get_active_profile_name`` / ``profiles_to_serve`` are
    monkeypatched to the case's active profile and two name sets, so
    ``print_standalone_by_config`` runs its real set difference and sorting;
  * both printing functions are run under ``redirect_stdout``: the compared
    value is exactly the text they print (None where they print nothing).

Every patched attribute is restored before the next case.
"""
import contextlib
import io
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_gatewaysta_605a76"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import gateway_status_notes as live  # noqa: E402
from hermes_cli import gateway_runtime as runtime_mod  # noqa: E402
from hermes_cli import profiles as profiles_mod  # noqa: E402

WANTED = {"verdict", "report_lines", "standalone_note"}


class Observed:
    """The discovery verdict the probe would have returned."""

    def __init__(self, state, reason_code):
        self.state = state
        self.reason_code = reason_code


@contextlib.contextmanager
def patched(module, name, value):
    missing = object()
    old = getattr(module, name, missing)
    setattr(module, name, value)
    try:
        yield
    finally:
        if old is missing:
            delattr(module, name)
        else:
            setattr(module, name, old)


def patch_discovery(r):
    raises = bool(r.get("raises"))
    raw = r.get("observed")
    raw = raw if isinstance(raw, dict) else {}

    def fake(home, timeout=None):
        if raises:
            raise OSError("hermes home is not a directory")
        return Observed(raw.get("state"), raw.get("reason_code"))

    # The Hermes home is a seam too: without a patched HOME the real
    # get_hermes_home() raises RuntimeError before discovery is ever reached.
    return _both(
        patched(live, "get_hermes_home", lambda: "/nonexistent/hermes-home"),
        patched(runtime_mod, "discover_gateway_endpoint", fake))


@contextlib.contextmanager
def _both(outer, inner):
    with outer:
        with inner:
            yield


def verdict(r):
    with patch_discovery(r):
        return live._incompatible_owner_verdict()


def report_lines(r):
    pids = r.get("pids")
    with patch_discovery(r):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            printed = live.report_incompatible_owner(pids)
    if not printed:
        return None
    return buf.getvalue().splitlines()


def standalone_note(r):
    active = r.get("active")
    roster = [n for n in (r.get("roster") or []) if isinstance(n, str)]
    served = [n for n in (r.get("served") or []) if isinstance(n, str)]

    def fake_serve(*_args, **kwargs):
        names = roster if kwargs.get("include_standalone") else served
        return [(name, "/nonexistent/" + name) for name in names]

    with patched(profiles_mod, "get_active_profile_name", lambda: active), \
            patched(profiles_mod, "profiles_to_serve", fake_serve):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            live.print_standalone_by_config()
    text = buf.getvalue().splitlines()
    return text[0] if text else None


def expected(r):
    f = r.get("func")
    if f == "verdict":
        return verdict(r)
    if f == "report_lines":
        return report_lines(r)
    if f == "standalone_note":
        return standalone_note(r)
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
