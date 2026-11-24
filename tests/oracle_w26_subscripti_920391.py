#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/subscriptions.py
(gate w26_subscripti_920391).

The TeamsPipelineStore and the MicrosoftGraphClient are the seams.  The store is
replaced by a stand-in whose ``get_subscription`` answers from the fixture's
``known_ids`` list (nothing else of it is touched), so ``is_managed_subscription``
runs its real decision.  ``utc_timestamp`` and ``expected_client_state`` are
already pure once the clock and the environment are fixed inputs.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_subscripti_920391"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The plugin package's import chain resolves the Hermes home at import time;
# the repo's parent is the home the oracle runs under.
import os  # noqa: E402
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from plugins.teams_pipeline import subscriptions as live  # noqa: E402

WANTED = {"expected_client_state", "utc_timestamp", "is_managed_subscription"}


class FakeStore:
    def __init__(self, known_ids):
        self._known = {str(x) for x in (known_ids or [])}

    def get_subscription(self, subscription_id):
        return {"subscription_id": subscription_id} if subscription_id in self._known else None


def expected_client_state(req):
    return live.expected_client_state(req.get("raw"))


def utc_timestamp(req):
    base = req.get("base")
    base = live._parse_datetime(base) if base is not None else None
    return live.utc_timestamp(req.get("hours_from_now", 0), base=base)


def is_managed_subscription(req):
    store = FakeStore(req.get("known_ids"))
    return live.is_managed_subscription(store, req.get("payload"),
                                        expected_client_state_value=req.get("expected_client_state_value"))


def expected(r):
    f = r.get("func")
    if f == "expected_client_state":
        return expected_client_state(r)
    if f == "utc_timestamp":
        return utc_timestamp(r)
    if f == "is_managed_subscription":
        return is_managed_subscription(r)
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
        except AttributeError:
            py = {"error": "AttributeError"}
        except TypeError:
            py = {"error": "TypeError"}
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
