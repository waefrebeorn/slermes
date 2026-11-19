#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow2.jsonl for the Slack
plugin-adapter FLOW port, wave 2 (src/gateway/platforms/slack_adapter_flow2.c).

Deterministic corpus: >=5 cases per ported function (26 unclaimed funcs),
covering pure-leaf edges, seamed decision chains (scripted results, ordered
traces), state mutation/eviction, fail-closed missing scripts and
BaseException passthrough. Case shape: {"func", "in": {"args","io","state"},
"expect_raise"?}.

Representation markers (shared with the C port + oracle):
- {"__obj__": {attrs}}  -- attribute carrier (SimpleNamespace double)
- {"__set__": [members]} -- set carrier (C keeps members sorted)
- {"__tuple__": [items]} -- tuple INPUT (outputs are plain JSON arrays)
- {"__exc__": {type,message,...attrs}} -- exception instance double
- state.slash_command_contexts: ordered [{"key":[...],"value":{...}}] pairs
  (a dict with tuple keys live)
- state.slash_user_id: the _slash_user_id ContextVar value
- state.aiohttp: {"classes": {name: parent|null|{"const":v}}} module double

Domain guards (ASCII scope, shared with the port header):
- str()-computed positions carry str/int/bool/None only (no containers, no
  non-integral floats).
- time.monotonic scripts carry NON-INTEGRAL floats (float identity matters
  for the subtraction TypeError messages).
- _discard_oldest_by_thread_ts carriers are dicts only: set iteration order
  is hash-randomized live, so ordered traces would be nondeterministic.
- _slack_timestamp_sort_key ts inputs: str/int/bool/None/2-tuples only
  (str() of other containers is repr, outside the ASCII corpus).
- dict-key flows (_get_client, scope_id_for_chat) use str keys/args only.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "slack_adapter_flow2.jsonl")
CASES = []


def case(func, args, io=None, state=None, expect_raise=None):
    inp = {"args": args}
    if io is not None:
        inp["io"] = io
    if state is not None:
        inp["state"] = state
    rec = {"func": func, "in": inp}
    if expect_raise:
        rec["expect_raise"] = expect_raise
    CASES.append(rec)


def script(ret=None, raise_spec=None):
    d = {"__script__": True}
    if raise_spec is not None:
        d["raise"] = raise_spec
    else:
        d["ret"] = ret
    return d


def rs(kind, msg):
    return {"type": kind, "message": msg}


def obj(**attrs):
    return {"__obj__": attrs}


def st(*members):
    return {"__set__": list(members)}


def tup(*items):
    return {"__tuple__": list(items)}


def exc(kind, msg="", **attrs):
    d = {"type": kind, "message": msg}
    d.update(attrs)
    return {"__exc__": d}


def int_or_zero(v):
    try:
        return int(v)
    except (ValueError, TypeError):
        return 0


def sort_key_parts(ts):
    s = str(ts)
    seconds, _, fraction = s.partition(".")
    return [int_or_zero(seconds), int_or_zero((fraction + "000000")[:6] or "0")]


# ── _slack_error_is ──
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response={"error": "invalid_auth"}), "code": "invalid_auth"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response={"error": "invalid_auth"}), "code": "not_authed"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response={"error": 5}), "code": "5"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response={}), "code": "x"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response=None), "code": "x"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom"), "code": "x"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response="text-body"), "code": "x"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response=[1]), "code": "x"})
case("_slack_error_is", {"exc": exc("SlackApiError", "boom", response={"ok": False}), "code": ""})
case("_slack_error_is", {"exc": exc("RuntimeError", "boom", response={"error": ""}), "code": ""})

# ── _is_transient_transport_error ──
AIO_FULL = {"classes": {"ClientConnectionError": "Exception",
                        "ClientSSLError": "ClientConnectionError",
                        "ServerFingerprintMismatch": "ClientConnectionError"}}
AIO_CONN_ONLY = {"classes": {"ClientConnectionError": "Exception"}}
AIO_CONST = {"classes": {"ClientConnectionError": {"const": 5},
                         "ClientSSLError": None}}
case("_is_transient_transport_error", {"exc": exc("TimeoutError", "t")}, state={"aiohttp": AIO_FULL})
case("_is_transient_transport_error", {"exc": exc("ClientConnectionError", "c")}, state={"aiohttp": AIO_FULL})
case("_is_transient_transport_error", {"exc": exc("ClientSSLError", "s")}, state={"aiohttp": AIO_FULL})
case("_is_transient_transport_error", {"exc": exc("ServerFingerprintMismatch", "f")}, state={"aiohttp": AIO_FULL})
case("_is_transient_transport_error", {"exc": exc("ClientSSLError", "s")}, state={"aiohttp": AIO_CONN_ONLY})
case("_is_transient_transport_error", {"exc": exc("ClientConnectionError", "c")}, state={"aiohttp": AIO_CONN_ONLY})
case("_is_transient_transport_error", {"exc": exc("ClientConnectionError", "c")}, state={"aiohttp": None})
case("_is_transient_transport_error", {"exc": exc("TimeoutError", "t")}, state={})
case("_is_transient_transport_error", {"exc": exc("ValueError", "v")}, state={"aiohttp": AIO_FULL})
case("_is_transient_transport_error", {"exc": exc("ClientConnectionError", "c")}, state={"aiohttp": AIO_CONST})
case("_is_transient_transport_error", {"exc": exc("ClientSSLError", "s")}, state={"aiohttp": AIO_CONST})
case("_is_transient_transport_error", {"exc": exc("OSError", "o")}, state={"aiohttp": AIO_FULL})

# ── _collect_slack_table_cell_text ──
case("_collect_slack_table_cell_text", {"value": None})
case("_collect_slack_table_cell_text", {"value": "plain"})
case("_collect_slack_table_cell_text", {"value": {"text": "leaf"}})
case("_collect_slack_table_cell_text", {"value": {"text": ""}})
case("_collect_slack_table_cell_text", {"value": {"text": 5}})
case("_collect_slack_table_cell_text", {"value": [{"text": "a"}, {"text": "b"}]})
case("_collect_slack_table_cell_text", {"value": {"text": "top", "elements": [{"text": "mid"}, {"text": "low"}]}})
case("_collect_slack_table_cell_text", {"value": {"text": "  pad  ", "x": {"text": ""}, "y": [{"text": "z"}]}})
case("_collect_slack_table_cell_text", {"value": {"a": {"text": "one"}, "b": {"text": "two"}, "text": "root"}})
case("_collect_slack_table_cell_text", {"value": [1, "s", None, {"text": "keep"}]})

# ── _render_slack_table_block ──
case("_render_slack_table_block", {"block": None})
case("_render_slack_table_block", {"block": "not-a-dict"})
case("_render_slack_table_block", {"block": {"rows": "no"}})
case("_render_slack_table_block", {"block": {"rows": []}})
case("_render_slack_table_block", {"block": {"rows": [[{"text": "a"}, {"text": "b"}], [{"text": "c"}, {"text": "d"}]]}})
case("_render_slack_table_block", {"block": {"rows": ["skip", [{"text": "keep"}], 5]}})
case("_render_slack_table_block", {"block": {"rows": [[{"text": ""}, {"text": " "}], [{"text": "x"}]]}})
case("_render_slack_table_block", {"block": {"rows": [[{"text": "a"}, {"text": "b"}]]}, "max_chars": 3})
case("_render_slack_table_block", {"block": {"rows": [[{"text": "abcdefgh"}, {"text": "ijklmnop"}]]}, "max_chars": 25})
case("_render_slack_table_block", {"block": {"rows": [[{"text": "abcdefgh"}, {"text": "ijklmnop"}]]}, "max_chars": 10})
case("_render_slack_table_block", {"block": {"rows": [[{"text": "abcdefgh"}, {"text": "ijklmnop"}]]}, "max_chars": -5})
case("_render_slack_table_block", {"block": {"rows": [[{"text": "a"}]]}, "max_chars": 20000})

# ── _extract_text_from_slack_blocks ──
def render_scripts(blocks):
    """Mirror the live traversal to count _render_inline_elements seam calls
    in order; assign deterministic markers R0,R1,... (scripts are INPUTS)."""
    rets = []

    def walk_elements(elements, depth=0, bullet=""):
        for elem in elements:
            t = elem.get("type", "")
            if t == "rich_text_section":
                rets.append("R%d" % len(rets))
            elif t == "rich_text_quote":
                walk_elements(elem.get("elements", []), depth + 1)
            elif t == "rich_text_list":
                style = elem.get("style")
                for idx, item in enumerate(elem.get("elements", [])):
                    b = "\u2022 " if style == "bullet" else "%d. " % (idx + 1)
                    walk_elements([item], depth, b)
            elif t == "rich_text_preformatted":
                for child in elem.get("elements", []):
                    rets.append("R%d" % len(rets))
            else:
                rets.append("R%d" % len(rets))

    for block in blocks or []:
        bt = (block or {}).get("type")
        if bt == "rich_text":
            walk_elements(block.get("elements", []))
    return rets


def extract_case(blocks, io=None, expect_raise=None):
    io = dict(io or {})
    if "_render_inline_elements" not in io and not expect_raise:
        io["_render_inline_elements"] = render_scripts(blocks)
    case("_extract_text_from_slack_blocks", {"blocks": blocks}, io=io, expect_raise=expect_raise)


extract_case(None)
extract_case([])
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_section", "elements": [{"type": "text", "text": "hi"}]}]}])
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_quote", "elements": [
        {"type": "rich_text_section", "elements": [{"type": "text", "text": "q"}]}]}]}])
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_list", "style": "bullet", "elements": [
        {"type": "rich_text_section", "elements": [{"type": "text", "text": "a"}]},
        {"type": "rich_text_section", "elements": [{"type": "text", "text": "b"}]}]}]}])
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_list", "style": "ordered", "elements": [
        {"type": "rich_text_section", "elements": [{"type": "text", "text": "a"}]},
        {"type": "rich_text_section", "elements": [{"type": "text", "text": "b"}]}]}]}])
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_preformatted", "language": "python", "elements": [
        {"type": "rich_text_section", "elements": [{"type": "text", "text": "x=1"}]},
        {"type": "text", "text": "y=2"}]}]}])
extract_case([{"type": "rich_text", "elements": [{"type": "emoji", "name": "smile"}]}])
extract_case([{"type": "table", "rows": [[{"text": "a"}]]},
              {"type": "rich_text", "elements": [
                  {"type": "rich_text_section", "elements": [{"type": "text", "text": "s"}]}]}],
             io={"_render_slack_table_block": ["a | b"], "_render_inline_elements": ["S"]})
extract_case([None, {}, {"type": "section", "text": "x"}])
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_section", "elements": []}]}],
    io={"_render_inline_elements": ["   "]})
extract_case(["bad-block"], expect_raise="AttributeError")
extract_case([{"type": "rich_text", "elements": ["bad-elem"]}], expect_raise="AttributeError")
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_section", "elements": []}]}], io={}, expect_raise="RuntimeError")  # fail-closed
extract_case([{"type": "table", "rows": []}], io={}, expect_raise="RuntimeError")  # fail-closed table seam
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_quote", "elements": [
        {"type": "rich_text_list", "style": "bullet", "elements": [
            {"type": "rich_text_section", "elements": []}]}]}]}],
    io={"_render_inline_elements": ["deep"]})
extract_case([{"type": "rich_text", "elements": [
    {"type": "rich_text_preformatted", "elements": [
        {"type": "rich_text_section", "elements": []}]}]}],
    io={"_render_inline_elements": [""]})

# ── _extract_urls_from_slack_blocks ──
case("_extract_urls_from_slack_blocks", {"blocks": None})
case("_extract_urls_from_slack_blocks", {"blocks": []})
case("_extract_urls_from_slack_blocks", {"blocks": {}})
case("_extract_urls_from_slack_blocks", {"blocks": [{"url": "https://a.example", "image_url": "http://b.example"}]})
case("_extract_urls_from_slack_blocks", {"blocks": [{"url": "ftp://c.example", "external_url": "https://d.example"}]})
case("_extract_urls_from_slack_blocks", {"blocks": [{"url": "https://a.example"}, {"url": "https://a.example"}, {"url": "http://a.example"}]})
case("_extract_urls_from_slack_blocks", {"blocks": [{"url": 5}, {"url": None}, {"url": "https://ok.example"}]})
case("_extract_urls_from_slack_blocks", {"blocks": [{"elements": [{"url": "https://nested.example"}], "url": "https://top.example"}]})
case("_extract_urls_from_slack_blocks", {"blocks": [[{"image_url": "https://img.example"}], {"external_url": "http://ext.example"}]})
case("_extract_urls_from_slack_blocks", {"blocks": [{"url": "http://"}, {"url": "https://x.example", "z": {"url": "https://deep.example"}}]})

# ── _apply_slack_proxy ──
case("_apply_slack_proxy", {"proxy_url": "http://p:8080"}, state={"client": obj(proxy=None)})
case("_apply_slack_proxy", {"proxy_url": None}, state={"client": obj(proxy="http://old")})
case("_apply_slack_proxy", {"proxy_url": "http://p"}, state={"client": obj(other=1)})
case("_apply_slack_proxy", {"proxy_url": "http://p"}, state={"client": {"proxy": "dict-not-obj"}})
case("_apply_slack_proxy", {"proxy_url": "http://p"}, state={"client": None})
case("_apply_slack_proxy", {"proxy_url": ""}, state={"client": obj(proxy="x", name="c1")})

# ── _slack_timestamp_sort_key ──
def sort_key_case(ts, scripts=None):
    parts = sort_key_parts(ts if not isinstance(ts, dict) else ts["__tuple__"][1])
    io = {"_int_or_zero": scripts if scripts is not None else parts}
    case("_slack_timestamp_sort_key", {"ts": ts}, io=io)


sort_key_case("1712345678.000001")
sort_key_case("123")
sort_key_case("123.4")
sort_key_case("123.1234567")
sort_key_case("abc.def")
sort_key_case("")
sort_key_case(1712345678)
sort_key_case(None)
sort_key_case(True)
sort_key_case(tup("T1", "1712345678.5"))
sort_key_case(".5")
sort_key_case("12.", scripts=[12, 0])
case("_slack_timestamp_sort_key", {"ts": "1.2"}, io={}, expect_raise="RuntimeError")  # fail-closed

# ── _trim_oldest_dict_entries ──
def trim_case(mapping, max_size):
    case("_trim_oldest_dict_entries", {"max_size": max_size}, state={"mapping": mapping})


trim_case({"a": 1, "b": 2, "c": 3}, 5)
trim_case({"a": 1, "b": 2, "c": 3}, 3)
trim_case({"a": 1, "b": 2, "c": 3, "d": 4}, 2)
trim_case({"a": 1, "b": 2, "c": 3, "d": 4, "e": 5}, 0)
trim_case({"a": 1, "b": 2, "c": 3}, -3)
trim_case({}, 1)
trim_case({"k%d" % i: i for i in range(10)}, 7)
trim_case({"a": 1}, 1)

# ── _discard_oldest_by_thread_ts ──
def discard_case(entries, count, ts_map=None, io_extra=None):
    io = {}
    keys = list(entries)
    if ts_map is not None:
        io["ts_getter"] = [ts_map[k] for k in keys]
        tss = [ts_map[k] for k in keys]
    else:
        tss = keys
    queue = []
    for t in tss:
        queue.extend(sort_key_parts(t))
    io["_int_or_zero"] = queue
    if io_extra:
        io.update(io_extra)
    args = {"count": count}
    if ts_map is not None:
        args["ts_getter"] = True
    case("_discard_oldest_by_thread_ts", args, io=io, state={"entries": entries})


discard_case({"1712345678.000001": "a", "1712345670.5": "b", "1712345680.0": "c"}, 1)
discard_case({"1712345678.000001": "a", "1712345670.5": "b", "1712345680.0": "c"}, 2)
discard_case({"1712345678.000001": "a", "1712345670.5": "b"}, 0)
discard_case({"1712345678.000001": "a", "1712345670.5": "b"}, -1)
discard_case({"1712345678.000001": "a", "1712345670.5": "b"}, 9)
discard_case({}, 3)
discard_case({"t:1712345678.000001": "a", "t:1712345670.5": "b", "t:1712345672.25": "c"}, 1,
             ts_map={"t:1712345678.000001": "1712345678.000001",
                     "t:1712345670.5": "1712345670.5",
                     "t:1712345672.25": "1712345672.25"})
discard_case({"10.5": "a", "9.9": "b", "10.4": "c", "2.0": "d"}, 2)
discard_case({"1712345678.000001": "a"}, 1, io_extra={"_int_or_zero": []}, )  # fail-closed exhausted
CASES[-1]["expect_raise"] = "RuntimeError"

# ── _metadata_team_id ──
FT_KEYS = ["scope_id", "slack_team_id", "team_id", "team", "guild_id", "workspace_id"]
SRC_KEYS = ["scope_id", "slack_team_id", "team_id", "guild_id"]
case("_metadata_team_id", {"metadata": None})
case("_metadata_team_id", {"metadata": {}}, io={"_first_truthy": [None]})
case("_metadata_team_id", {"metadata": {"team_id": "T1"}}, io={"_first_truthy": ["T1"]})
case("_metadata_team_id", {"metadata": {"team_id": 5}}, io={"_first_truthy": [5]})
case("_metadata_team_id", {"metadata": {"team_id": ""}}, io={"_first_truthy": [""]})
case("_metadata_team_id", {"metadata": {"source": {"team_id": "T2"}}},
     io={"_first_truthy": [None, "T2"]})
case("_metadata_team_id", {"metadata": {"source": {"team_id": "T2"}}},
     io={"_first_truthy": [None, None]})
case("_metadata_team_id", {"metadata": {"source": obj(scope_id="T3")}},
     io={"_first_truthy": [None]})
case("_metadata_team_id", {"metadata": {"source": obj(guild_id="G1")}},
     io={"_first_truthy": [None]})
case("_metadata_team_id", {"metadata": {"source": obj(other=1)}},
     io={"_first_truthy": [None]})
case("_metadata_team_id", {"metadata": {"source": "not-obj"}},
     io={"_first_truthy": [None]})
case("_metadata_team_id", {"metadata": {"source": None}}, io={"_first_truthy": [None]})
case("_metadata_team_id", {"metadata": {"team_id": "T1"}}, io={}, expect_raise="RuntimeError")  # fail-closed
case("_metadata_team_id", {"metadata": "string-meta"}, io={"_first_truthy": [None]},
     expect_raise="AttributeError")

# ── _workspace_event_id ──
for team, ev in [("T1", "E1"), ("", "E1"), (None, "E1"), ("T1", 5), (0, "E1"), ("T1", ""), (None, None), ("T1", True)]:
    case("_workspace_event_id", {"team_id": team, "event_id": ev})

# ── _workspace_message_marker ──
for team, mid in [("T1", "M1"), ("", "M1"), (None, "M1"), ("T1", 5), (0, 7), ("T1", ""), (None, None)]:
    case("_workspace_message_marker", {"team_id": team, "message_id": mid})

# ── _truthy_config ──
for v in ["1", "true", "YES", " On ", "on", "0", "false", "", "  ", "no", "True",
          1, 0, True, False, None, [], [0], {}, {"a": 1}, st(), st("x")]:
    case("_truthy_config", {"value": v})

# ── _task_update_chunk ──
case("_task_update_chunk", {"task": {}})
case("_task_update_chunk", {"task": {"status": "complete", "id": "t1", "title": "Done"}})
case("_task_update_chunk", {"task": {"status": "error", "task_id": "t2"}})
case("_task_update_chunk", {"task": {"status": "weird", "id": "t3", "title": "X"}})
case("_task_update_chunk", {"task": {"status": None, "id": None, "title": None}})
case("_task_update_chunk", {"task": {"status": 0, "id": 0, "title": 0}})
case("_task_update_chunk", {"task": {"status": "in_progress", "id": "t4", "title": "L" * 300}})
case("_task_update_chunk", {"task": {"id": "t5", "title": False}})
case("_task_update_chunk", {"task": {"task_id": "t6", "title": "T"}})
case("_task_update_chunk", {"task": "not-a-dict"}, expect_raise="AttributeError")
case("_task_update_chunk", {"task": None}, expect_raise="AttributeError")

# ── _stream_relation ──
for sent, text in [("", "anything"), ("   ", "x"), ("abc", "abcdef"), ("abc", "abc"),
                   ("abc", "  abc"), ("abc", "  abc  "), ("abc", "  abcX"),
                   ("abc", "xxabc"), ("abc", ""), (" abc ", "abc"),
                   ("abc", "abc\n"), ("a*b", "a*b tail"), ("abc", "  ab"),
                   ("abc", "abcabc")]:
    case("_stream_relation", {"sent": sent, "text": text})

# ── _lazy_attr ──
case("_lazy_attr", {"name": "_cache"}, io={"factory": [{"a": 1}]}, state={})
case("_lazy_attr", {"name": "_cache"}, io={"factory": [None]}, state={"_cache": None})
case("_lazy_attr", {"name": "_cache"}, state={"_cache": {"keep": 1}})
case("_lazy_attr", {"name": "_cache"}, state={"_cache": []})
case("_lazy_attr", {"name": "_cache"}, state={"_cache": 0})
case("_lazy_attr", {"name": "_cache"}, state={"_cache": ""})
case("_lazy_attr", {"name": "_cache"}, io={"factory": [obj(x=1)]}, state={})
case("_lazy_attr", {"name": "_cache"}, io={"factory": [st("a")]}, state={})
case("_lazy_attr", {"name": "_cache"}, io={}, state={}, expect_raise="RuntimeError")  # fail-closed
case("_lazy_attr", {"name": "_cache"}, io={"factory": [script(raise_spec=rs("ValueError", "bad factory"))]}, state={},
     expect_raise="ValueError")
case("_lazy_attr", {"name": "_cache"}, io={"factory": [script(raise_spec=rs("SystemExit", "bye"))]}, state={},
     expect_raise="SystemExit")

# ── _remember_channel_team ──
def remember_case(channel_id, team_id, state=None):
    case("_remember_channel_team", {"channel_id": channel_id, "team_id": team_id}, state=state or {})


remember_case("C1", "T1", state={"_channel_team": {}})
remember_case("C1", "T2", state={"_channel_team": {"C1": "T1"},
                                "_channel_teams": {"C1": st("T1")}})
remember_case("C1", "T1", state={"_channel_team": {"C1": "T1"},
                                 "_channel_teams": {"C1": st("T1")}})
remember_case("", "T1")
remember_case("C1", "")
remember_case(None, None)
remember_case(5, 7, state={"_channel_team": {}})
remember_case("C2", "T1", state={"_channel_team": {"C1": "T1"},
                                 "_channel_teams": {"C1": st("T1")}})
remember_case("C1", "T3", state={"_channel_team": {"C1": "T1", "C0": "T0"},
                                 "_channel_teams": {"C1": st("T1", "T2")}})
remember_case("C1", "T1", state={"_channel_team": {}, "_channel_teams": {}})

# ── _pop_slash_context ──
def ctxs(*pairs):
    return [{"key": list(k), "value": v} for k, v in pairs]


def pop_case(chat_id, team_id, contexts, uid, now=1000.25, io_extra=None, expect_raise=None):
    io = {"time.monotonic": [now]}
    if io_extra:
        io.update(io_extra)
    case("_pop_slash_context", {"chat_id": chat_id, "team_id": team_id}, io=io,
         state={"slash_command_contexts": contexts, "slash_user_id": uid},
         expect_raise=expect_raise)


pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 990.0, "response_url": "u1"})), "U1")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 990.0})), "U2")
pop_case("C1", "", ctxs((("C1", "U1"), {"ts": 990.0})), "U1")
pop_case("C1", "T1", ctxs((("C1", "U1"), {"ts": 990.0})), "U1")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 990.0})), None)
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 990.0})), "")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 800.0})), "U1")  # stale purge
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 880.25})), "U1")  # exactly TTL boundary
pop_case("C1", None, ctxs((("C1", "U1"), {"ts": 990.0})), "U1")
pop_case("C1", "T1", ctxs(), "U1")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": 990.0}), (("T1", "C1", "U2"), {"ts": 995.0})), "U1")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"no_ts": 1})), "U1", expect_raise="KeyError")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), "not-dict")), "U1", expect_raise="TypeError")
pop_case("C1", "T1", ctxs((("T1", "C1", "U1"), {"ts": "old"})), "U1", expect_raise="TypeError")
pop_case("C1", "T1", ctxs(), "U1", io_extra={"time.monotonic": []}, expect_raise="RuntimeError")  # fail-closed exhausted
pop_case("C1", "T1", ctxs(), "U1", io_extra={"time.monotonic": [script(raise_spec=rs("SystemExit", "bye"))]},
         expect_raise="SystemExit")

# ── _purge_stale_slash_contexts ──
def purge_case(contexts, now=1000.25, io_extra=None, expect_raise=None):
    io = {"time.monotonic": [now]}
    if io_extra:
        io.update(io_extra)
    case("_purge_stale_slash_contexts", {}, io=io,
         state={"slash_command_contexts": contexts}, expect_raise=expect_raise)


purge_case(ctxs())
purge_case(ctxs((("T1", "C1", "U1"), {"ts": 990.0})))
purge_case(ctxs((("T1", "C1", "U1"), {"ts": 800.0})))
purge_case(ctxs((("T1", "C1", "U1"), {"ts": 880.25})))
purge_case(ctxs((("T1", "C1", "U1"), {"ts": 879.0}), (("C2", "U2"), {"ts": 999.0}),
                (("T1", "C3", "U3"), {"ts": 100.0})))
purge_case(ctxs((("T1", "C1", "U1"), {"ts": 1001.0})))  # future ts
purge_case(ctxs((("T1", "C1", "U1"), None)), expect_raise="TypeError")
purge_case(ctxs((("T1", "C1", "U1"), {"ts": None})), expect_raise="TypeError")
purge_case(ctxs((("T1", "C1", "U1"), [1])), expect_raise="TypeError")
purge_case(ctxs(), io_extra={"time.monotonic": []}, expect_raise="RuntimeError")  # fail-closed

# ── scope_id_for_chat ──
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": {"C1": "T1"}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": {"C1": ""}, "_team_clients": {"T9": {}}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": {}, "_channel_teams": {"C1": st("T1", "T2")}, "_team_clients": {"T9": {}}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": {}, "_channel_teams": {}, "_team_clients": {"T9": {}}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": {}, "_channel_teams": {}, "_team_clients": {"T9": {}, "T8": {}}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={})
case("scope_id_for_chat", {"chat_id": ""}, state={"_team_clients": {"T9": {}}})
case("scope_id_for_chat", {"chat_id": None}, state={"_team_clients": {"T9": {}}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": {"C1": 5}})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_team": None, "_channel_teams": None, "_team_clients": None})
case("scope_id_for_chat", {"chat_id": "C1"}, state={"_channel_teams": {"C1": st("T1")}})

# ── _get_client ──
CL1 = obj(client_id="c1")
CL2 = obj(client_id="c2")
PRIMARY = obj(client_id="primary")
case("_get_client", {"chat_id": "C1", "team_id": "T1"},
     state={"_team_clients": {"T1": CL1}, "_channel_team": {}, "_app": obj(client=PRIMARY)})
case("_get_client", {"chat_id": "C1", "team_id": "T9"},
     state={"_team_clients": {"T1": CL1}, "_channel_team": {"C1": "T1"}, "_app": obj(client=PRIMARY)})
case("_get_client", {"chat_id": "C1"},
     state={"_team_clients": {"T1": CL1, "T2": CL2}, "_channel_team": {"C1": "T2"}, "_app": obj(client=PRIMARY)})
case("_get_client", {"chat_id": "CX"},
     state={"_team_clients": {"T1": CL1}, "_channel_team": {"C1": "T1"}, "_app": obj(client=PRIMARY)})
case("_get_client", {"chat_id": "C1", "team_id": ""},
     state={"_team_clients": {"T1": CL1}, "_channel_team": {"C1": "T1"}, "_app": obj(client=PRIMARY)})
case("_get_client", {"chat_id": "C1", "team_id": "T1"},
     state={"_team_clients": {}, "_channel_team": {}, "_app": obj(client=PRIMARY)})
case("_get_client", {"chat_id": "C1"},
     state={"_team_clients": {}, "_channel_team": {}, "_app": None}, expect_raise="AttributeError")
case("_get_client", {"chat_id": "C1"},
     state={"_team_clients": {}, "_channel_team": {}, "_app": obj(other=1)}, expect_raise="AttributeError")
case("_get_client", {"chat_id": "C1", "team_id": "T1"},
     state={"_channel_team": {}, "_app": obj(client=PRIMARY)}, expect_raise="AttributeError")
case("_get_client", {"chat_id": "C1", "team_id": "T1"},
     state={"_team_clients": {"T1": CL1}, "_channel_team": {"C1": "T2"}, "_app": obj(client=PRIMARY)})

# ── _is_ignored_channel ──
case("_is_ignored_channel", {"chat_id": ""}, io={"_slack_ignored_channels": [["C1"]]})
case("_is_ignored_channel", {"chat_id": None})
case("_is_ignored_channel", {"chat_id": "C1"}, io={"_slack_ignored_channels": [["C1", "C2"]]})
case("_is_ignored_channel", {"chat_id": "C1"}, io={"_slack_ignored_channels": [["*"]]})
case("_is_ignored_channel", {"chat_id": "C1:1712345678.000001"}, io={"_slack_ignored_channels": [["C1"]]})
case("_is_ignored_channel", {"chat_id": "C1:1712345678.000001"}, io={"_slack_ignored_channels": [["C1:1712345678.000001"]]})
case("_is_ignored_channel", {"chat_id": "C2"}, io={"_slack_ignored_channels": [[]]})
case("_is_ignored_channel", {"chat_id": 5}, io={"_slack_ignored_channels": [["5"]]})
case("_is_ignored_channel", {"chat_id": "C1"}, io={}, expect_raise="RuntimeError")  # fail-closed
case("_is_ignored_channel", {"chat_id": "C1"},
     io={"_slack_ignored_channels": [script(raise_spec=rs("KeyboardInterrupt", "stop"))]},
     expect_raise="KeyboardInterrupt")

# ── native_task_cards_enabled ──
def ntc_case(extra):
    case("native_task_cards_enabled", {}, state={"config": obj(extra=extra)})


ntc_case({})
ntc_case({"native_task_cards": True})
ntc_case({"native_task_cards": "yes"})
ntc_case({"nativeTaskCards": "on"})
ntc_case({"native_task_cards": None, "nativeTaskCards": "1"})
ntc_case({"native_task_cards": None})
ntc_case({"streaming": {"progress": {"native_task_cards": True}}})
ntc_case({"streaming": {"progress": {"nativeTaskCards": "off"}}})
ntc_case({"streaming": "not-dict"})
ntc_case({"streaming": {"progress": "not-dict"}, "native_task_cards": "0"})
ntc_case("not-dict-extra")
ntc_case(None)
ntc_case({"streaming": {"progress": {"native_task_cards": False}}, "native_task_cards": True})
ntc_case({"native_task_cards": 0})

# ── _describe_slack_api_error ──
def desc_case(response, file_obj=None, label="file.txt", io_extra=None, expect_raise=None):
    io = {"_attachment_label": [label]}
    if io_extra:
        io.update(io_extra)
    case("_describe_slack_api_error", {"response": response, "file_obj": file_obj}, io=io,
         expect_raise=expect_raise)


desc_case(None)
desc_case("not-a-dict")
desc_case(obj(error="invalid_auth"))
desc_case({})
desc_case({"error": ""})
desc_case({"error": "  "})
desc_case({"error": "invalid_auth"}, file_obj={"name": "a.png"})
desc_case({"error": "not_authed"})
desc_case({"error": "file_not_found"})
desc_case({"error": "access_denied"})
desc_case({"error": "missing_scope", "needed": "files:read", "provided": "chat:write"})
desc_case({"error": "missing_scope"})
desc_case({"error": "missing_scope", "needed": " files:read ", "provided": ""})
desc_case({"error": "unknown_code"})
desc_case({"error": 5})
desc_case({"error": "invalid_auth"}, io_extra={"_attachment_label": []})  # fail-closed exhausted
CASES[-1]["expect_raise"] = "RuntimeError"
desc_case({"error": "invalid_auth"},
          io_extra={"_attachment_label": [script(raise_spec=rs("GeneratorExit", "gone"))]},
          expect_raise="GeneratorExit")

# ── _warn_if_not_bot_token ──
def warn_case(auth_response, team_name, state=None, io=None, expect_raise=None):
    case("_warn_if_not_bot_token", {"auth_response": auth_response, "team_name": team_name},
         io=io if io is not None else {"logger.warning": [None]},
         state=state or {}, expect_raise=expect_raise)


warn_case({"user_id": "U1", "bot_id": ""}, "TeamA")
warn_case({"user_id": "U1", "bot_id": "B1"}, "TeamA")
warn_case({"user_id": "", "bot_id": ""}, "TeamA")
warn_case({"user_id": "U1"}, "")
warn_case({"user_id": "U1"}, "TeamA", state={"_user_token_warned": st("TeamA")})
warn_case(obj(data={"user_id": "U2"}), "TeamB")
warn_case(obj(data={"user_id": "U2", "bot_id": "B2"}), "TeamB")
warn_case(obj(other=1), "TeamB")
warn_case(obj(data={"user_id": "U2"}), "TeamB", state={"_user_token_warned": st("TeamB")})
warn_case(None, "TeamC")
warn_case("string-response", "TeamC")
warn_case(obj(data="not-dict"), "TeamC")
warn_case({"user_id": "U1"}, "TeamA", io={})  # fail-closed logger swallowed
warn_case({"user_id": "U1"}, "TeamA",
          io={"logger.warning": [script(raise_spec=rs("RuntimeError", "log fail"))]})
warn_case({"user_id": "U1"}, "TeamA",
          io={"logger.warning": [script(raise_spec=rs("SystemExit", "bye"))]},
          expect_raise="SystemExit")
warn_case({"user_id": "U1"}, "TeamA",
          io={"logger.warning": [script(raise_spec=rs("CancelledError", "cancel"))]},
          expect_raise="CancelledError")
warn_case({"user_id": 5}, "TeamA")
warn_case({"user_id": "U1", "bot_id": 0}, "TeamA")

# ── write ──
def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit(f"THIN funcs (<5 cases): {thin}")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"gen_slack_adapter_flow2: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")


if __name__ == "__main__":
    main()
