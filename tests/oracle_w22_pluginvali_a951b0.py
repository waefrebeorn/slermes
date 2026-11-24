#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugin_validate_locales.py
(gate w22_pluginvali_a951b0).

`_check_locale_file` runs live; its seams are injected:

  _load_document  -- returns the fixture document (or raises for the parse-error
                     branch); the YAML read is the host seam.
  reference_keys  -- the bundled English key set, or None (key check skipped).
  report          -- captures `add(label, ok, detail)` / `warn(message)`.

`flatten` / `non_text_leaves` come from the live `agent.i18n_layers` module the
validator imports, and `parse_provides_locales` from the live
`hermes_cli.plugins_manifest` the admission check calls.
"""
import importlib.util
import json
import sys
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "plugin_validate_locales.py"
GATE = "w22_pluginvali_a951b0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("pvl_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["pvl_live"] = live
_spec.loader.exec_module(live)

from hermes_cli.plugins_manifest import parse_provides_locales  # noqa: E402

WANTED = {"label", "flatten", "bad_leaves", "known_ids", "check"}


class FakeReport:
    def __init__(self):
        self.adds = []
        self.warns = []

    def add(self, label, ok, detail):
        self.adds.append((label, bool(ok), detail))

    def warn(self, message):
        self.warns.append(message)


class FakePath:
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return self.name


class ParseFailure(Exception):
    pass


class Documented:
    """Stand-in for the module's `_load_document` seam."""

    def __init__(self, document, failure):
        self.document = document
        self.failure = failure

    def __call__(self, _path):
        if self.failure is not None:
            raise ParseFailure(self.failure)
        return self.document


@contextmanager
def seams(document, reference, failure):
    original_load = live._load_document
    original_ref = live.reference_keys
    live._load_document = Documented(document, failure)
    live.reference_keys = lambda _surface: reference
    try:
        yield
    finally:
        live._load_document = original_load
        live.reference_keys = original_ref


def check(document, lang_id, surface, reference, filename, failure):
    report = FakeReport()
    ref = set(reference) if isinstance(reference, list) else None
    with seams(document, ref, failure):
        live._check_locale_file(report, lang_id, surface, FakePath(filename))
    label, ok, detail = report.adds[0]
    return {"label": label, "ok": 1 if ok else 0, "detail": detail,
            "warnings": list(report.warns)}


def expected(r):
    f = r.get("func")
    if f == "label":
        # Read the label the live file check would use: force the parse branch,
        # which names the report row exactly like the passing branch does.
        report = FakeReport()
        with seams(None, None, "boom"):
            live._check_locale_file(report, r.get("lang_id", ""), r.get("surface", ""),
                                    FakePath("x.yaml"))
        return report.adds[0][0]
    if f == "flatten":
        document = r.get("document")
        if not isinstance(document, dict):
            document = {} if document is None else document
        return sorted(live.flatten(document).keys())
    if f == "bad_leaves":
        document = r.get("document")
        if not isinstance(document, dict):
            document = {} if document is None else document
        return sorted(live.non_text_leaves(document))
    if f == "known_ids":
        return parse_provides_locales(r.get("raw"), "fixture")[0]
    if f == "check":
        return check(r.get("document"), r.get("lang_id", ""), r.get("surface", "core"),
                     r.get("reference"), r.get("filename", "x.yaml"), r.get("parse_error"))
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
