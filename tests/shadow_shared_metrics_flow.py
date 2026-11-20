#!/usr/bin/env python3
"""Shadow-diff probe: a faithful Python transcription of the C port
(src/cli/shared_metrics_flow.c) decision logic, driven over the SAME fixtures and
the SAME scripted-seam machinery as tests/oracle_shared_metrics_flow.py, then
diffed against the oracle's LIVE-def reference. Because the C harness cannot be
linked/run under the code-only constraint, this mirror isolates C-port logic
drift: it builds the identical seam arg dicts and replicates the C branch /
parse / truncation / busy-detection algorithms, while reusing the oracle's
Script/record/proj/materialize/norm/proj_state so the trace+state machinery is
shared. A 0-mismatch diff means the C decision sequences agree with live.

Run: python tests/shadow_shared_metrics_flow.py
"""
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("orc", HERE / "oracle_shared_metrics_flow.py")
O = importlib.util.module_from_spec(spec)
sys.modules["orc"] = O
spec.loader.exec_module(O)

FX = HERE / "fixtures" / "shared_metrics_flow.jsonl"


# ── C algorithm transcriptions ──────────────────────────────────────────────
def m_compact_json(v):
    """Transcribe shm_compact_json / shm_ser_node (sorted keys, C escaping)."""
    def ser_str(s):
        out = ['"']
        for ch in s:
            c = ord(ch)
            if ch == '"': out.append('\\"')
            elif ch == '\\': out.append('\\\\')
            elif ch == '\n': out.append('\\n')
            elif ch == '\r': out.append('\\r')
            elif ch == '\t': out.append('\\t')
            elif ch == '\b': out.append('\\b')
            elif ch == '\f': out.append('\\f')
            elif c < 0x20: out.append('\\u%04x' % c)
            else: out.append(ch)
        out.append('"')
        return ''.join(out)

    def ser(x):
        if x is None: return "null"
        if isinstance(x, bool): return "true" if x else "false"
        if isinstance(x, (int, float)):
            if isinstance(x, float) and not x.is_integer():
                # best-effort shortest round-trip (floats excluded from domain)
                for prec in range(1, 18):
                    t = "%.*g" % (prec, x)
                    if float(t) == x: return t
                return "%.17g" % x
            return "%d" % int(x)
        if isinstance(x, str): return ser_str(x)
        if isinstance(x, list): return "[" + ",".join(ser(i) for i in x) + "]"
        if isinstance(x, dict):
            keys = sorted(x.keys())   # strcmp == codepoint for ASCII
            return "{" + ",".join(ser_str(k) + ":" + ser(x[k]) for k in keys) + "}"
        return "null"
    return ser(v)


def m_days_from_civil(y, m, d):
    y -= (m <= 2)
    era = (y if y >= 0 else y - 399) // 400
    yoe = y - era * 400
    doy = (153 * (m + (-3 if m > 2 else 9)) + 2) // 5 + d - 1
    doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
    return era * 146097 + doe - 719468


def m_dt_parse(s):
    """Transcribe shm_dt_parse -> epoch microseconds, or None on malformed."""
    if s is None or len(s) < 19: return None
    if s[4] != '-' or s[7] != '-' or s[10] != 'T' or s[13] != ':' or s[16] != ':': return None
    def dig(sub):
        if not sub.isdigit(): return -1
        return int(sub)
    Y, Mo, D = dig(s[0:4]), dig(s[5:7]), dig(s[8:10])
    H, Mi, S = dig(s[11:13]), dig(s[14:16]), dig(s[17:19])
    if min(Y, Mo, D, H, Mi, S) < 0 or Mo < 1 or Mo > 12 or D < 1 or D > 31 or H > 23 or Mi > 59 or S > 59:
        return None
    pos = 19
    frac = 0
    if pos < len(s) and s[pos] == '.':
        pos += 1
        fd = 0; f = 0
        while pos < len(s) and s[pos].isdigit() and fd < 6:
            f = f * 10 + int(s[pos]); pos += 1; fd += 1
        if fd == 0: return None
        while fd < 6: f *= 10; fd += 1
        frac = f
    off = 0
    if pos < len(s) and s[pos] == 'Z':
        pos += 1
    elif pos < len(s) and s[pos] in '+-':
        sign = -1 if s[pos] == '-' else 1
        if pos + 6 > len(s) or s[pos + 3] != ':': return None
        oh, om = dig(s[pos + 1:pos + 3]), dig(s[pos + 4:pos + 6])
        if oh < 0 or om < 0 or oh > 23 or om > 59: return None
        off = sign * (oh * 60 + om) * 60000000
        pos += 6
    else:
        return None
    if pos != len(s): return None
    days = m_days_from_civil(Y, Mo, D)
    return (((days * 24 + H) * 60 + Mi) * 60 + S) * 1000000 + frac - off


INTERVAL_US = 86400000000
OSERROR_KINDS = {"OSError", "IOError", "EnvironmentError", "WindowsError", "BlockingIOError",
                 "ChildProcessError", "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
                 "ConnectionRefusedError", "ConnectionResetError", "FileExistsError", "FileNotFoundError",
                 "InterruptedError", "IsADirectoryError", "NotADirectoryError", "PermissionError",
                 "ProcessLookupError", "TimeoutError"}
BASE_KINDS = {"SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def m_type_name(v):
    if v is None: return "NoneType"
    if isinstance(v, bool): return "bool"
    if isinstance(v, int): return "int"
    if isinstance(v, float): return "int" if v.is_integer() else "float"
    if isinstance(v, str): return "str"
    if isinstance(v, list): return "list"
    if isinstance(v, dict): return "dict"
    return "object"


class MErr(Exception):
    """A C raise spec carried as a value (kind + message)."""
    def __init__(self, kind, msg):
        self.kind = kind; self.msg = msg
        super().__init__(msg)


def m_row_get(row, key):
    if isinstance(row, O.RowDouble):
        row = row._d
    if row is None: raise MErr("TypeError", "'NoneType' object is not subscriptable")
    if not isinstance(row, dict): raise MErr("TypeError", "'%s' object is not subscriptable" % m_type_name(row))
    if key not in row: raise MErr("IndexError", "No item with that key")
    return row[key]


def m_index_get(r, i):
    if not isinstance(r, list): raise MErr("TypeError", "'%s' object is not subscriptable" % m_type_name(r))
    if i >= len(r): raise MErr("IndexError", "list index out of range")
    return r[i]


def m_unpack2(result):
    if isinstance(result, list):
        n = len(result)
        if n == 2: return result[0], result[1]
        raise MErr("ValueError", "%s values to unpack (expected 2, got %d)" % ("too many" if n > 2 else "not enough", n))
    raise MErr("TypeError", "cannot unpack non-iterable %s object" % m_type_name(result))


# ── mirror seam helpers (build the SAME arg dicts the C builds) ─────────────
def call(name, args):
    return O.record(name, args)


def m_connection(bt):
    cm = call("store._connection", {"busy_timeout_ms": bt})
    return cm.value.spec   # raw connection handle (dict or None), like C


def m_write(st, bt):
    conn = m_connection(bt)
    call("write_txn", {"connection": conn})
    return conn


def m_db_execute(sql, params):
    return call("db.execute", {"sql": sql, "params": params})   # CursorDouble


def m_invoke_deferred(st, item, conn):
    if isinstance(item, dict) and item.get("__fn__") == "_update_rollup_state":
        return m_update_rollup_state_core(st, conn, item.get("state_key", ""), item.get("update"))
    name = item.get("__callable__") if isinstance(item, dict) else None
    if name is None:
        raise MErr("TypeError", "'NoneType' object is not callable")
    return call(name, {"args": [conn], "kwargs": {}})


def m_deferred_assign(st, a, b):
    merged = list(a) + list(b)
    if len(merged) > 1024:
        merged = merged[-1024:]
    st["_deferred"] = merged


SQL_SELECT_VALUE = "SELECT value FROM telemetry_state WHERE key = ?"
SQL_STATE_UPSERT = "INSERT INTO telemetry_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value"
SQL_UPDATE_STATE = "UPDATE telemetry_state SET value = ? WHERE key = ?"
SQL_DAILY_UPSERT = "INSERT INTO telemetry_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value WHERE telemetry_state.value != excluded.value"
SQL_SELECT_KEY_LIKE = "SELECT key FROM telemetry_state WHERE key LIKE ?"
SQL_INSERT_NOTHING = "INSERT INTO telemetry_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO NOTHING"
SQL_UPDATE_PACKAGED = "\n                    UPDATE counter_aggregates SET packaged_value = value\n                    WHERE period_start = ? AND metric_name = ? AND hermes_version = ?\n                      AND os_family = ? AND architecture = ? AND install_method = ?\n                      AND dimensions_json = ?\n                    "
RES_COLS = ["hermes_version", "os_family", "architecture", "install_method"]


def m_validate(metric, dims, res):
    call("store._validate_counter", {"metric_name": metric, "dimensions": dims, "resource": res})


def m_install_id(conn):
    call("store._install_id", {"connection": conn})


def m_record_txn(conn, metric, dims, res, period):
    call("store._record_counter_in_transaction",
         {"connection": conn, "metric_name": metric, "dimensions": dims, "resource": res, "period_start": period})


def m_iso(now):
    return call("_isoformat", {"value": now})


def m_parse_ts(val):
    return call("store._parse_state_timestamp", {"value": val})


def m_utc_now():
    return call("_utc_now", {})


# ── mirror ported functions ─────────────────────────────────────────────────
def m_latched_core(st, prefix):
    conn = m_connection(250)
    spec = m_db_execute(SQL_SELECT_KEY_LIKE, [prefix + "%"])
    allrows = spec.fetchall()
    suffixes = []
    for row in allrows:
        k = m_row_get(row, "key")
        if not isinstance(k, str):
            raise MErr("TypeError", "'%s' object is not subscriptable" % m_type_name(k))
        suf = k[len(prefix):] if len(k) >= len(prefix) else ""
        if suf not in suffixes:
            suffixes.append(suf)
    return sorted(suffixes)


def m_record_once_per_interval_core(st, state_key, metric, dims, res):
    m_validate(metric, dims, res)
    now = m_utc_now()
    conn = m_write(st, 250)
    spec = m_db_execute(SQL_SELECT_VALUE, [state_key])
    row = spec.fetchone()
    last = None
    if row is not None:
        val = m_row_get(row, "value")
        last = m_parse_ts(val)
    if last is not None:
        last_us = m_dt_parse(last.isoformat())
        now_us = m_dt_parse(now.isoformat())
        if last_us is None: raise MErr("ValueError", "Invalid isoformat string: '%s'" % last.isoformat())
        if now_us is None: raise MErr("ValueError", "Invalid isoformat string: '%s'" % now.isoformat())
        if last_us > now_us:
            iso = m_iso(now)
            m_db_execute(SQL_UPDATE_STATE, [iso, state_key])
            return False
        if now_us < last_us + INTERVAL_US:
            return False
    m_install_id(conn)
    m_record_txn(conn, metric, dims, res, now.isoformat()[:10])
    iso2 = m_iso(now)
    m_db_execute(SQL_STATE_UPSERT, [state_key, iso2])
    return True


def m_record_once_ever_core(st, state_key, metric, dims, res):
    m_validate(metric, dims, res)
    now = m_utc_now()
    conn = m_write(st, 250)
    iso = m_iso(now)
    spec = m_db_execute(SQL_INSERT_NOTHING, [state_key, iso])
    inserted = spec.rowcount
    if inserted == 0:
        return False
    m_install_id(conn)
    m_record_txn(conn, metric, dims, res, now.isoformat()[:10])
    return True


def m_update_rollup_state_core(st, conn, state_key, update):
    spec = m_db_execute(SQL_SELECT_VALUE, [state_key])
    row = spec.fetchone()
    state = None
    if row is not None:
        val = m_row_get(row, "value")
        if isinstance(val, str):
            try:
                state = json.loads(val)
            except (TypeError, ValueError):
                state = None
    name = update.get("__callable__") if isinstance(update, dict) else None
    if name is None:
        raise MErr("TypeError", "'NoneType' object is not callable")
    result = call(name, {"args": [state if isinstance(state, dict) else None], "kwargs": {}})
    ns, rows = m_unpack2(result)
    valid = []
    if not isinstance(rows, list):
        raise MErr("TypeError", "'%s' object is not iterable" % m_type_name(rows))
    for r in rows:
        m = m_index_get(r, 0); d = m_index_get(r, 1); res = m_index_get(r, 2)
        dv = call("contract.counter_dimensions_are_valid", {"metric_name": m, "dimensions": d})
        ok = bool(dv)
        if ok:
            rv = call("contract.client_resource_is_valid", {"resource": res})
            ok = bool(rv)
        if ok:
            valid.append(r)
    if valid:
        m_install_id(conn)
    for r in valid:
        if not isinstance(r, list) or len(r) != 4:
            if isinstance(r, list):
                n = len(r)
                raise MErr("ValueError", "%s values to unpack (expected 4, got %d)" % ("too many" if n > 4 else "not enough", n))
            raise MErr("TypeError", "cannot unpack non-iterable %s object" % m_type_name(r))
        m_record_txn(conn, r[0], r[1], r[2], r[3])
    cj = m_compact_json(ns)
    m_db_execute(SQL_STATE_UPSERT, [state_key, cj])
    return None


def m_write_or_defer_core(st, write_cb):
    pending = list(st.get("_deferred", [])) + [write_cb]
    st["_deferred"] = []
    try:
        conn = m_write(st, 250)
        for item in pending:
            m_invoke_deferred(st, item, conn)
    except Exception as exc:
        kind = type(exc).__name__
        if isinstance(exc, MErr):
            kind = exc.kind
        if kind != "OperationalError":
            raise
        msg = str(exc)
        busy = ("locked" in msg) or ("busy" in msg)
        keep = pending if busy else pending[:-1]
        m_deferred_assign(st, keep, st.get("_deferred", []))
        drain = st.get("_drain_at_exit", False)
        regist = not drain
        st["_drain_at_exit"] = True
        if regist:
            call("atexit.register", {"callback": {"__callable__": "store.drain_deferred"}})
            ds = st.setdefault("_DEFERRING_STORES", [])
            ds.append({"id": st.get("id", "")})
        if not busy:
            raise
        return None
    return None


def m_drain_deferred(st, bt):
    pending = list(st.get("_deferred", []))
    st["_deferred"] = []
    pending_n = len(pending)
    if pending_n == 0:
        return None
    try:
        conn = m_write(st, bt)
        for item in pending:
            m_invoke_deferred(st, item, conn)
    except BaseException as exc:
        kind = exc.kind if isinstance(exc, MErr) else type(exc).__name__
        if kind in BASE_KINDS:
            raise
        msg = str(exc)
        busy = (kind == "OperationalError") and (("locked" in msg) or ("busy" in msg))
        if busy:
            m_deferred_assign(st, pending, st.get("_deferred", []))
            return None
        call("logger.warning", {"msg": "Unable to write %d deferred shared-metrics updates",
                                "args": [pending_n], "exc_info": True})
        return None
    return None


def mirror_drive(func, inp, st):
    if func == "_compact_json":
        return m_compact_json(inp.get("value"))
    if func == "_row_resource":
        row = inp.get("row")
        return {c: m_row_get(row, c) for c in RES_COLS}
    if func == "_ensure_private":
        path = inp.get("path", ""); mode = inp.get("mode", 0)
        if mode == 448:
            call("path.mkdir", {"path": path, "parents": True, "exist_ok": True, "mode": mode})
        else:
            call("path.touch", {"path": path, "mode": mode, "exist_ok": True})
        try:
            call("path.chmod", {"path": path, "mode": mode})
        except Exception as exc:
            kind = exc.kind if isinstance(exc, MErr) else type(exc).__name__
            if kind not in OSERROR_KINDS:
                raise
        return None
    if func == "drain_deferring_stores":
        deadline = inp.get("deadline", 0)
        stores = st.get("_DEFERRING_STORES", [])
        for store in stores:
            mono = call("time.monotonic", {})
            m = mono if isinstance(mono, (int, float)) else 0
            remaining = int((deadline - m) * 1000)
            if remaining <= 0:
                return None
            call("store.drain_deferred", {"store_id": store.get("id", ""), "busy_timeout_ms": remaining})
        return None
    if func == "_write":
        m_write(st, inp.get("busy_timeout_ms", 250))
        return None
    if func == "install_snapshot_due":
        conn = m_connection(250)
        spec = m_db_execute(SQL_SELECT_VALUE, ["install_snapshot_recorded_at"])
        row = spec.fetchone()
        last = None
        if row is not None:
            val = m_row_get(row, "value")
            last = m_parse_ts(val)
        now = m_utc_now()
        if last is None:
            return True
        last_us = m_dt_parse(last.isoformat()); now_us = m_dt_parse(now.isoformat())
        if last_us is None: raise MErr("ValueError", "Invalid isoformat string: '%s'" % last.isoformat())
        if now_us is None: raise MErr("ValueError", "Invalid isoformat string: '%s'" % now.isoformat())
        return last_us > now_us or now_us >= last_us + INTERVAL_US
    if func == "_record_once_per_interval":
        return m_record_once_per_interval_core(st, inp.get("state_key", ""), inp.get("metric_name", ""),
                                               inp.get("dimensions"), inp.get("resource"))
    if func == "record_install_snapshot":
        return m_record_once_per_interval_core(st, "install_snapshot_recorded_at", "hermes.install.snapshot",
                                               inp.get("dimensions"), inp.get("resource"))
    if func == "_record_once_ever":
        return m_record_once_ever_core(st, inp.get("state_key", ""), inp.get("metric_name", ""),
                                       inp.get("dimensions"), inp.get("resource"))
    if func == "record_milestone":
        ms = inp.get("milestone", "")
        dims = {"install_age_bucket": inp.get("install_age_bucket", ""), "milestone": ms}
        return m_record_once_ever_core(st, "milestone:" + ms, "hermes.install.milestone", dims, inp.get("resource"))
    if func == "record_feature_adoption":
        f = inp.get("feature", "")
        dims = {"days_since_install_bucket": inp.get("days_bucket", ""), "feature": f}
        return m_record_once_ever_core(st, "feature:" + f, "hermes.feature_adoption.count", dims, inp.get("resource"))
    if func == "_latched":
        return m_latched_core(st, inp.get("prefix", ""))
    if func == "recorded_milestones":
        return m_latched_core(st, "milestone:")
    if func == "recorded_features":
        return m_latched_core(st, "feature:")
    if func == "record_counter_once_per_day":
        metric = inp.get("metric_name", ""); dims = inp.get("dimensions"); res = inp.get("resource")
        m_validate(metric, dims, res)
        now = m_utc_now()
        if not isinstance(now, str) and now is not None:
            pass
        if now is None:
            raise MErr("AttributeError", "'NoneType' object has no attribute 'date'")
        today = now.isoformat()[:10]
        dn = {k: v for k, v in (dims or {}).items() if k != "surface"}
        key = "daily_once:" + metric + ":" + m_compact_json(dn)
        conn = m_write(st, 250)
        spec = m_db_execute(SQL_DAILY_UPSERT, [key, today])
        changed = spec.rowcount
        if changed == 0:
            return False
        m_install_id(conn)
        m_record_txn(conn, metric, dims, res, today)
        return True
    if func == "_update_rollup_state":
        return m_update_rollup_state_core(st, inp.get("connection"), inp.get("state_key", ""), inp.get("update"))
    if func == "_write_or_defer":
        return m_write_or_defer_core(st, inp.get("write"))
    if func == "update_rollup_state":
        cb = {"__fn__": "_update_rollup_state", "state_key": inp.get("state_key", ""), "update": inp.get("update")}
        return m_write_or_defer_core(st, cb)
    if func == "drain_deferred":
        return m_drain_deferred(st, inp.get("busy_timeout_ms", 5000))
    if func == "_mark_packaged":
        conn = inp.get("connection"); rows = inp.get("rows") or []
        pv = inp.get("period_value", ""); rv = inp.get("resource_values") or []
        for row in rows:
            mn = m_row_get(row, "metric_name"); dj = m_row_get(row, "dimensions_json")
            params = [pv, mn] + list(rv) + [dj]
            m_db_execute(SQL_UPDATE_PACKAGED, params)
        return None
    if func == "_package_metrics":
        rows = inp.get("rows") or []
        merged = []; keys = []
        for row in rows:
            metric = call("store._package_metric", {"row": row})
            if metric is None:
                continue
            if "name" not in metric: raise MErr("KeyError", "'name'")
            if "dimensions" not in metric: raise MErr("KeyError", "'dimensions'")
            name = metric["name"]
            cj = m_compact_json(metric["dimensions"])
            key = "%d:%s%s" % (len(name) if isinstance(name, str) else 0, name if isinstance(name, str) else "", cj)
            if key in keys:
                at = keys.index(key)
                ev = merged[at].get("value"); mv = metric.get("value")
                if ev is None or mv is None: raise MErr("KeyError", "'value'")
                if not isinstance(ev, (int, float)) or not isinstance(mv, (int, float)):
                    raise MErr("TypeError", "unsupported operand type(s) for +=: '%s' and '%s'" % (m_type_name(ev), m_type_name(mv)))
                merged[at]["value"] = ev + mv
            else:
                keys.append(key); merged.append(metric)
        return merged
    raise RuntimeError("unknown func " + func)


def mirror_run_case(func, inp):
    inp = json.loads(json.dumps(inp))   # deepcopy
    ctx = O.CaseCtx(inp)
    O.CURRENT["ctx"] = ctx
    st = inp.get("state") or {}
    O.install_deferring_stores(st)
    try:
        ret = mirror_drive(func, inp, st)
        return {"ret": O.norm_value(ret), "trace": ctx.trace, "st": mirror_proj_state(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec; break
        if spec is None:
            if isinstance(e, MErr):
                spec = {"__raise__": e.kind, "message": e.msg}
            else:
                spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": mirror_proj_state(st)}


def mirror_proj_state(st):
    out = {}
    for k in st:
        if k == "_DEFERRING_STORES":
            out[k] = [{"id": s.get("id", "")} if isinstance(s, dict) else {"id": getattr(s, "id", "")} for s in st[k]]
        elif k == "_deferred":
            out[k] = [proj_double(x) for x in st[k]]
        else:
            out[k] = O.norm_value(st[k])
    return out


def proj_double(x):
    if isinstance(x, dict) and ("__callable__" in x or "__fn__" in x):
        return x
    return O.proj_callable(x)


def main():
    recs = [json.loads(l) for l in FX.read_text(encoding="utf-8").splitlines() if l.strip()]
    n = mism = 0
    for i, rec in enumerate(recs):
        func = rec["func"]; inp = rec.get("in") or {}
        n += 1
        want = O.run_case(func, inp)
        got = mirror_run_case(func, inp)
        got = O.norm_value(got)
        if got != want:
            mism += 1
            if mism <= 20:
                print("SHADOW MISMATCH [%s] case %d" % (func, i))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s MIRROR: %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s LIVE  : %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("SHADOW shared_metrics_flow: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
