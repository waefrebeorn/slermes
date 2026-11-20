#!/usr/bin/env python3
"""Generate tests/fixtures/store_rules_flow.jsonl — input cases for the
conversation-store compaction-rules FLOW oracle.

Defining module: persistence/sqlite/conversation/_store/rules.py (11 defs) plus
the 6 unclaimed uid/typed-ask leaves of session_rules/turns.py. The oracle
computes want from the LIVE quarry defs at gate time; this script only emits
{func, in} inputs.

To guarantee the scripted seams match the live call sequence (no script gaps),
each case is built by driving the live reference ONCE with an auto-recording
context: a provider supplies each collaborator's return (sensible defaults plus
per-case overrides), and the recorded returns become in.io. The gate replays
the identical scripts through the oracle's Ctx and the C universal seam.

Override shapes: a plain value (returned on every call), a {"__script__": ...}
dict (ret/raise), or a Python LIST meaning one return per call (recorded as the
fixture's sequence script). No seam returns a bare JSON list as a single result
in this port, so the list-as-sequence convention is unambiguous.

Representation contract (shared with the C port and the oracle):
- None == JSON null; plain JSON numbers are Python ints; JSON bools are bools.
- A Python float travels as {"__float__": true, "v": <number>}; a tuple as
  {"__tuple__": true, "v": [...]}; a datetime double as
  {"__dt__": true, "tzinfo": <token|null>, "microsecond": <int>}.
- Lone surrogates ride the fixture as RAW UTF-8 triplets (surrogatepass write);
  libjson passes them through verbatim and the oracle decodes surrogatepass.
- Fixture domain: text ASCII (+ raw surrogate triplets / valid non-ASCII);
  timestamps int/float/None; uids non-empty ASCII strings; data_use values
  list/tuple/str/dict/int/bool/float/null; uuid4 returns {"hex": str};
  dt.timestamp returns int/float; wall_time/localtime are 9-tuples;
  session_id str/null; no NaN/Infinity JSON literals.
"""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import oracle_store_rules_flow as oracle  # noqa: E402

OUT = HERE / "fixtures" / "store_rules_flow.jsonl"
CASES = []


def fl(v):
    return {"__float__": True, "v": v}


def tu(items):
    return {"__tuple__": True, "v": list(items)}


def dt(tzinfo=None, microsecond=0):
    return {"__dt__": True, "tzinfo": tzinfo, "microsecond": microsecond}


def script(ret=None, raise_spec=None):
    d = {"__script__": True}
    if raise_spec is not None:
        d["raise"] = raise_spec
    else:
        d["ret"] = ret
    return d


def raise_spec(type, message="", errno=None):
    d = {"type": type, "message": message}
    if errno is not None:
        d["errno"] = errno
    return d


WALL = (2026, 1, 2, 3, 4, 5, 4, 2, -1)


def default_return(name, idx, args):
    if name == "uuid4":
        return {"hex": f"u{idx:031x}"}
    if name in ("datetime.fromisoformat", "datetime.strptime"):
        return dt()
    if name == "dt.replace":
        return dt(tzinfo=args.get("tzinfo"))
    if name == "dt.astimezone":
        return dt(tzinfo="local")
    if name == "dt.timestamp":
        return fl(1700000000.0)
    if name == "dt.timetuple":
        return tu(WALL)
    if name == "time.mktime":
        return fl(3600.0)
    if name == "time.localtime":
        wt = args.get("wall_time") or {}
        items = wt.get("v", []) if isinstance(wt, dict) else list(wt)
        return tu(tuple(items[:6]) + (0, 0, -1))
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


# ── lift_data_use ──
add("lift_data_use", {"message": {"role": "user", "display_metadata": {"data_use": ["a", "b"]}}})
add("lift_data_use", {"message": {"display_metadata": {"data_use": []}}})
add("lift_data_use", {"message": {"display_metadata": "not-a-dict"}})
add("lift_data_use", {"message": {"role": "user"}})
add("lift_data_use", {"message": {"display_metadata": {"data_use": "ab"}}})
add("lift_data_use", {"message": {"display_metadata": {"data_use": {"k1": 1, "k2": 2}}}})
add("lift_data_use", {"message": {"display_metadata": {"data_use": 5}}})
add("lift_data_use", {"message": {"display_metadata": {"data_use": tu(("x", "y"))}}})
add("lift_data_use", {"message": {"display_metadata": {"data_use": 0}}})
add("lift_data_use", {"message": None})
add("lift_data_use", {"message": {"display_metadata": {"data_use": False}}})

# ── stamp_message_uid ──
add("stamp_message_uid", {"msg": {"message_uid": "u-existing", "content": "x"}})
add("stamp_message_uid", {"msg": {"content": "x"}})
add("stamp_message_uid", {"msg": {"message_uid": ""}})
add("stamp_message_uid", {"msg": {"message_uid": 42}})
add("stamp_message_uid", {"msg": {}},
    overrides={"uuid4": script(raise_spec=raise_spec("RuntimeError", "entropy failed"))})
add("stamp_message_uid", {"msg": {}}, overrides={"uuid4": {}})
add("stamp_message_uid", {"msg": {}}, overrides={"uuid4": None})
add("stamp_message_uid", {"msg": {}}, fail_closed=True)
add("stamp_message_uid", {"msg": {"message_uid": None}})

# ── _is_checkpoint_item ──
add("_is_checkpoint_item", {"item": {"type": "compaction"}})
add("_is_checkpoint_item", {"item": {"type": "text", "text": "x"}})
add("_is_checkpoint_item", {"item": {"type": "compaction", "summary": "s"}})
add("_is_checkpoint_item", {"item": "compaction"})
add("_is_checkpoint_item", {"item": {}})
add("_is_checkpoint_item", {"item": None})
add("_is_checkpoint_item", {"item": {"type": 5}})
add("_is_checkpoint_item", {"item": [ {"type": "compaction"} ]})

# ── _newest_checkpoint_carrier ──
CP = {"type": "compaction"}
TX = {"type": "text", "text": "x"}
add("_newest_checkpoint_carrier", {"messages": [], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [{"role": "assistant", "content": [CP]}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [
    {"role": "assistant", "content": [CP]},
    {"role": "user", "content": [CP]},
    {"role": "assistant", "content": [TX, CP]}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [{"role": "user", "content": [CP]}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [{"role": "assistant", "content": "plain"}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [{"role": "assistant", "content": [TX]}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [
    "not-a-dict", None, {"role": "assistant", "content": [CP]}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [{"role": "assistant", "tool_calls": [CP]}], "key": "tool_calls"})
add("_newest_checkpoint_carrier", {"messages": 5, "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [
    {"role": "assistant", "content": [CP]},
    {"role": "assistant", "content": [TX]}], "key": "content"})
add("_newest_checkpoint_carrier", {"messages": [{"role": "assistant"}], "key": "content"})

# ── _sanitize_surrogates (raw lone-surrogate triplets ride surrogatepass) ──
add("_sanitize_surrogates", {"text": "hello world"})
add("_sanitize_surrogates", {"text": ""})
add("_sanitize_surrogates", {"text": "caf\u00e9 \u2014 ok"})
add("_sanitize_surrogates", {"text": "\ud800"})
add("_sanitize_surrogates", {"text": "\udfff"})
add("_sanitize_surrogates", {"text": "a\ud800b\udfffc"})
add("_sanitize_surrogates", {"text": "\ud800\udc00"})
add("_sanitize_surrogates", {"text": "\ufffd already replacement"})
add("_sanitize_surrogates", {"text": 5})
add("_sanitize_surrogates", {"text": "tab\tand\nnewline \x1c ctl"})

# ── _localize ──
add("_localize", {"dt": dt(tzinfo="UTC"), "tz": None})
add("_localize", {"dt": dt(), "tz": "tz:+02:00"})
add("_localize", {"dt": dt(), "tz": None})
add("_localize", {"dt": dt(tzinfo="UTC"), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("ValueError", "year out of range"))})
add("_localize", {"dt": dt(tzinfo="UTC"), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OverflowError", "too big"))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22))})
add("_localize", {"dt": dt(microsecond=500000), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22))})
add("_localize", {"dt": dt(tzinfo="UTC"), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22))})
add("_localize", {"dt": dt(), "tz": "tz:+02:00"},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "no such file", errno=2))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "time.mktime": script(raise_spec=raise_spec("OSError", "mktime failed", errno=22))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "time.localtime": tu((1970, 1, 1, 0, 0, 0, 3, 1, -1))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "time.mktime": fl(90000.0)})
add("_localize", {"dt": dt(tzinfo="UTC"), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("TypeError", "bad type"))})
add("_localize", {"dt": dt(tzinfo="UTC"), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("SystemExit", "bye"))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "time.mktime": script(raise_spec=raise_spec("ValueError", "bad wall"))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "dt.timetuple": script(raise_spec=raise_spec("RuntimeError", "timetuple broke"))})
add("_localize", {"dt": None, "tz": None})
add("_localize", {"dt": dt(), "tz": None}, fail_closed=True)
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "dt.replace": dt(tzinfo="x", microsecond=250000),
               "time.mktime": fl(7200.0)},
    )
# recovery reads microsecond from the REPLACED dt (tz given -> replace fires)
add("_localize", {"dt": dt(), "tz": "tz:UTC"},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "dt.replace": dt(tzinfo="tz:UTC", microsecond=250000),
               "time.mktime": fl(7200.0)})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "time.localtime": script(raise_spec=raise_spec("OverflowError", "lt overflow"))})
add("_localize", {"dt": dt(), "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22)),
               "time.mktime": script(raise_spec=raise_spec("SystemExit", "mk exit"))})

# ── _parse_iso ──
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": None})
add("_parse_iso", {"text": "2026-01-02T03:04:05+0200", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("ValueError", "Invalid isoformat string"))})
add("_parse_iso", {"text": "garbage", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("ValueError", "Invalid isoformat string")),
               "datetime.strptime": script(raise_spec=raise_spec("ValueError", "time data does not match"))})
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("TypeError", "fromisoformat: argument must be str"))})
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("ValueError", "year out of range"))})
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": None},
    overrides={"dt.timestamp": [script(raise_spec=raise_spec("TypeError", "bad ts")), fl(1700000000.0)]})
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("RuntimeError", "boom"))})
add("_parse_iso", {"text": "2026-01-02T03:04:05+0200", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("ValueError", "Invalid isoformat string")),
               "datetime.strptime": script(raise_spec=raise_spec("RuntimeError", "strptime boom"))})
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": None}, fail_closed=True)
add("_parse_iso", {"text": "2026-01-02T03:04:05", "tz": "tz:UTC"},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "no recovery", errno=2))})
add("_parse_iso", {"text": "2026-01-02T03:04:05+0200", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("ValueError", "Invalid isoformat string")),
               "datetime.strptime": dt(tzinfo="+0200")})

# ── _parse_timestamp_match ──
add("_parse_timestamp_match", {"match_text": "[Tue 2026-01-02 03:04:05 UTC] hi", "tz": None})
add("_parse_timestamp_match", {"match_text": "[2026-01-02T03:04:05+00:00] hi", "tz": None})
add("_parse_timestamp_match", {"match_text": "no prefix here", "tz": None})
add("_parse_timestamp_match", {"match_text": "[Tue 2026-01-02 03:04:05] hi", "tz": None},
    overrides={"datetime.strptime": script(raise_spec=raise_spec("ValueError", "time data does not match"))})
add("_parse_timestamp_match", {"match_text": "[Tue 2026-01-02 03:04:05] hi", "tz": None},
    overrides={"datetime.strptime": script(raise_spec=raise_spec("TypeError", "strptime type"))})
add("_parse_timestamp_match", {"match_text": "[2026-01-02T03:04:05] hi", "tz": None},
    overrides={"datetime.fromisoformat": script(raise_spec=raise_spec("ValueError", "Invalid isoformat string"))})
add("_parse_timestamp_match", {"match_text": "[Tue 2026-01-02 03:04:05]", "tz": None})
add("_parse_timestamp_match", {"match_text": 5, "tz": None})
add("_parse_timestamp_match", {"match_text": "[Tue 2026-01-02 03:04:05] hi", "tz": None},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("OSError", "invalid fold", errno=22))})
add("_parse_timestamp_match", {"match_text": "[Tue 2026-01-02 03:04:05] hi", "tz": None}, fail_closed=True)
add("_parse_timestamp_match", {"match_text": "[Fri 2026-12-31 23:59:59 GMT+1] x", "tz": None})
add("_parse_timestamp_match", {"match_text": None, "tz": None})

# ── strip_leading_message_timestamps ──
add("strip_leading_message_timestamps", {"content": 5, "tz": None})
add("strip_leading_message_timestamps", {"content": None, "tz": None})
add("strip_leading_message_timestamps", {"content": "", "tz": None})
add("strip_leading_message_timestamps", {"content": "plain text", "tz": None})
add("strip_leading_message_timestamps", {"content": "[Tue 2026-01-02 03:04:05 UTC] hello", "tz": None})
add("strip_leading_message_timestamps", {"content": "[2026-01-02T03:04:05] body", "tz": None})
add("strip_leading_message_timestamps",
    {"content": "[Tue 2026-01-02 03:04:05] [Wed 2026-01-03 04:05:06] msg", "tz": None},
    overrides={"datetime.strptime": [dt(), dt()],
               "dt.timestamp": [fl(1700000000.0), fl(1700086400.0)]})
add("strip_leading_message_timestamps", {"content": "[Tue 2026-13-99 99:99:99] msg", "tz": None},
    overrides={"datetime.strptime": script(raise_spec=raise_spec("ValueError", "unconverted data"))})
add("strip_leading_message_timestamps", {"content": "[Tue 2026-01-02 03:04:05] [bad] msg", "tz": None})
add("strip_leading_message_timestamps", {"content": [1, 2], "tz": None})
add("strip_leading_message_timestamps", {"content": "[Tue 2026-01-02 03:04:05] \t\n msg", "tz": None})
add("strip_leading_message_timestamps", {"content": "[2026-01-02T03:04:05] x", "tz": "tz:UTC"})
add("strip_leading_message_timestamps", {"content": "[Tue 2026-01-02 03:04:05] x", "tz": None},
    overrides={"datetime.strptime": script(raise_spec=raise_spec("RuntimeError", "boom"))})
add("strip_leading_message_timestamps", {"content": "[Tue 2026-01-02 03:04:05] x", "tz": None}, fail_closed=True)
add("strip_leading_message_timestamps",
    {"content": "[bad] [Tue 2026-01-02 03:04:05] after-non-prefix", "tz": None})
add("strip_leading_message_timestamps",
    {"content": "[Tue 2026-01-02 03:04:05] [2026-01-03T04:05:06] mixed", "tz": None},
    overrides={"dt.timestamp": [fl(1700000000.0), fl(1700086400.0)]})

# ── coerce_epoch ──
add("coerce_epoch", {"value": None})
add("coerce_epoch", {"value": ""})
add("coerce_epoch", {"value": 1700000000})
add("coerce_epoch", {"value": fl(1.5)})
add("coerce_epoch", {"value": "1700000000"})
add("coerce_epoch", {"value": " 1.5e3 "})
add("coerce_epoch", {"value": "1_000"})
add("coerce_epoch", {"value": "bad"})
add("coerce_epoch", {"value": "inf"})
add("coerce_epoch", {"value": "nan"})
add("coerce_epoch", {"value": 4200000001})
add("coerce_epoch", {"value": -1})
add("coerce_epoch", {"value": True})
add("coerce_epoch", {"value": [1]})
add("coerce_epoch", {"value": {"a": 1}})
add("coerce_epoch", {"value": dt(tzinfo="UTC")})
add("coerce_epoch", {"value": dt(tzinfo="UTC")},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("ValueError", "year out of range"))})
add("coerce_epoch", {"value": "bad", "session_id": "s1"})
add("coerce_epoch", {"value": "bad", "field": "created_at"})
add("coerce_epoch", {"value": "0"})
add("coerce_epoch", {"value": "4200000000"})
add("coerce_epoch", {"value": dt(tzinfo="UTC")},
    overrides={"dt.timestamp": script(raise_spec=raise_spec("RuntimeError", "boom"))})
add("coerce_epoch", {"value": dt()}, fail_closed=True)
add("coerce_epoch", {"value": "_1"})
add("coerce_epoch", {"value": "1__0"})
add("coerce_epoch", {"value": tu((1,))})
add("coerce_epoch", {"value": "1."})
add("coerce_epoch", {"value": ".5"})
add("coerce_epoch", {"value": "1e"})
add("coerce_epoch", {"value": "0x10"})
add("coerce_epoch", {"value": "INFINITY"})
add("coerce_epoch", {"value": "-inf"})
add("coerce_epoch", {"value": "1_0.0_1e1_0"})
add("coerce_epoch", {"value": "bad", "session_id": ""})
add("coerce_epoch", {"value": False})
add("coerce_epoch", {"value": "5\x1c"})
add("coerce_epoch", {"value": "\x1c5"})
add("coerce_epoch", {"value": "\x1f5\x1f"})
add("coerce_epoch", {"value": " 5 "})
add("coerce_epoch", {"value": "-1.5e3"})
add("coerce_epoch", {"value": "-0"})
add("coerce_epoch", {"value": "+1_0"})
add("coerce_epoch", {"value": "-1e-3"})

# ── _is_image_part ──
add("_is_image_part", {"part": {"type": "image_url", "image_url": {"url": "u"}}})
add("_is_image_part", {"part": {"type": "input_image"}})
add("_is_image_part", {"part": {"type": "image"}})
add("_is_image_part", {"part": {"type": "text", "text": "x"}})
add("_is_image_part", {"part": "image"})
add("_is_image_part", {"part": {}})
add("_is_image_part", {"part": {"type": 5}})
add("_is_image_part", {"part": None})

# ── mint_uid ──
add("mint_uid", {})
add("mint_uid", {}, overrides={"uuid4": script(raise_spec=raise_spec("ValueError", "bad entropy"))})
add("mint_uid", {}, overrides={"uuid4": None})
add("mint_uid", {}, overrides={"uuid4": "not-an-object"})
add("mint_uid", {}, fail_closed=True)
add("mint_uid", {}, overrides={"uuid4": {"hex": ""}})
add("mint_uid", {}, overrides={"uuid4": {"hex": "deadbeef" * 4}})

# ── uid_list ──
add("uid_list", {"value": ["a", "b", "a", "", "c", 5, None]})
add("uid_list", {"value": []})
add("uid_list", {"value": "abc"})
add("uid_list", {"value": None})
add("uid_list", {"value": tu(("a", "b"))})
add("uid_list", {"value": ["", ""]})
add("uid_list", {"value": {"a": 1}})
add("uid_list", {"value": ["x", "x", "x"]})
add("uid_list", {"value": [True, "t"]})

# ── message_uid_or_none ──
add("message_uid_or_none", {"msg": {"message_uid": "u"}})
add("message_uid_or_none", {"msg": {"message_uid": ""}})
add("message_uid_or_none", {"msg": {"message_uid": 5}})
add("message_uid_or_none", {"msg": {}})
add("message_uid_or_none", {"msg": None})
add("message_uid_or_none", {"msg": [1]})
add("message_uid_or_none", {"msg": "s"})
add("message_uid_or_none", {"msg": {"message_uid": None}})

# ── _is_empty_content ──
add("_is_empty_content", {"content": None})
add("_is_empty_content", {"content": []})
add("_is_empty_content", {"content": ""})
add("_is_empty_content", {"content": "  \t\n "})
add("_is_empty_content", {"content": "x"})
add("_is_empty_content", {"content": [{"type": "text"}]})
add("_is_empty_content", {"content": tu(())})
add("_is_empty_content", {"content": 0})
add("_is_empty_content", {"content": {}})
add("_is_empty_content", {"content": " \x1c "})

# ── drop_typed_ask ──
add("drop_typed_ask", {"message": {"display_metadata": {"typed_ask": "the ask", "reactions": []}}})
add("drop_typed_ask", {"message": {"display_metadata": {"reactions": []}}})
add("drop_typed_ask", {"message": {"display_metadata": "not-a-dict"}})
add("drop_typed_ask", {"message": {"role": "user"}})
add("drop_typed_ask", {"message": {"display_metadata": {"typed_ask": None}}})
add("drop_typed_ask", {"message": None})
add("drop_typed_ask", {"message": {"display_metadata": {"a": 1, "typed_ask": "x", "b": 2}}})

# ── record_absorbed_message ──
add("record_absorbed_message", {"survivor": {"message_uid": "s", "content": "keep",
                                             "display_metadata": {"typed_ask": "ask"}},
                                "dropped": {"message_uid": "d", "content": "text"}})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "content": "keep"},
                                "dropped": {"message_uid": "d", "content": "text"},
                                "dropped_leads": True})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "content": "keep",
                                             "display_metadata": {"typed_ask": "ask"}},
                                "dropped": {"message_uid": "d", "content": ""}})
add("record_absorbed_message", {"survivor": {"message_uid": "s"},
                                "dropped": {"message_uid": "d", "content": None}})
add("record_absorbed_message", {"survivor": {"content": "keep"}, "dropped": {"content": "text"}})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "_absorbed_message_uids": ["x"]},
                                "dropped": {"message_uid": "d", "_absorbed_message_uids": ["y"],
                                            "content": "text"}})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "_absorbed_message_uids": ["o"]},
                                "dropped": {"message_uid": "d", "_absorbed_message_uids": ["t"],
                                            "content": "text"},
                                "dropped_leads": True})
add("record_absorbed_message", {"survivor": {"message_uid": "s"},
                                "dropped": {"message_uid": "s", "content": "text"}})
add("record_absorbed_message", {"survivor": {"message_uid": "s"},
                                "dropped": {"content": "text"}, "dropped_leads": True})
add("record_absorbed_message", {"survivor": None, "dropped": {"content": "text"}})
add("record_absorbed_message", {"survivor": {"message_uid": "s"}, "dropped": None})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "content": "keep"},
                                "dropped": {"message_uid": "d", "content": []}})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "_absorbed_message_uids": ["d"]},
                                "dropped": {"message_uid": "d", "content": "text"}})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "content": "keep",
                                             "display_metadata": {"typed_ask": "ask", "model_only": True}},
                                "dropped": {"message_uid": "d", "content": "  "}})
add("record_absorbed_message", {"survivor": {"message_uid": "s", "_absorbed_message_uids": ["d", "d", "x"]},
                                "dropped": {"message_uid": "d", "_absorbed_message_uids": ["x", "y"],
                                            "content": "text"}})


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    missing = [k for k in oracle.KINDS if counts.get(k, 0) < 5]
    if missing:
        print(f"FATAL gen_store_rules_flow: funcs with <5 cases: {missing}")
        sys.exit(2)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "wb") as f:
        for c in CASES:
            line = json.dumps(c, ensure_ascii=False)
            f.write(line.encode("utf-8", errors="surrogatepass") + b"\n")
    print(f"gen_store_rules_flow: {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()