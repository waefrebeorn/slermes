#!/usr/bin/env python3
"""Generate tests/fixtures/shared_metrics_flow.jsonl — the scripted-seam case
corpus for the shared_metrics_flow FLOW port (21 unclaimed funcs of
hermes_cli/observability/shared_metrics.py). >=5 cases per ported func, covering
happy paths, branch gates, scripted Exception (swallowed / re-raised), scripted
BaseException (passthrough), and fail-closed missing scripts.

Each case: {"func": <live_name>, "in": {...args..., "io": {seam: script},
"state": {...mutable carrier...}}}. A bare JSON array script is a per-call QUEUE;
a list RESULT is wrapped {"__script__": true, "ret": [...]}; a raise is
{"__raise__": kind, "message": msg}. Deterministic (no randomness).

Domain (ASCII scope, documented in the port header): datetimes are canonical
datetime.isoformat() strings; _compact_json operands carry no floats; db.execute
scripts are cursor specs {one, all, rowcount}; rows are dicts; stateful funcs
carry the full {id, _deferred, _drain_at_exit, _DEFERRING_STORES} state shape.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "shared_metrics_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def seq(*vals):
    return list(vals)


def listret(arr):
    return {"__script__": True, "ret": arr}


def cur(**kw):
    return dict(kw)


def state(deferred=None, drain=False, stores=None, sid="s1"):
    return {"id": sid, "_deferred": deferred if deferred is not None else [],
            "_drain_at_exit": drain, "_DEFERRING_STORES": stores if stores is not None else []}


NOW = "2026-11-18T12:00:00+00:00"
NOW_Z = "2026-11-18T12:00:00Z"
RES = {"hermes_version": "1.0", "os_family": "linux", "architecture": "x86_64", "install_method": "pip"}
RV = ["1.0", "linux", "x86_64", "pip"]
CONN = {"id": "c1"}
DIMS = {"provider": "openai"}
LOCKED = raise_("OperationalError", "database is locked")
IOERR = raise_("OperationalError", "disk I/O error")

# base io for a latch core happy path (no prior state row)
def latch_io(insert_rowcount=1, select_one=None):
    return {
        "store._validate_counter": None,
        "_utc_now": NOW,
        "store._connection": CONN,
        "write_txn": None,
        "db.execute": seq(cur(one=select_one) if select_one is not None else cur(), cur(rowcount=insert_rowcount)),
        "_isoformat": NOW_Z,
        "store._install_id": "iid",
        "store._record_counter_in_transaction": None,
    }


# ── _compact_json ──
add("_compact_json", value={"b": 1, "a": "x"})
add("_compact_json", value={"z": {"y": 2, "x": [True, None]}})
add("_compact_json", value=None)
add("_compact_json", value="hi")
add("_compact_json", value=[3, 1, 2])
add("_compact_json", value={"a": 'quote"and\\slash'})
add("_compact_json", value={})
add("_compact_json", value=True)
add("_compact_json", value={"k": "tab\tnew\n"})

# ── _row_resource ──
add("_row_resource", row=RES)
add("_row_resource", row={**RES, "extra": "ignored"})
add("_row_resource", row={"hermes_version": "1.0", "os_family": "linux", "architecture": "x86_64"})  # missing install_method
add("_row_resource", row={})
add("_row_resource", row=None)
add("_row_resource", row={"hermes_version": "2", "os_family": "win", "architecture": "arm64", "install_method": "exe"})

# ── _ensure_private ──
add("_ensure_private", path="/h/t", mode=448, io={"path.mkdir": None, "path.chmod": None})
add("_ensure_private", path="/h/f", mode=384, io={"path.touch": None, "path.chmod": None})
add("_ensure_private", path="/h/f", mode=384, io={"path.touch": None, "path.chmod": raise_("OSError")})
add("_ensure_private", path="/h/f", mode=384, io={"path.touch": None, "path.chmod": raise_("FileNotFoundError")})
add("_ensure_private", path="/h/f", mode=384, io={"path.touch": None, "path.chmod": raise_("ValueError")})
add("_ensure_private", path="/h/t", mode=448, io={"path.mkdir": raise_("RuntimeError"), "path.chmod": None})
add("_ensure_private", path="/h/f", mode=384, io={"path.touch": None, "path.chmod": raise_("SystemExit")})
add("_ensure_private", path="/h/t", mode=448, io={})

# ── drain_deferring_stores ──
add("drain_deferring_stores", deadline=10.0, state=state(stores=[{"id": "s1"}, {"id": "s2"}]),
    io={"time.monotonic": seq(1.0, 2.0), "store.drain_deferred": None})
add("drain_deferring_stores", deadline=1.0, state=state(stores=[{"id": "s1"}]),
    io={"time.monotonic": 2.0, "store.drain_deferred": None})
add("drain_deferring_stores", deadline=10.0, state=state(stores=[]), io={})
add("drain_deferring_stores", deadline=10.0, state=state(stores=[{"id": "s1"}]),
    io={"time.monotonic": raise_("RuntimeError")})
add("drain_deferring_stores", deadline=10.0, state=state(stores=[{"id": "s1"}]),
    io={"time.monotonic": 1.0, "store.drain_deferred": raise_("SystemExit")})
add("drain_deferring_stores", deadline=10.0, state=state(stores=[{"id": "s1"}]), io={})
add("drain_deferring_stores", deadline=5.0, state=state(stores=[{"id": "s1"}, {"id": "s2"}]),
    io={"time.monotonic": 4.9995, "store.drain_deferred": None})

# ── _write ──
add("_write", io={"store._connection": CONN, "write_txn": None}, state=state())
add("_write", busy_timeout_ms=5000, io={"store._connection": CONN, "write_txn": None}, state=state())
add("_write", io={"store._connection": raise_("RuntimeError"), "write_txn": None}, state=state())
add("_write", io={"store._connection": CONN, "write_txn": LOCKED}, state=state())
add("_write", io={}, state=state())
add("_write", io={"store._connection": None, "write_txn": None}, state=state())

# ── install_snapshot_due ──
add("install_snapshot_due", io={"store._connection": CONN, "db.execute": cur(one=None), "_utc_now": NOW}, state=state())
add("install_snapshot_due", io={"store._connection": CONN, "db.execute": cur(one={"value": "2026-11-10T00:00:00+00:00"}),
                                "store._parse_state_timestamp": "2026-11-10T00:00:00+00:00", "_utc_now": NOW}, state=state())
add("install_snapshot_due", io={"store._connection": CONN, "db.execute": cur(one={"value": "2026-11-18T11:00:00+00:00"}),
                                "store._parse_state_timestamp": "2026-11-18T11:00:00+00:00", "_utc_now": NOW}, state=state())
add("install_snapshot_due", io={"store._connection": CONN, "db.execute": cur(one={"value": "2026-11-19T00:00:00+00:00"}),
                                "store._parse_state_timestamp": "2026-11-19T00:00:00+00:00", "_utc_now": NOW}, state=state())
add("install_snapshot_due", io={"store._connection": CONN, "db.execute": cur(one={"value": "garbage"}),
                                "store._parse_state_timestamp": None, "_utc_now": NOW}, state=state())
add("install_snapshot_due", io={"store._connection": CONN, "db.execute": raise_("RuntimeError"), "_utc_now": NOW}, state=state())
add("install_snapshot_due", io={"store._connection": CONN}, state=state())

# ── _record_once_per_interval ──
add("_record_once_per_interval", state_key="k", metric_name="hermes.install.snapshot", dimensions=DIMS, resource=RES,
    io=latch_io(), state=state())
add("_record_once_per_interval", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**latch_io(), "db.execute": cur(one={"value": "2026-11-18T11:00:00+00:00"}),
        "store._parse_state_timestamp": "2026-11-18T11:00:00+00:00"}, state=state())
add("_record_once_per_interval", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**latch_io(), "db.execute": seq(cur(one={"value": "2026-11-19T00:00:00+00:00"}), cur()),
        "store._parse_state_timestamp": "2026-11-19T00:00:00+00:00"}, state=state())
add("_record_once_per_interval", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**latch_io(), "store._validate_counter": raise_("ValueError")}, state=state())
add("_record_once_per_interval", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**latch_io(), "db.execute": LOCKED}, state=state())
add("_record_once_per_interval", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**latch_io(), "_utc_now": raise_("SystemExit")}, state=state())
add("_record_once_per_interval", state_key="k", metric_name="m", dimensions=DIMS, resource=RES, io={}, state=state())

# ── record_install_snapshot ──
add("record_install_snapshot", dimensions=DIMS, resource=RES, io=latch_io(), state=state())
add("record_install_snapshot", dimensions=DIMS, resource=RES,
    io={**latch_io(), "store._validate_counter": raise_("ValueError")}, state=state())
add("record_install_snapshot", dimensions=DIMS, resource=RES,
    io={**latch_io(), "db.execute": cur(one={"value": "2026-11-18T11:00:00+00:00"}),
        "store._parse_state_timestamp": "2026-11-18T11:00:00+00:00"}, state=state())
add("record_install_snapshot", dimensions=DIMS, resource=RES, io={}, state=state())
add("record_install_snapshot", dimensions=DIMS, resource=RES,
    io={**latch_io(), "store._connection": raise_("KeyboardInterrupt")}, state=state())

# ── _record_once_ever ──
def ever_io(rowcount=1):
    return {"store._validate_counter": None, "_utc_now": NOW, "store._connection": CONN, "write_txn": None,
            "_isoformat": NOW_Z, "db.execute": cur(rowcount=rowcount), "store._install_id": "iid",
            "store._record_counter_in_transaction": None}


add("_record_once_ever", state_key="milestone:m1", metric_name="hermes.install.milestone", dimensions=DIMS, resource=RES,
    io=ever_io(1), state=state())
add("_record_once_ever", state_key="milestone:m1", metric_name="m", dimensions=DIMS, resource=RES,
    io=ever_io(0), state=state())
add("_record_once_ever", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**ever_io(), "store._validate_counter": raise_("ValueError")}, state=state())
add("_record_once_ever", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**ever_io(), "db.execute": LOCKED}, state=state())
add("_record_once_ever", state_key="k", metric_name="m", dimensions=DIMS, resource=RES,
    io={**ever_io(), "store._install_id": raise_("RuntimeError")}, state=state())
add("_record_once_ever", state_key="k", metric_name="m", dimensions=DIMS, resource=RES, io={}, state=state())

# ── record_milestone ──
add("record_milestone", milestone="m1", install_age_bucket="0-7", resource=RES, io=ever_io(1), state=state())
add("record_milestone", milestone="m1", install_age_bucket="0-7", resource=RES, io=ever_io(0), state=state())
add("record_milestone", milestone="m1", install_age_bucket="0-7", resource=RES,
    io={**ever_io(), "store._validate_counter": raise_("ValueError")}, state=state())
add("record_milestone", milestone="", install_age_bucket="0-7", resource=RES, io=ever_io(1), state=state())
add("record_milestone", milestone="m1", install_age_bucket="0-7", resource=RES, io={}, state=state())
add("record_milestone", milestone="m1", install_age_bucket="0-7", resource=RES,
    io={**ever_io(), "_utc_now": raise_("SystemExit")}, state=state())

# ── record_feature_adoption ──
add("record_feature_adoption", feature="f1", days_bucket="0-7", resource=RES, io=ever_io(1), state=state())
add("record_feature_adoption", feature="f1", days_bucket="0-7", resource=RES, io=ever_io(0), state=state())
add("record_feature_adoption", feature="f1", days_bucket="0-7", resource=RES,
    io={**ever_io(), "store._validate_counter": raise_("ValueError")}, state=state())
add("record_feature_adoption", feature="f1", days_bucket="0-7", resource=RES, io={}, state=state())
add("record_feature_adoption", feature="f-x", days_bucket="30+", resource=RES, io=ever_io(1), state=state())

# ── _latched ──
add("_latched", prefix="milestone:", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "milestone:b"}, {"key": "milestone:a"}])}, state=state())
add("_latched", prefix="milestone:", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "milestone:a"}, {"key": "milestone:a"}])}, state=state())
add("_latched", prefix="milestone:", io={"store._connection": CONN, "db.execute": cur(all=[])}, state=state())
add("_latched", prefix="milestone:", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "milestone:"}, {"key": "milestone:x"}])}, state=state())
add("_latched", prefix="milestone:", io={"store._connection": CONN, "db.execute": raise_("RuntimeError")}, state=state())
add("_latched", prefix="milestone:", io={"store._connection": CONN, "db.execute": cur(all=[{"other": "x"}])}, state=state())
add("_latched", prefix="milestone:", io={"store._connection": CONN}, state=state())

# ── recorded_milestones ──
add("recorded_milestones", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "milestone:b"}, {"key": "milestone:a"}])}, state=state())
add("recorded_milestones", io={"store._connection": CONN, "db.execute": cur(all=[])}, state=state())
add("recorded_milestones", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "milestone:a"}, {"key": "milestone:a"}])}, state=state())
add("recorded_milestones", io={"store._connection": CONN, "db.execute": raise_("RuntimeError")}, state=state())
add("recorded_milestones", io={"store._connection": CONN}, state=state())

# ── recorded_features ──
add("recorded_features", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "feature:z"}, {"key": "feature:a"}])}, state=state())
add("recorded_features", io={"store._connection": CONN, "db.execute": cur(all=[])}, state=state())
add("recorded_features", io={"store._connection": CONN,
    "db.execute": cur(all=[{"key": "feature:a"}, {"key": "feature:a"}])}, state=state())
add("recorded_features", io={"store._connection": CONN, "db.execute": raise_("ValueError")}, state=state())
add("recorded_features", io={"store._connection": CONN}, state=state())

# ── record_counter_once_per_day ──
def daily_io(rowcount=1):
    return {"store._validate_counter": None, "_utc_now": NOW, "store._connection": CONN, "write_txn": None,
            "db.execute": cur(rowcount=rowcount), "store._install_id": "iid",
            "store._record_counter_in_transaction": None}


add("record_counter_once_per_day", metric_name="hermes.install.snapshot", dimensions={"surface": "cli", "provider": "openai"},
    resource=RES, io=daily_io(1), state=state())
add("record_counter_once_per_day", metric_name="m", dimensions={"provider": "openai"}, resource=RES,
    io=daily_io(0), state=state())
add("record_counter_once_per_day", metric_name="m", dimensions={}, resource=RES, io=daily_io(1), state=state())
add("record_counter_once_per_day", metric_name="m", dimensions=DIMS, resource=RES,
    io={**daily_io(), "store._validate_counter": raise_("ValueError")}, state=state())
add("record_counter_once_per_day", metric_name="m", dimensions=DIMS, resource=RES,
    io={**daily_io(), "_utc_now": None}, state=state())
add("record_counter_once_per_day", metric_name="m", dimensions=DIMS, resource=RES, io={}, state=state())
add("record_counter_once_per_day", metric_name="m", dimensions=DIMS, resource=RES,
    io={**daily_io(), "db.execute": LOCKED}, state=state())

# ── _update_rollup_state ──
ROLL_ROW = ["hermes.install.snapshot", DIMS, RES, "2026-11-18"]
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": seq(cur(one={"value": '{"a":1}'}), cur()),
        "rollup.update": listret([{"a": 2}, [ROLL_ROW]]),
        "contract.counter_dimensions_are_valid": True, "contract.client_resource_is_valid": True,
        "store._install_id": "iid", "store._record_counter_in_transaction": None}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": seq(cur(one=None), cur()), "rollup.update": listret([{}, []])}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": seq(cur(one={"value": "not json"}), cur()), "rollup.update": listret([{}, []])}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": seq(cur(one={"value": "[1,2]"}), cur()), "rollup.update": listret([{}, []])}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": cur(one=None), "rollup.update": None}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": cur(one=None), "rollup.update": listret([1])}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": cur(one=None), "rollup.update": listret([1, 2, 3])}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": seq(cur(one=None), cur()), "rollup.update": listret([{}, [ROLL_ROW]]),
        "contract.counter_dimensions_are_valid": False}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": cur(one=None), "rollup.update": listret([{}, [ROLL_ROW]]),
        "contract.counter_dimensions_are_valid": raise_("RuntimeError")}, state=state())
add("_update_rollup_state", connection=CONN, state_key="rollup", update={"__callable__": "rollup.update"},
    io={"db.execute": cur(one=None)}, state=state())

# ── _write_or_defer ──
WCB = {"__callable__": "write.cb"}
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None, "write.cb": None}, state=state())
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None, "write.cb": LOCKED,
                                     "atexit.register": None}, state=state())
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None, "write.cb": IOERR,
                                     "atexit.register": None}, state=state())
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None,
                                     "write.cb": raise_("ValueError")}, state=state())
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None,
                                     "write.cb": raise_("SystemExit")}, state=state())
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None, "old.cb": None,
                                     "write.cb": LOCKED, "atexit.register": None},
    state=state(deferred=[{"__callable__": "old.cb"}]))
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None, "write.cb": LOCKED},
    state=state(drain=True))
add("_write_or_defer", write=WCB, io={}, state=state())
add("_write_or_defer", write=WCB, io={"store._connection": CONN, "write_txn": None, "o": None,
                                     "write.cb": LOCKED, "atexit.register": None},
    state=state(deferred=[{"__callable__": "o"}] * 1025))

# ── update_rollup_state ──
add("update_rollup_state", state_key="rollup", update={"__callable__": "rollup.update"},
    io={"store._connection": CONN, "write_txn": None, "db.execute": seq(cur(one=None), cur()),
        "rollup.update": listret([{}, []])}, state=state())
add("update_rollup_state", state_key="rollup", update={"__callable__": "rollup.update"},
    io={"store._connection": CONN, "write_txn": None, "db.execute": cur(one=None),
        "rollup.update": LOCKED, "atexit.register": None}, state=state())
add("update_rollup_state", state_key="rollup", update={"__callable__": "rollup.update"},
    io={"store._connection": CONN, "write_txn": None, "db.execute": cur(one=None),
        "rollup.update": None}, state=state())
add("update_rollup_state", state_key="rollup", update={"__callable__": "rollup.update"}, io={}, state=state())
add("update_rollup_state", state_key="rollup", update={"__callable__": "rollup.update"},
    io={"store._connection": CONN, "write_txn": None, "db.execute": cur(one=None),
        "rollup.update": raise_("SystemExit")}, state=state())

# ── drain_deferred ──
add("drain_deferred", io={}, state=state())
add("drain_deferred", io={"store._connection": CONN, "write_txn": None, "write.cb": None},
    state=state(deferred=[WCB]))
add("drain_deferred", io={"store._connection": CONN, "write_txn": None, "write.cb": LOCKED},
    state=state(deferred=[WCB]))
add("drain_deferred", io={"store._connection": CONN, "write_txn": None, "write.cb": raise_("ValueError"),
                         "logger.warning": None}, state=state(deferred=[WCB]))
add("drain_deferred", io={"store._connection": CONN, "write_txn": None, "write.cb": IOERR,
                         "logger.warning": None}, state=state(deferred=[WCB]))
add("drain_deferred", io={"store._connection": CONN, "write_txn": None, "write.cb": raise_("SystemExit")},
    state=state(deferred=[WCB]))
add("drain_deferred", io={"store._connection": CONN, "write_txn": None, "write.cb": raise_("ValueError"),
                         "logger.warning": raise_("RuntimeError")}, state=state(deferred=[WCB]))
add("drain_deferred", busy_timeout_ms=250, io={"store._connection": CONN, "write_txn": None, "write.cb": None},
    state=state(deferred=[WCB]))
add("drain_deferred", io={}, state=state(deferred=[WCB]))

# ── _mark_packaged ──
add("_mark_packaged", connection=CONN, rows=[{"metric_name": "m1", "dimensions_json": "{}"}],
    period_value="2026-11-18", resource_values=RV, io={"db.execute": cur()}, state=state())
add("_mark_packaged", connection=CONN, rows=[{"metric_name": "m1", "dimensions_json": "{}"},
                                             {"metric_name": "m2", "dimensions_json": '{"a":1}'}],
    period_value="2026-11-18", resource_values=RV, io={"db.execute": cur()}, state=state())
add("_mark_packaged", connection=CONN, rows=[], period_value="2026-11-18", resource_values=RV, io={}, state=state())
add("_mark_packaged", connection=CONN, rows=[{"dimensions_json": "{}"}], period_value="p",
    resource_values=RV, io={"db.execute": cur()}, state=state())
add("_mark_packaged", connection=CONN, rows=[{"metric_name": "m1", "dimensions_json": "{}"}],
    period_value="p", resource_values=RV, io={"db.execute": raise_("RuntimeError")}, state=state())
add("_mark_packaged", connection=CONN, rows=[{"metric_name": "m1", "dimensions_json": "{}"}],
    period_value="p", resource_values=RV, io={}, state=state())

# ── _package_metrics ──
M1 = {"name": "m", "type": "counter", "dimensions": {"a": "1"}, "value": 2}
M2 = {"name": "m", "type": "counter", "dimensions": {"a": "1"}, "value": 3}
M3 = {"name": "n", "type": "counter", "dimensions": {"b": "2"}, "value": 5}
add("_package_metrics", rows=[{"metric_name": "m"}, {"metric_name": "n"}],
    io={"store._package_metric": seq(M1, M3)}, state=state())
add("_package_metrics", rows=[{"metric_name": "m"}, {"metric_name": "m"}],
    io={"store._package_metric": seq(M1, M2)}, state=state())
add("_package_metrics", rows=[{"metric_name": "m"}, {"metric_name": "n"}],
    io={"store._package_metric": seq(None, M3)}, state=state())
add("_package_metrics", rows=[], io={}, state=state())
add("_package_metrics", rows=[{"metric_name": "m"}],
    io={"store._package_metric": raise_("ValueError")}, state=state())
add("_package_metrics", rows=[{"metric_name": "m"}],
    io={"store._package_metric": {"type": "counter", "dimensions": {}, "value": 1}}, state=state())
add("_package_metrics", rows=[{"metric_name": "m"}, {"metric_name": "m"}],
    io={"store._package_metric": seq(M1, {"name": "m", "dimensions": {"a": "1"}, "value": "x"})}, state=state())
add("_package_metrics", rows=[{"metric_name": "m"}], io={}, state=state())


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    # per-func coverage report
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  <-- UNDER 5"
        print("  %-32s %d%s" % (fn, counts[fn], flag))
    under = [fn for fn, n in counts.items() if n < 5]
    if under:
        raise SystemExit("FUNCS UNDER 5 CASES: %s" % under)


if __name__ == "__main__":
    main()
