#!/usr/bin/env python3
"""Differential oracle for hermes_cli/observability/shared_metrics_subscriber.py
(gate w20_sharedmetr_d8839c).

The subscriber's own methods -- `__call__`, `deactivate`, `take_saved`,
`_persist`, `_record_milestones`, `_record_features`, `_record_engagement` -- are
lifted out of the class and executed as written (sync and async). The store, the
contract projections (`_classify`, `milestones_for`, `features_for`), the
engagement rollup and the install-age buckets are stubs the fixture scripts, so
the compared value is the decision trace: which counter routed where, which
latches fired, and whether the commit ticket advanced.
"""
import __future__
import ast
import json
import sys
import threading
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "observability" / "shared_metrics_subscriber.py"
GATE = "w20_sharedmetr_d8839c"
PKG = "hermes_cli.observability"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

CASE = {}
CLASSIFY_CALLED = []

# Contract constants the live methods read as module globals.
FEATURE_USED_MARK = "hermes.feature.used"
FEATURE_DISABLED_METRIC = "hermes.feature_disabled.count"
CLIENT_ACTIVE_METRIC = "hermes.client.active"
INSTALL_SNAPSHOT_METRIC = "hermes.install.snapshot"
COMMIT_TICKET_KEY = "hermes.shared_metrics.commit_ticket"
RUNTIME_INSTANCE_KEY = "hermes.relay.runtime_instance"


class _Logger:
    def warning(self, *a, **k):
        pass

    def debug(self, *a, **k):
        pass


class _Home:
    def __init__(self, s):
        self.s = s

    def __str__(self):
        return self.s


class Store:
    def __init__(self):
        self.rows = []
        self.milestones = []
        self.features = []
        self.errs = set(CASE.get("store_errors") or [])

    def record_counter(self, metric, dimensions, resource, amount):
        if metric in self.errs:
            raise RuntimeError("store down")
        self.rows.append({"kind": "counter", "metric": metric,
                          "dimensions": dimensions, "amount": amount})

    def record_counter_once_per_day(self, metric, dimensions, resource):
        if metric in self.errs:
            raise RuntimeError("store down")
        self.rows.append({"kind": "once_per_day", "metric": metric, "dimensions": dimensions})

    def record_client_active(self, resource):
        self.rows.append({"kind": "client_active"})

    def record_install_snapshot(self, dimensions, resource):
        self.rows.append({"kind": "install_snapshot", "dimensions": dimensions})

    def record_milestone(self, milestone, age, resource):
        if CASE.get("milestone_error"):
            raise RuntimeError("milestone write failed")
        self.milestones.append(milestone)

    def record_feature_adoption(self, feature, age, resource):
        if CASE.get("feature_error"):
            raise RuntimeError("feature write failed")
        self.features.append(feature)


TRACE = {"engagement": None}


engagement_stub = types.ModuleType(PKG + ".shared_metrics_engagement")
engagement_stub.interaction_surface = lambda classified: (CASE.get("engagement") or {}).get("surface")
engagement_stub.turn_route = lambda event: tuple((CASE.get("engagement") or {}).get("route") or ()) or None


def _record(store, resource, surface=None, route=None):
    TRACE["engagement"] = {"surface": surface, "route": list(route) if route else None}


engagement_stub.record = _record

signals_stub = types.ModuleType(PKG + ".shared_metrics_signals")
signals_stub.features_for = lambda metric, dimensions: list(
    (CASE.get("features_for") or {}).get(metric, []))
signals_stub.days_since_install_bucket = lambda home: CASE.get("age") or "unknown"

snapshot_stub = types.ModuleType(PKG + ".shared_metrics_snapshot")
snapshot_stub.install_age_bucket = lambda home: CASE.get("age") or "unknown"

for _name in ("hermes_cli", PKG):
    if _name not in sys.modules:
        _m = types.ModuleType(_name)
        _m.__path__ = []
        sys.modules[_name] = _m
sys.modules[PKG + ".shared_metrics_engagement"] = engagement_stub
sys.modules[PKG + ".shared_metrics_signals"] = signals_stub
sys.modules[PKG + ".shared_metrics_snapshot"] = snapshot_stub

M = {
    "__name__": PKG + ".shared_metrics_subscriber_live",
    "__package__": PKG,
    "logger": _Logger(),
    "threading": threading,
    "RUNTIME_INSTANCE_KEY": RUNTIME_INSTANCE_KEY,
    "COMMIT_TICKET_KEY": COMMIT_TICKET_KEY,
    "CLIENT_ACTIVE_METRIC": CLIENT_ACTIVE_METRIC,
    "FEATURE_DISABLED_METRIC": FEATURE_DISABLED_METRIC,
    "FEATURE_USED_MARK": FEATURE_USED_MARK,
    "INSTALL_SNAPSHOT_METRIC": INSTALL_SNAPSHOT_METRIC,
    "milestones_for": lambda metric, dimensions: list(
        (CASE.get("milestones_for") or {}).get(metric, [])),
    "signals": signals_stub,
    "engagement": engagement_stub,
}

METHODS = {"__call__", "deactivate", "take_saved", "_persist", "_record_milestones",
           "_record_features", "_record_engagement"}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            if isinstance(node, ast.AsyncFunctionDef):
                raise AssertionError("async method under test is not supported here")
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


_exec(SRC, METHODS, M)
M = M  # the exec'd methods live in this namespace


class Sub:
    pass


for _name in METHODS:
    setattr(Sub, _name, M[_name])

Sub.__dict__  # keep the class referenced


def make(case):
    s = Sub()
    s.store = Store()
    s._client_resource = {"client": "slermes/test"}
    s._runtime_id = case.get("runtime_id")
    s._active = bool(case.get("active", True))
    s._lock = threading.RLock()
    s._milestones_done = set(case.get("milestones_done") or [])
    s._features_done = set(case.get("features_done") or [])
    s._saved_tickets = {}
    s._hermes_home = _Home(case.get("age") or "")

    def _classify(event):
        CLASSIFY_CALLED.append(True)
        return [(m, d, a) for m, d, a in (case.get("classified") or [])]

    s._classify = _classify
    return s


def expected(r):
    global CASE
    CASE = r
    if r.get("func") == "persist_kind":
        sub = make({"classified": []})
        sub._persist(r.get("metric") or "", {"a": 1}, 1)
        return sub.store.rows[-1]["kind"] if sub.store.rows else "noop"
    CLASSIFY_CALLED.clear()
    TRACE["engagement"] = None
    sub = make(r)
    event = types.SimpleNamespace(metadata=(r.get("event") or {}).get("metadata"))
    sub(event)
    taken = None
    if r.get("take"):
        taken = sub.take_saved(r["take"])
    classified = r.get("classified") or []
    value = {
        "filtered": not CLASSIFY_CALLED,
        "engagement": TRACE["engagement"],
        "persisted": sub.store.rows,
        "milestones": sub.store.milestones,
        "features": sub.store.features,
        "tickets": dict(sub._saved_tickets),
        "active": bool(sub._active),
        "stopped": bool(CLASSIFY_CALLED) and bool(classified) and not r.get("active", True),
        "taken": taken,
    }
    return {"status": "ok", "value": value}


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
        if not isinstance(r, dict) or r.get("func") not in ("call", "persist_kind"):
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH call case=%r live-error=%r" % (r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH call case=%r\n  py=%r\n   c=%r" % (r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
