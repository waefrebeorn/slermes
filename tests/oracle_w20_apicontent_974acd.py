#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/api_content.py
(gate w20_apicontent_974acd).

The live module is imported for real; its imports resolve from the quarry. Each
harness line is replayed through the live writer with a capturing holder, so the
compared values are exactly what the live function handed to the store: the
statement text, the parameter tuple (api_content scrubbed, content encoded via
the module's own _encode_content) and the returned rowcount -- or the bare 0 the
row-id guard answers without a write.
"""
import importlib.util
import json
import os
import sys

sys.dont_write_bytecode = True
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_apicontent_974acd_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "sqlite", "conversation", "_store", "api_content.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Holder:
    """Stand-in for a StoreHolder: records the write and reports a rowcount."""

    def __init__(self, rowcount):
        self.rowcount = rowcount
        self.calls = []

    def _write_rowcount(self, sql, params):
        self.calls.append((sql, list(params)))
        return self.rowcount


def wire(v):
    """Representation contract: a NUL-led (reserved-prefix) string cannot ride in
    a C string, so both sides carry it as {"__prefixed__": <rest after the NUL>}."""
    if isinstance(v, str) and v.startswith("\x00"):
        return {"__prefixed__": v[1:]}
    if isinstance(v, dict):
        return {k: wire(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [wire(x) for x in v]
    return v


def replay(rowcount, call):
    holder = Holder(rowcount)
    result = call(holder)
    out = {"rowcount": result}
    if holder.calls:
        sql, params = holder.calls[-1]
        out["sql"] = sql
        out["params"] = params
    return wire(out)


def expected(r):
    f = r.get("func")
    rc = r.get("rowcount", 1)
    if f == "plan_latest":
        return replay(rc, lambda h: live.set_latest_user_api_content(
            h, r.get("session_id"), r.get("content"), r.get("api_content")))
    if f == "plan_row":
        return replay(rc, lambda h: live.set_message_api_content(
            h, r.get("session_id"), r.get("row_id"), r.get("content"), r.get("api_content")))
    if f == "encode_content":
        return wire(live._encode_content(r.get("content")))
    if f == "latest_sql":
        return replay(1, lambda h: live.set_latest_user_api_content(h, "s", None, None))["sql"]
    if f == "row_sql":
        return replay(1, lambda h: live.set_message_api_content(h, "s", 1, None, None))["sql"]
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_apicontent_974acd: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
