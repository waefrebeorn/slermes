#!/usr/bin/env python3
"""Differential oracle for hermes_platform/resolver/availability.py
(gate w22_availabili_9d8d88).

`version_at_least` / `_version_parts` / `Availability` run live as imported.
`availability()` runs live too; its three host seams are injected:

  facts.os_family / facts.gpu_class  -- the cached host facts (never re-probed).
  AppResolver / Resolution / CheckState -- the locate/inspect probes, replaced
      by the fixture's located definition and per-copy version observations.

Only the probe results are faked; the state machine, the `looked_at` fallback
chain and the "highest readable copy" tie-break all come from live code.
"""
import importlib.util
import json
import sys
import types
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_platform" / "resolver" / "availability.py"
GATE = "w22_availabili_9d8d88"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("availability_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["availability_live"] = live
_spec.loader.exec_module(live)

WANTED = {"vparts", "at_least", "offerable", "as_dict", "evaluate"}

PRESENT = object()
ABSENT = object()


@contextmanager
def patched(obj, name, value):
    original = getattr(obj, name)
    setattr(obj, name, value)
    try:
        yield
    finally:
        setattr(obj, name, original)


class FakeCopy:
    def __init__(self, path, version):
        self.value = path
        self.version = version


class FakeObservation:
    def __init__(self, value, state):
        self.value = value
        self.state = state


class FakeDefinition:
    def __init__(self, raw):
        self.raw = raw

    @property
    def version_kind(self):
        return self.raw.get("version_kind")


class FakeLocated:
    def __init__(self, raw):
        self.raw = raw
        command = raw.get("command")
        self.command = [str(c) for c in command] if isinstance(command, list) else []
        candidates = raw.get("candidates")
        candidates = candidates if isinstance(candidates, list) else []
        self.candidates = [types.SimpleNamespace(value=str(c)) for c in candidates]
        present = raw.get("present")
        present = present if isinstance(present, list) else []
        self.present = [FakeCopy(row.get("path"), row.get("version"))
                        if isinstance(row, dict) else FakeCopy(None, None)
                        for row in present]
        self.kind = raw.get("kind")
        self.found = bool(raw.get("found"))


class FakeResolver:
    def __init__(self, definition):
        self.definition = definition

    def locate(self):
        return FakeLocated(self.definition.raw if isinstance(self.definition, FakeDefinition) else {})

    def inspect(self, bundle):
        copy = bundle[1][0]
        # `.inspect(...)` hands back an Inspection whose `.version` is the
        # Observation of that single copy.
        return types.SimpleNamespace(
            version=FakeObservation(copy.version, PRESENT if copy.version is not None else ABSENT))


class FakeEntry:
    def __init__(self, raw):
        self.raw = raw if isinstance(raw, dict) else {}

    @property
    def requires_app(self):
        return self.raw.get("requires_app")

    @property
    def min_version(self):
        return self.raw.get("min_version")

    @property
    def required_gpu(self):
        return self.raw.get("required_gpu")

    def app_for(self, os_family):
        app = self.raw.get("app")
        return FakeDefinition(app) if isinstance(app, dict) else None


def evaluate(entry, os_family, gpu_class, app):
    raw = dict(entry) if isinstance(entry, dict) else {}
    if app is not None:
        raw["app"] = app
    facts = types.SimpleNamespace(os_family=lambda: os_family, gpu_class=lambda: gpu_class)
    with patched(live, "facts", facts), \
         patched(live, "AppResolver", FakeResolver), \
         patched(live, "Resolution", lambda kind, copies: (kind, tuple(copies))), \
         patched(live, "CheckState", types.SimpleNamespace(PRESENT=PRESENT)):
        return live.availability(FakeEntry(raw)).as_dict()


def expected(r):
    f = r.get("func")
    if f == "vparts":
        version = r.get("version")
        return live._version_parts(version) if isinstance(version, str) else None
    if f == "at_least":
        # A missing/non-string side reads as "not a version", which fails closed
        # in Python exactly like an unparseable one.
        found = r.get("found") if isinstance(r.get("found"), str) else ""
        minimum = r.get("minimum") if isinstance(r.get("minimum"), str) else ""
        return 1 if live.version_at_least(found, minimum) else 0
    if f == "offerable":
        return 1 if r.get("state") in live.OFFERABLE else 0
    if f == "as_dict":
        return live.Availability(r.get("state"), r.get("version"), r.get("path"),
                                 r.get("minimum")).as_dict()
    if f == "evaluate":
        return evaluate(r.get("entry"), r.get("os_family") or "linux",
                        r.get("gpu_class"), r.get("app"))
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
