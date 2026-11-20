#!/usr/bin/env python3
"""Generate tests/fixtures/store_identity_flow.jsonl — input cases for the
conversation-store message-identity codec FLOW oracle.

Defining module: persistence/sqlite/conversation/_store/identity.py (12 defs)
plus the 2 unclaimed pure leaves (_json_or @ persistence/sqlite/_state/common.py,
coalesce_tool_call_id @ session_rules/turns.py). The oracle computes want from
the LIVE quarry defs at gate time; this script only emits {func, in} inputs.

To guarantee the scripted seams match the live call sequence (no script gaps),
each case is built by driving the live reference ONCE with an auto-recording
context: a provider supplies each collaborator's return (sensible defaults plus
per-case overrides), and the recorded returns become in.io. The gate replays
the identical scripts through the oracle's Ctx and the C universal seam.

Override shapes: a plain value (returned on every call), a {"__script__": ...}
dict (ret/raise), or a Python LIST meaning one return per call (recorded as the
fixture's sequence script). uid_list's live return is itself a list, so its
recorded script is a sequence OF lists (one level deeper than a bare array).

Representation contract (shared with the C port and the oracle):
- None == JSON null; plain JSON numbers are Python ints; JSON bools are bools.
- A Python float travels as {"__float__": true, "v": <number>}; a tuple as
  {"__tuple__": true, "v": [...]}.
- Fixture domain (ASCII scope): strings ASCII (control chars ride \\u00xx
  escapes); uids non-empty ASCII strings; uid_list seam results lists of
  non-empty ASCII strings; uid-map values non-empty str or non-empty list of
  non-empty str; JSON texts within depth 32, int magnitude <= 2^53, finite
  floats; NO NaN/Infinity literals, NO lone-surrogate \\u escapes, NO
  bytes/bytearray, NO non-ASCII whitespace in strip positions, NO
  quote/backslash KeyError keys.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_store_identity_flow as oracle  # noqa: E402

OUT = HERE / "fixtures" / "store_identity_flow.jsonl"
CASES = []


def fl(v):
    return {"__float__": True, "v": v}


def tu(items):
    return {"__tuple__": True, "v": list(items)}


def script(ret=None, raise_spec=None):
    d = {"__script__": True}
    if raise_spec is not None:
        d["raise"] = raise_spec
    else:
        d["ret"] = ret
    return d


def raise_spec(type, message=""):
    return {"type": type, "message": message}


def real_uid_list(value):
    """The claimed sibling's live semantics (session_rules/turns.py:uid_list),
    used only to supply realistic default scripts."""
    if not isinstance(value, list):
        return []
    return list(dict.fromkeys(u for u in value if isinstance(u, str) and u))


def default_return(name, idx, args):
    if name == "uid_list":
        return real_uid_list(oracle.deep_mat(args.get("value")))
    if name == "logger.warning":
        return None
    return None


class AutoCtx:
    def __init__(self, provider):
        self.provider = provider
        self.cursors = {}
        self.recorded = {}
        self.trace = []

    def call(self, name, args):
        idx = self.cursors.get(name, 0)
        self.cursors[name] = idx + 1
        ret = self.provider(name, idx, args)
        self.recorded.setdefault(name, []).append(ret)
        self.trace.append({"name": name, "args": args, "ret": ret})
        if isinstance(ret, dict) and ret.get("__script__"):
            if isinstance(ret.get("raise"), dict):
                raise oracle.make_exc(ret["raise"])
            return copy.deepcopy(ret.get("ret"))
        return ret


def build_provider(overrides):
    overrides = overrides or {}

    def provider(name, idx, args):
        if name in overrides:
            ov = overrides[name]
            if isinstance(ov, list):
                return ov[idx] if idx < len(ov) else ov[-1]
            return ov
        return default_return(name, idx, args)
    return provider


def add(func, args, overrides=None, fail_closed=False, io=None):
    if fail_closed:
        inp = {"args": args}
        if io:
            inp["io"] = io
        CASES.append({"func": func, "in": inp, "fail_closed": True})
        return
    ctx = AutoCtx(build_provider(overrides))
    oracle.drive(func, {"args": args}, ctx=ctx)
    inp = {"args": args}
    if ctx.recorded:
        inp["io"] = ctx.recorded
    CASES.append({"func": func, "in": inp})


# ── _live_or_column ──
LK, COL = "_tool_call_uids", "tool_call_uids"
add("_live_or_column", {"msg": {LK: {"a": "u"}, COL: {"b": "v"}}, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": {COL: {"b": "v"}}, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": {}, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": {LK: None, COL: "col"}, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": {COL: None}, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": None, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": 5, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": "abc", "live_key": LK, "column": COL})
add("_live_or_column", {"msg": [1], "live_key": LK, "column": COL})
add("_live_or_column", {"msg": tu((1,)), "live_key": LK, "column": COL})
add("_live_or_column", {"msg": fl(1.5), "live_key": LK, "column": COL})
add("_live_or_column", {"msg": True, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": {LK: ""}, "live_key": LK, "column": COL})
add("_live_or_column", {"msg": {LK: [], COL: ["x"]}, "live_key": LK, "column": COL})

# ── _uid_list ──
add("_uid_list", {"value": ["a", "b", "a", "", "c", 5, None]})
add("_uid_list", {"value": []})
add("_uid_list", {"value": None})
add("_uid_list", {"value": 5})
add("_uid_list", {"value": tu(("a", "b"))})
add("_uid_list", {"value": {"a": 1}})
add("_uid_list", {"value": '["a","b","a"]'})
add("_uid_list", {"value": "[1,2]"})
add("_uid_list", {"value": "bad json"})
add("_uid_list", {"value": '{"a":1}'})
add("_uid_list", {"value": ""})
add("_uid_list", {"value": "null"})
add("_uid_list", {"value": "5"})
add("_uid_list", {"value": "5.0"})
add("_uid_list", {"value": ["a"]}, fail_closed=True)
add("_uid_list", {"value": ["a"]},
    overrides={"uid_list": script(raise_spec=raise_spec("RuntimeError", "uid boom"))})
add("_uid_list", {"value": ["a"]},
    overrides={"uid_list": script(raise_spec=raise_spec("SystemExit", "bye"))})
add("_uid_list", {"value": '["a\\u0001b"]'})
add("_uid_list", {"value": '  ["a"]  '})
add("_uid_list", {"value": "\v5"})
add("_uid_list", {"value": '["x", "x", "x"]'})
add("_uid_list", {"value": [True, "t"]})

# ── _uid_map ──
add("_uid_map", {"value": {"a": "u1", "b": ["u2", "u3"], "c": "", "d": [], "e": [1],
                           "f": ["x", ""], "g": 5, "h": True, "i": {}, "j": None, "": "u"}})
add("_uid_map", {"value": {}})
add("_uid_map", {"value": None})
add("_uid_map", {"value": [1]})
add("_uid_map", {"value": 5})
add("_uid_map", {"value": tu(("a",))})
add("_uid_map", {"value": '{"a":"u1","b":["u2"]}'})
add("_uid_map", {"value": "bad"})
add("_uid_map", {"value": "[1]"})
add("_uid_map", {"value": '{"a":5,"b":"u"}'})
add("_uid_map", {"value": '{"a":["u","u"]}'})
add("_uid_map", {"value": {"a": tu(("u",))}})
add("_uid_map", {"value": "bad"}, fail_closed=True)
add("_uid_map", {"value": "bad"},
    overrides={"logger.warning": script(raise_spec=raise_spec("ValueError", "log boom"))})
add("_uid_map", {"value": '{"a":"u","a":"v"}'})
add("_uid_map", {"value": '{"": "u", "a": "v"}'})
add("_uid_map", {"value": '{"a": []}'})
add("_uid_map", {"value": '{"a": ["x", 5]}'})
add("_uid_map", {"value": '{"a": "u", "b": {"c": "v"}}'})
add("_uid_map", {"value": '{"a": false}'})
add("_uid_map", {"value": {"a": "u", "b": ["v"], "c": [None]}})

# ── _tool_call_uid_map ──
add("_tool_call_uid_map", {"msg": {LK: {"a": "u"}}})
add("_tool_call_uid_map", {"msg": {COL: {"b": "v"}}})
add("_tool_call_uid_map", {"msg": {}})
add("_tool_call_uid_map", {"msg": {LK: "bad", COL: {"x": "y"}}})
add("_tool_call_uid_map", {"msg": None})
add("_tool_call_uid_map", {"msg": {LK: None}})
add("_tool_call_uid_map", {"msg": {LK: '{"a":"u"}'}})
add("_tool_call_uid_map", {"msg": "s"})
add("_tool_call_uid_map", {"msg": {LK: {"a": ""}, COL: {"b": "v"}}})

# ── _absorbed_uids_json ──
AK, ACOL = "_absorbed_message_uids", "absorbed_message_uids"
add("_absorbed_uids_json", {"msg": {AK: ["u1", "u2"]}})
add("_absorbed_uids_json", {"msg": {ACOL: ["u3"]}})
add("_absorbed_uids_json", {"msg": {}})
add("_absorbed_uids_json", {"msg": {AK: []}})
add("_absorbed_uids_json", {"msg": {AK: '["u1","u1","u2"]'}})
add("_absorbed_uids_json", {"msg": {AK: "bad"}})
add("_absorbed_uids_json", {"msg": {AK: ["a\x01b", "c\"d", "e\\f"]}})
add("_absorbed_uids_json", {"msg": None})
add("_absorbed_uids_json", {"msg": {AK: ["u"]}},
    overrides={"uid_list": script(raise_spec=raise_spec("RuntimeError", "uid boom"))})
add("_absorbed_uids_json", {"msg": {AK: ["x"], ACOL: ["y"]}})
add("_absorbed_uids_json", {"msg": {AK: '["\\u007f"]'}})
add("_absorbed_uids_json", {"msg": {AK: ["u"]}}, fail_closed=True)
add("_absorbed_uids_json", {"msg": {AK: ["\t\n\r\b\f"]}})

# ── _tool_call_uids_json ──
add("_tool_call_uids_json", {"msg": {LK: {"b": "u2", "a": "u1"}}})
add("_tool_call_uids_json", {"msg": {LK: {"a": ["u1", "u2"]}}})
add("_tool_call_uids_json", {"msg": {}})
add("_tool_call_uids_json", {"msg": {LK: {}}})
add("_tool_call_uids_json", {"msg": {LK: {"a": "", "b": "u"}}})
add("_tool_call_uids_json", {"msg": {COL: '{"z":"u9","A":"u0"}'}})
add("_tool_call_uids_json", {"msg": {LK: "bad"}})
add("_tool_call_uids_json", {"msg": {LK: {"a": "x\x01y"}}})
add("_tool_call_uids_json", {"msg": None})
add("_tool_call_uids_json", {"msg": {LK: {"B": "u", "a": "v", "A": "w"}}})
add("_tool_call_uids_json", {"msg": {LK: "bad"}}, fail_closed=True)
add("_tool_call_uids_json", {"msg": {LK: {"a": ["x\"y", "z\\w"]}}})

# ── _tool_call_uid_or_none ──
TK, TCOL = "_tool_call_uid", "tool_call_uid"
add("_tool_call_uid_or_none", {"msg": {TK: "u"}})
add("_tool_call_uid_or_none", {"msg": {TCOL: "v"}})
add("_tool_call_uid_or_none", {"msg": {}})
add("_tool_call_uid_or_none", {"msg": {TK: ""}})
add("_tool_call_uid_or_none", {"msg": {TK: 5}})
add("_tool_call_uid_or_none", {"msg": {TK: None}})
add("_tool_call_uid_or_none", {"msg": None})
add("_tool_call_uid_or_none", {"msg": {TK: "u", TCOL: "v"}})
add("_tool_call_uid_or_none", {"msg": {TK: ["u"]}})
add("_tool_call_uid_or_none", {"msg": {TK: True}})

# ── _live_extends ──
add("_live_extends", {"live_uid": ["u1", "u2"], "stored_uid": "u1"})
add("_live_extends", {"live_uid": ["u1", "u2"], "stored_uid": ["u1", "u2"]})
add("_live_extends", {"live_uid": ["u1"], "stored_uid": ["u1", "u2"]})
add("_live_extends", {"live_uid": ["u2"], "stored_uid": "u1"})
add("_live_extends", {"live_uid": "u1", "stored_uid": "u1"})
add("_live_extends", {"live_uid": None, "stored_uid": "u1"})
add("_live_extends", {"live_uid": tu(("u1",)), "stored_uid": "u1"})
add("_live_extends", {"live_uid": ["u1", "u2"], "stored_uid": []})
add("_live_extends", {"live_uid": [], "stored_uid": "u1"})
add("_live_extends", {"live_uid": [1, 2], "stored_uid": fl(1.0)})
add("_live_extends", {"live_uid": [True], "stored_uid": 1})
add("_live_extends", {"live_uid": ["u1", "u2"], "stored_uid": None})
add("_live_extends", {"live_uid": [None], "stored_uid": None})
add("_live_extends", {"live_uid": [{"a": 1}], "stored_uid": {"a": 1}})
add("_live_extends", {"live_uid": [[1]], "stored_uid": tu((1,))})
add("_live_extends", {"live_uid": ["u1", "u2", "u3"], "stored_uid": ["u1", "u2"]})
add("_live_extends", {"live_uid": [fl(1.5)], "stored_uid": fl(1.5)})
add("_live_extends", {"live_uid": {"a": 1}, "stored_uid": "a"})
add("_live_extends", {"live_uid": [], "stored_uid": []})
add("_live_extends", {"live_uid": ["u1"], "stored_uid": tu(("u1",))})

# ── _fill_missing_tool_call_uids ──
def row_tcu(v):
    return {"tool_call_uids": v}


add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u1", "b": "u2"}), "msg": {"tool_calls": [{"id": "a"}, {"id": "b"}]}})
add("_fill_missing_tool_call_uids", {"row": row_tcu({}), "msg": {"tool_calls": [{"id": "a"}]}})
add("_fill_missing_tool_call_uids", {"row": row_tcu(None), "msg": {}})
add("_fill_missing_tool_call_uids", {"row": row_tcu({"a": "u"}), "msg": {"tool_calls": "not-list"}})
add("_fill_missing_tool_call_uids", {"row": row_tcu({"a": "u"}), "msg": None})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u", "b": "u2"}),
     "msg": {"tool_calls": [{"id": "a"}], LK: {"a": "live"}}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u1"}), "msg": {"tool_calls": [{"id": "a"}], LK: {"z": "keep"}}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu('{"a":"u1"}'), "msg": {"tool_calls": [{"id": "a"}]}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu("bad"), "msg": {"tool_calls": [{"id": "a"}]}})
add("_fill_missing_tool_call_uids", {"row": {}, "msg": {"tool_calls": [{"id": "a"}]}})
add("_fill_missing_tool_call_uids", {"row": None, "msg": {}})
add("_fill_missing_tool_call_uids", {"row": [1], "msg": {}})
add("_fill_missing_tool_call_uids", {"row": "s", "msg": {}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u"}), "msg": {"tool_calls": [{"call_id": "x|y", "id": "a"}]}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"x": "u"}), "msg": {"tool_calls": [{"call_id": "x|y"}]}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u"}), "msg": {"tool_calls": ["a", None, 5]}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"": "u", "a": "v"}), "msg": {"tool_calls": [{"id": "a"}]}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": ["u1", "u2"]}), "msg": {"tool_calls": [{"id": "a"}, {"id": "a"}]}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu("bad"), "msg": {"tool_calls": [{"id": "a"}]}},
    overrides={"logger.warning": script(raise_spec=raise_spec("RuntimeError", "log boom"))})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u"}), "msg": {"tool_calls": tu(({"id": "a"},))}})
add("_fill_missing_tool_call_uids",
    {"row": row_tcu("bad"), "msg": {"tool_calls": [{"id": "a"}]}}, fail_closed=True)
add("_fill_missing_tool_call_uids",
    {"row": row_tcu({"a": "u"}), "msg": {"tool_calls": [{"id": "a"}], LK: "not-dict"}})

# ── _restore_row_identity ──
def row3(mu, tcu, tcuid):
    return {"message_uid": mu, "tool_call_uids": tcu, "tool_call_uid": tcuid}


R_FULL = row3("m", {"a": "u1"}, "t")
add("_restore_row_identity", {"row": R_FULL, "msg": {}})
add("_restore_row_identity", {"row": row3("", {}, ""), "msg": {"x": 1}})
add("_restore_row_identity", {"row": R_FULL, "msg": {LK: {"a": ["u1", "u2"]}}})
add("_restore_row_identity", {"row": R_FULL, "msg": {LK: {"a": ["other"]}}})
add("_restore_row_identity", {"row": row3(None, {"a": "u"}, None), "msg": {LK: "not-dict"}})
add("_restore_row_identity", {"row": row3("m", {}, "t"), "msg": {}})
add("_restore_row_identity", {"row": row3("m", "bad", "t"), "msg": {}})
add("_restore_row_identity", {"row": {"message_uid": "m"}, "msg": {}})
add("_restore_row_identity", {"row": row3("m", {"a": "u"}, None), "msg": {}})
add("_restore_row_identity", {"row": R_FULL, "msg": None})
add("_restore_row_identity", {"row": row3("", {"a": "u"}, ""), "msg": None})
add("_restore_row_identity",
    {"row": R_FULL, "msg": {"message_uid": "old", LK: {"z": "keep"}, TK: "oldt"}})
add("_restore_row_identity", {"row": row3("m", '{"a":"u"}', "t"), "msg": {}})
add("_restore_row_identity", {"row": R_FULL, "msg": 5})
add("_restore_row_identity", {"row": R_FULL, "msg": {LK: {"a": ["u1", "u2"], "b": "keep"}}})
add("_restore_row_identity", {"row": row3("m", "bad", "t"), "msg": {}},
    overrides={"logger.warning": script(raise_spec=raise_spec("SystemExit", "log exit"))})
add("_restore_row_identity", {"row": row3(0, 0, 0), "msg": {"k": "v"}})
add("_restore_row_identity",
    {"row": row3("m", {"a": "u", "b": "v"}, "t"), "msg": {LK: {"a": ["u"]}}})
add("_restore_row_identity", {"row": row3("m", {"a": "u"}, "t"), "msg": {LK: {}}})
add("_restore_row_identity", {"row": row3("m", "bad", "t"), "msg": {}}, fail_closed=True)

# ── _restore_identity_columns ──
def row4(mu, tcu, tcuid, am):
    return {"message_uid": mu, "tool_call_uids": tcu, "tool_call_uid": tcuid,
            "absorbed_message_uids": am}


R4 = row4("m", {"a": "u"}, "t", ["x", "y"])
add("_restore_identity_columns", {"row": R4, "msg": {}})
add("_restore_identity_columns", {"row": row4("m", {"a": "u"}, "t", []), "msg": {}})
add("_restore_identity_columns", {"row": row4("m", {}, "t", '["x","x","y"]'), "msg": {}})
add("_restore_identity_columns", {"row": row4("m", {}, "t", "bad"), "msg": {}})
add("_restore_identity_columns", {"row": row3("m", {}, "t"), "msg": {}})
add("_restore_identity_columns", {"row": R4, "msg": None})
add("_restore_identity_columns", {"row": R4, "msg": {}},
    overrides={"uid_list": script(raise_spec=raise_spec("RuntimeError", "uid boom"))})
add("_restore_identity_columns", {"row": row4("m", {}, "t", None), "msg": {}})
add("_restore_identity_columns",
    {"row": row4("", {}, "", ["x"]), "msg": {AK: ["old"]}})
add("_restore_identity_columns", {"row": R4, "msg": {"message_uid": "old"}})
add("_restore_identity_columns", {"row": R4, "msg": {}}, fail_closed=True)
add("_restore_identity_columns", {"row": R4, "msg": {}},
    overrides={"uid_list": script(raise_spec=raise_spec("KeyboardInterrupt", "kb"))})
add("_restore_identity_columns", {"row": row4("m", {}, "t", '["\\u0001"]'), "msg": {}})

# ── _stable_tool_key ──
def row_st(tool_calls, role="assistant", ts=100, tcid="t", tn="f", **extra):
    r = {"role": role, "tool_calls": tool_calls, "timestamp": ts,
         "tool_call_id": tcid, "tool_name": tn}
    r.update(extra)
    return r


add("_stable_tool_key", {"row": row_st(None, role="user")})
add("_stable_tool_key", {"row": row_st([{"id": "a"}, {"id": "b"}])})
add("_stable_tool_key", {"row": row_st([])})
add("_stable_tool_key", {"row": row_st(None)})
add("_stable_tool_key", {"row": row_st('[{"id":"a"}]')})
add("_stable_tool_key", {"row": row_st("bad")})
add("_stable_tool_key", {"row": row_st([{"id": ""}])})
add("_stable_tool_key", {"row": row_st([{"id": "a"}, {"id": ""}])})
add("_stable_tool_key", {"row": row_st([{"call_id": "x|y"}])})
add("_stable_tool_key", {"row": row_st(5)})
add("_stable_tool_key", {"row": row_st([1])})
add("_stable_tool_key", {"row": row_st("5")})
add("_stable_tool_key", {"row": row_st("5.0")})
add("_stable_tool_key", {"row": row_st("true")})
add("_stable_tool_key", {"row": row_st('"ab"')})
add("_stable_tool_key", {"row": row_st('{"a":1}')})
add("_stable_tool_key", {"row": row_st("0")})
add("_stable_tool_key", {"row": row_st("null")})
add("_stable_tool_key", {"row": {"role": "assistant"}})
add("_stable_tool_key", {"row": {}})
add("_stable_tool_key", {"row": None})
add("_stable_tool_key", {"row": {"role": "assistant", "tool_calls": [{"id": "a"}]}})
add("_stable_tool_key", {"row": {"role": "assistant", "tool_calls": [{"id": "a"}], "timestamp": 1}})
add("_stable_tool_key",
    {"row": {"role": "assistant", "tool_calls": [{"id": "a"}], "timestamp": 1, "tool_call_id": "t"}})
add("_stable_tool_key", {"row": row_st([{"id": "a"}], ts=fl(1.5), tcid=None)})
add("_stable_tool_key", {"row": row_st("bad")}, fail_closed=True)
add("_stable_tool_key", {"row": row_st("bad")},
    overrides={"logger.warning": script(raise_spec=raise_spec("SystemExit", "log exit"))})
add("_stable_tool_key", {"row": row_st([{"id": "  a  "}])})
add("_stable_tool_key", {"row": row_st([{"id": "|lead"}])})
add("_stable_tool_key", {"row": row_st("")})
add("_stable_tool_key", {"row": row_st(0)})
add("_stable_tool_key", {"row": row_st('[{"id":"a"},{"call_id":"b|c","id":"d"}]')})
add("_stable_tool_key", {"row": row_st('[null,5,"x"]')})
add("_stable_tool_key", {"row": row_st('["a"]')})
add("_stable_tool_key", {"row": row_st("1e2")})
add("_stable_tool_key", {"row": row_st([{"id": "a"}], ts=0, tcid="", tn="")})

# ── _json_or ──
add("_json_or", {"raw": "[1,2]", "fallback": [], "warning": "w"})
add("_json_or", {"raw": "bad", "fallback": [], "warning": "w"})
add("_json_or", {"raw": 5, "fallback": {}, "warning": "w"})
add("_json_or", {"raw": None, "fallback": [], "warning": "w"})
add("_json_or", {"raw": '{"a":1}', "fallback": {}, "warning": "w"})
add("_json_or", {"raw": "5.0", "fallback": [], "warning": "w"})
add("_json_or", {"raw": "", "fallback": [], "warning": "w"})
add("_json_or", {"raw": "bad", "fallback": [], "warning": "w"},
    overrides={"logger.warning": script(raise_spec=raise_spec("RuntimeError", "log boom"))})
add("_json_or", {"raw": "bad", "fallback": [], "warning": "w"}, fail_closed=True)
add("_json_or", {"raw": "null", "fallback": [1], "warning": "w"})
add("_json_or", {"raw": [1], "fallback": [], "warning": "w"})
add("_json_or", {"raw": '  {"a": [1, 2.5, "x"]}  ', "fallback": {}, "warning": "w"})
add("_json_or", {"raw": "01", "fallback": [], "warning": "w"})
add("_json_or", {"raw": '"\\u0041"', "fallback": [], "warning": "w"})
add("_json_or", {"raw": "[1,]", "fallback": [], "warning": "w"})
add("_json_or", {"raw": "bad", "fallback": [], "warning": "w"},
    overrides={"logger.warning": script(raise_spec=raise_spec("KeyboardInterrupt", "kb"))})
add("_json_or", {"raw": '{"a":1,"a":2}', "fallback": {}, "warning": "w"})
add("_json_or", {"raw": '"\\ud83d\\ude00"', "fallback": [], "warning": "w"})

# ── coalesce_tool_call_id ──
add("coalesce_tool_call_id", {"tc": {"id": "a"}})
add("coalesce_tool_call_id", {"tc": {"call_id": "b"}})
add("coalesce_tool_call_id", {"tc": {"call_id": "x|y", "id": "z"}})
add("coalesce_tool_call_id", {"tc": {"id": "  c  "}})
add("coalesce_tool_call_id", {"tc": {"id": ""}})
add("coalesce_tool_call_id", {"tc": {"call_id": "|lead"}})
add("coalesce_tool_call_id", {"tc": {"call_id": "  ", "id": "d"}})
add("coalesce_tool_call_id", {"tc": None})
add("coalesce_tool_call_id", {"tc": "str"})
add("coalesce_tool_call_id", {"tc": 5})
add("coalesce_tool_call_id", {"tc": [1]})
add("coalesce_tool_call_id", {"tc": {"id": 5}})
add("coalesce_tool_call_id", {"tc": {"id": "a|b|c"}})
add("coalesce_tool_call_id", {"tc": {"id": " |x"}})
add("coalesce_tool_call_id", {"tc": {}})
add("coalesce_tool_call_id", {"tc": {"call_id": "\x1ca\x1c"}})
add("coalesce_tool_call_id", {"tc": {"id": "a| "}})
add("coalesce_tool_call_id", {"tc": {"id": " | "}})
add("coalesce_tool_call_id", {"tc": tu((1,))})
add("coalesce_tool_call_id", {"tc": {"call_id": None, "id": "e"}})
add("coalesce_tool_call_id", {"tc": {"call_id": "a|", "id": "z"}})
add("coalesce_tool_call_id", {"tc": fl(1.5)})
add("coalesce_tool_call_id", {"tc": True})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    missing = [k for k in oracle.KINDS if counts.get(k, 0) < 5]
    if missing:
        print(f"FATAL gen_store_identity_flow: funcs with <5 cases: {missing}")
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"gen_store_identity_flow: {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
