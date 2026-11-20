#!/usr/bin/env python3
"""Generate tests/fixtures/cron_incidents_flow2.jsonl (FLOW2 oracle fixtures).

Each case is {"func", "in"}; the harness echoes "in" and the oracle recomputes
the live reference from it. Scripted seam results live in in.io: a single
{"ret"/"raise"/"state"} object is reused for every call; a bare LIST is a
per-call queue. Run: python tests/gen_cron_incidents_flow2_cases.py [--check].
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures" / "cron_incidents_flow2.jsonl"


def raise_(t, msg="boom", **kw):
    d = {"type": t, "message": msg}
    d.update(kw)
    return {"raise": d}


def ret(v):
    return {"ret": v}


def row(state):
    return {"fetchone": {"state": state}}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []

# ── db_path ────────────────────────────────────────────────────────────────
CASES += [
    case("db_path", io={"executions_file": ret("/override/exec.db")}),
    case("db_path", module_file="/mod/exec.db",
         io={"executions_file": ret(None)}),
    case("db_path", io={"executions_file": ret(None),
                        "get_hermes_home": ret("/home/u"),
                        "path.resolve": ret("/home/u")}),
    case("db_path", io={"executions_file": ret(None),
                        "get_hermes_home": ret("/home/u/"),
                        "path.resolve": ret("/home/u/")}),
    case("db_path", io={"executions_file": raise_("RuntimeError", "attr fail")}),
    case("db_path", io={"executions_file": ret(None),
                        "get_hermes_home": raise_("OSError", "no home")}),
    case("db_path", io={"executions_file": ret(None),
                        "get_hermes_home": ret("/h"),
                        "path.resolve": raise_("SystemExit", "exit")}),
]

# ── connect ────────────────────────────────────────────────────────────────
CASES += [
    case("connect", io={"_db_path": ret("/h/cron/executions.db"),
                        "ensure_cron_dir": ret(None), "open_db": ret("conn1")}),
    case("connect", io={"_db_path": raise_("OSError", "path")}),
    case("connect", io={"_db_path": ret("/h/cron/executions.db"),
                        "ensure_cron_dir": raise_("PermissionError", "mkdir")}),
    case("connect", io={"_db_path": ret("/h/cron/executions.db"),
                        "ensure_cron_dir": ret(None),
                        "open_db": raise_("OSError", "open")}),
    case("connect", io={"_db_path": ret("/h/cron/executions.db"),
                        "ensure_cron_dir": ret(None), "open_db": ret(None)}),
    case("connect", io={"_db_path": ret("C:/h/cron/executions.db"),
                        "ensure_cron_dir": ret(None), "open_db": ret("conn2")}),
]

# ── initialize_schema ──────────────────────────────────────────────────────
_SCHEMA_IO = {"sql.create_table": ret(None), "add_column_if_missing": ret(None),
              "sql.index_job": ret(None), "sql.index_state": ret(None)}
CASES += [
    case("initialize_schema", io=dict(_SCHEMA_IO)),
    case("initialize_schema", io=dict(_SCHEMA_IO, **{"sql.create_table": raise_("OSError", "ddl")})),
    case("initialize_schema", io=dict(_SCHEMA_IO, **{"add_column_if_missing": raise_("ValueError", "col")})),
    case("initialize_schema", io=dict(_SCHEMA_IO, **{"sql.index_job": raise_("OSError", "idx")})),
    case("initialize_schema", io=dict(_SCHEMA_IO, **{"sql.index_state": raise_("OSError", "idx2")})),
    case("initialize_schema", io=dict(_SCHEMA_IO,
         **{"sql.create_table": {"ret": None, "state": {"schema": 1}}})),
]

# ── transaction ────────────────────────────────────────────────────────────
CASES += [
    case("transaction", io={"_connect": ret("conn1"), "transaction.enter": ret("c1"),
                            "transaction.exit": ret(None)}),
    case("transaction", io={"_connect": raise_("OSError", "connect")}),
    case("transaction", io={"_connect": ret("conn1"),
                            "transaction.enter": raise_("RuntimeError", "enter")}),
    case("transaction", io={"_connect": ret("conn1"), "transaction.enter": ret("c1"),
                            "transaction.exit": raise_("RuntimeError", "exit")}),
    case("transaction", io={"_connect": ret("conn1"), "transaction.enter": ret(None),
                            "transaction.exit": ret(None)}),
    case("transaction", io={"_connect": ret("conn1"), "transaction.enter": ret("c1"),
                            "transaction.exit": {"ret": None, "state": {"tx": 1}}}),
]

# ── set_incident_state ─────────────────────────────────────────────────────
_SET_BASE = {"hermes_now": ret("T"), "tx.enter": ret(None), "tx.exit": ret(None)}
CASES += [
    case("set_incident_state", incident_id="j1", inc_state="bogus"),
    case("set_incident_state", incident_id="j1", inc_state="detected",
         io=dict(_SET_BASE, **{"sql.select": row("detected")})),
    case("set_incident_state", incident_id="j1", inc_state="alerted",
         io=dict(_SET_BASE, **{"sql.select": row("detected"),
                               "sql.update_alerted": ret(None)})),
    case("set_incident_state", incident_id="j1", inc_state="closed",
         io=dict(_SET_BASE, **{"sql.select": row("alerted"),
                               "sql.update_closed": ret(None)})),
    case("set_incident_state", incident_id="j1", inc_state="resolved",
         io=dict(_SET_BASE, **{"sql.select": row("alerted"),
                               "sql.update_state": ret(None)})),
    case("set_incident_state", incident_id="j1", inc_state="detected",
         io=dict(_SET_BASE, **{"sql.select": ret({"fetchone": None})})),
    case("set_incident_state", incident_id="j1", inc_state="detected",
         io=dict(_SET_BASE, **{"sql.select": row("closed")})),
    case("set_incident_state", incident_id="j1", inc_state="alerted",
         io=dict(_SET_BASE, **{"hermes_now": raise_("OSError", "clock")})),
    case("set_incident_state", incident_id="j1", inc_state="alerted",
         io=dict(_SET_BASE, **{"sql.select": raise_("OSError", "select")})),
    case("set_incident_state", incident_id="j1", inc_state="alerted",
         io=dict(_SET_BASE, **{"sql.select": row("detected"),
                               "sql.update_alerted": raise_("ValueError", "upd"),
                               "tx.exit": raise_("RuntimeError", "exit")})),
    case("set_incident_state", incident_id="j1", inc_state="alerted",
         io=dict(_SET_BASE, **{"sql.select": row("detected"),
                               "sql.update_alerted": ret(None),
                               "tx.enter": {"ret": None, "state": {"seen": 1}}})),
]

# ── ack_incident ───────────────────────────────────────────────────────────
CASES += [
    case("ack_incident", incident_id="j1",
         io={"set_incident_state": ret(True)}),
    case("ack_incident", incident_id="j1",
         io={"set_incident_state": ret(False)}),
    case("ack_incident", incident_id="j1",
         io={"set_incident_state": raise_("OSError", "db")}),
    case("ack_incident", incident_id="j1",
         io={"set_incident_state": ret(None)}),
    case("ack_incident", incident_id="j1",
         io={"set_incident_state": {"ret": True, "state": {"acked": 1}}}),
]

# ── close_incidents ────────────────────────────────────────────────────────
_CI_BASE = {"hermes_now": ret("T"), "tx.enter": ret(None), "tx.exit": ret(None)}
CASES += [
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": ret({"rowcount": 3})})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": ret({"rowcount": 0})})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": ret({})})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"hermes_now": raise_("OSError", "clock")})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": raise_("OSError", "upd")})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": ret({"rowcount": 5}),
                              "tx.exit": raise_("RuntimeError", "exit")})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": ret({"rowcount": 2.7})})),
    case("close_incidents", job_id="j1",
         io=dict(_CI_BASE, **{"sql.update_resolved": {"ret": {"rowcount": 1},
                                                       "state": {"closed": 1}}})),
]

# ── state_filter (pure) ────────────────────────────────────────────────────
CASES += [
    case("state_filter"),
    case("state_filter", inc_state=None),
    case("state_filter", inc_state="detected"),
    case("state_filter", inc_state="closed"),
    case("state_filter", inc_state=""),
    case("state_filter", inc_state=5),
]

# ── count_incidents ────────────────────────────────────────────────────────
_CNT_BASE = {"tx.enter": ret(None), "tx.exit": ret(None)}
CASES += [
    case("count_incidents", io=dict(_CNT_BASE, **{"sql.count": ret({"fetchone": {"n": 5}})})),
    case("count_incidents", inc_state="detected",
         io=dict(_CNT_BASE, **{"sql.count_filtered": ret({"fetchone": {"n": 2}})})),
    case("count_incidents", inc_state="bogus"),
    case("count_incidents", inc_state=None,
         io=dict(_CNT_BASE, **{"sql.count": ret({"fetchone": None})})),
    case("count_incidents", io=dict(_CNT_BASE, **{"sql.count": raise_("OSError", "count")})),
    case("count_incidents", io=dict(_CNT_BASE, **{"sql.count": ret({"fetchone": {"n": 4}}),
                                                  "tx.exit": raise_("RuntimeError", "exit")})),
    case("count_incidents", io=dict(_CNT_BASE, **{"sql.count": ret({"fetchone": {"n": 0}})})),
    case("count_incidents", io=dict(_CNT_BASE,
         **{"sql.count": {"ret": {"fetchone": {"n": 3}}, "state": {"cnt": 1}}})),
]


def write():
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    with FIXTURE.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    return counts


def check():
    import importlib.util
    spec = importlib.util.spec_from_file_location("oci2", HERE / "oracle_cron_incidents_flow2.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    errs = 0
    for c in CASES:
        try:
            mod.run_case(c["func"], c["in"])
        except Exception as e:
            errs += 1
            print(f"PYERR {c['func']}: {type(e).__name__}: {e}")
    print(f"GEN-CHECK cron_incidents_flow2: {len(CASES)} cases, {errs} errors")
    return 0 if errs == 0 else 1


if __name__ == "__main__":
    counts = write()
    print("wrote", FIXTURE, len(CASES), "cases")
    print("per-func:", json.dumps(counts, sort_keys=True))
    if "--check" in sys.argv:
        sys.exit(check())
