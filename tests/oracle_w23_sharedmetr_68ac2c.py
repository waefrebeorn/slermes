#!/usr/bin/env python3
"""Differential oracle for hermes_cli/observability/shared_metrics_prepared_compaction.py
(gate w23_sharedmetr_68ac2c).

Everything runs live: the module is loaded out of the quarry under its real
package name (a synthetic ``hermes_cli.observability`` package, so the
``from .shared_metrics_fields import ...`` inside
``prepared_compaction_fields`` resolves to the real module and the real
``compression_failure_class``).  No seam is stubbed -- these functions are pure
and their only input is the fixture's parsed JSON.

  * ``prepare_ahead_state`` reads ``config["compression"]["prepare_ahead"]``
    through the agent's own ``utils.is_truthy_value``;
  * ``wait_bucket`` is the threshold ladder (bool / non-number / negative ->
    "none");
  * ``prepared_compaction_fields`` is the closed-dimension builder, ``None``
    for an event outside the set.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "observability" / "shared_metrics_prepared_compaction.py"
GATE = "w23_sharedmetr_68ac2c"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# Synthetic package shells: the module's relative import must land on the real
# shared_metrics_fields, without dragging in hermes_cli/__init__.
for name, sub in (("hermes_cli", "hermes_cli"),
                  ("hermes_cli.observability", "hermes_cli/observability")):
    mod = types.ModuleType(name)
    mod.__path__ = [str(QUARRY / sub)]
    sys.modules.setdefault(name, mod)

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.observability.shared_metrics_prepared_compaction", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["hermes_cli.observability.shared_metrics_prepared_compaction"] = live
_spec.loader.exec_module(live)

from utils import is_truthy_value  # noqa: E402  (the live truthiness reader)

WANTED = {"metric_name", "truthy", "prepare_ahead_state", "wait_bucket", "fields"}


def expected(r):
    f = r.get("func")
    if f == "metric_name":
        return live.PREPARED_COMPACTION_METRIC
    if f == "truthy":
        return is_truthy_value(r.get("value"), default=False)
    if f == "prepare_ahead_state":
        return live.prepare_ahead_state(r.get("config"))
    if f == "wait_bucket":
        return live.wait_bucket(r.get("seconds"))
    if f == "fields":
        return live.prepared_compaction_fields(
            event=r.get("event"), origin=r.get("origin"), reason=r.get("reason"),
            failure_class=r.get("failure_class"), wait_seconds=r.get("wait_seconds"),
        )
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
