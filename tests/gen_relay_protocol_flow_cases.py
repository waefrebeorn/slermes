#!/usr/bin/env python3
"""Generate tests/fixtures/relay_protocol_flow.jsonl — differential cases for
the relay wire-protocol UNCLAIMED-remainder FLOW port (relay_protocol_flow;
defining module gateway/relay/ws_transport.py).

Each case is {"func": <live_name>, "in": {...}} where in.io carries the
universal-seam scripts and in.state the mutable carrier. >=5 cases per func
exercise the happy path, branch edges, fail-closed (absent script), scripted
raises, and BaseException passthrough through `except Exception` guards.

Domain (ASCII scope): str() operands ASCII scalars; raw/frame/descriptor maps
dict-or-falsy; media_urls/media/identities list-or-falsy; platform dict-key
positions str-or-falsy; from_json results carry a platform key; create_task ack
ids fresh vs _inbound_acks; _inbound_acks inputs sorted. EXCLUDED: integral-
FLOAT str()/type-name positions; KeyError raise specs in exc-arg positions;
non-str truthy platform keys; non-list truthy identities.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "relay_protocol_flow.jsonl"
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def rz(kind, msg, **attrs):
    return {"__raise__": kind, "message": msg, **attrs}


def fut(id, done=False, **extra):
    d = {"__future__": True, "id": id, "done": done,
         "result": None, "exception": None, "cancelled": False}
    d.update(extra)
    return d


def task(id, done=False, cancelled=False, exception=None):
    return {"__task__": True, "id": id, "done": done,
            "cancelled": cancelled, "exception": exception}


def ev(id, set=False):
    return {"__event__": True, "id": id, "set": set}


def cb(name):
    return {"__callable__": name}


EV1 = ev("ev1")
TASK1 = task("t1")
ADM1 = task("adm1")
ACK1 = task("ack1")
FUT1 = fut("f1")


# ── _per_url_from_wire ──
case("_per_url_from_wire", key="mime",
     raw={"media_urls": ["u1", "u2"], "media": [{"url": "u1", "mime": "a"}, {"url": "u2", "mime": "b"}]})
case("_per_url_from_wire", key="mime",
     raw={"media_urls": ["u1", "u2", "u1"], "media": [{"url": "u1", "mime": "a"}, {"url": "u1", "mime": "b"}, {"url": "u2", "mime": 5}]})
case("_per_url_from_wire", key="mime",
     raw={"media_urls": ["u1", 7, None], "media": [{"url": "u1", "mime": "a"}]})
case("_per_url_from_wire", key="mime", raw={"media_urls": [], "media": [{"url": "u1"}]})
case("_per_url_from_wire", key="mime", raw={"media_urls": "u1"})
case("_per_url_from_wire", key="mime", raw={"media_urls": ["u1"], "media": {"url": "u1"}})
case("_per_url_from_wire", key="mime",
     raw={"media_urls": ["", "u1"], "media": [{"url": "", "mime": "x"}, {"url": "u1", "mime": "y"}]})
case("_per_url_from_wire", key="filename",
     raw={"media_urls": ["u1"], "media": [{"url": "u1", "filename": "doc.pdf"}]})
case("_per_url_from_wire", key="mime", raw=None)
case("_per_url_from_wire", key="mime", raw={"media_urls": ["u1"], "media": [None, 3, {"url": "u1"}]})

# ── _media_types_from_wire ──
case("_media_types_from_wire", raw={"media_urls": ["u1"], "media": [{"url": "u1", "mime": "a"}]}, io={})
case("_media_types_from_wire", raw={"media_urls": ["u1", "u2"], "media": [{"url": "u1", "mime": "a"}]},
     io={"logger.debug": None})
case("_media_types_from_wire", raw={"media_urls": ["u1"], "media": []}, io={})
case("_media_types_from_wire", raw={"media_urls": ["u1"], "media": None}, io={})
case("_media_types_from_wire", raw={"media_urls": ["u1", "u2"], "media": [{"url": "u1", "mime": "a"}]},
     io={"logger.debug": rz("RuntimeError", "log boom")})
case("_media_types_from_wire", raw={"media_urls": ["u1"], "media": [{"url": "u1"}]},
     io={"logger.debug": rz("KeyboardInterrupt", "ki")})
case("_media_types_from_wire", raw=None, io={})
case("_media_types_from_wire", raw={"media_urls": []}, io={})

# ── _media_filenames_from_wire ──
case("_media_filenames_from_wire",
     raw={"media_urls": ["u1", "u2"], "media": [{"url": "u1", "filename": "a.png"}, {"url": "u2", "filename": "b.png"}]},
     io={"clean_attachment_filename": ["clean1", "clean2"]})
case("_media_filenames_from_wire", raw={"media_urls": ["u1"], "media": [{"url": "u1"}]},
     io={"clean_attachment_filename": [""]})
case("_media_filenames_from_wire",
     raw={"media_urls": ["u1", "u2"], "media": [{"url": "u1", "filename": "a"}, {"url": "u2", "filename": "b"}]},
     io={"clean_attachment_filename": ["ok", rz("ValueError", "bad")]})
case("_media_filenames_from_wire", raw={"media_urls": ["u1"], "media": [{"url": "u1", "filename": "a"}]}, io={})
case("_media_filenames_from_wire", raw={"media_urls": ["u1"], "media": [{"url": "u1", "filename": "a"}]},
     io={"clean_attachment_filename": [rz("SystemExit", "se")]})
case("_media_filenames_from_wire", raw={"media_urls": []}, io={})
case("_media_filenames_from_wire", raw={"media_urls": ["u1"], "media": [{"url": "u1", "filename": "a"}]},
     io={"clean_attachment_filename": [None]})

# ── _await_bounded ──
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": None})
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": rz("TimeoutError", "t")})
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": rz("CancelledError", "c")})
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": rz("RuntimeError", "x")})
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": rz("SystemExit", "se")})
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": rz("KeyboardInterrupt", "ki")})
case("_await_bounded", aw=cb("teardown"), io={"asyncio.wait_for": rz("GeneratorExit", "ge")})
case("_await_bounded", aw=cb("teardown"), io={})
case("_await_bounded", aw=None, io={"asyncio.wait_for": None})

# ── _PendingRequest.__init__ ──
case("_PendingRequest.__init__", op="send", loop={"__loop__": "l1"}, io={"super.__init__": None})
case("_PendingRequest.__init__", op="send_media", loop=None, io={"super.__init__": None})
case("_PendingRequest.__init__", op="send", loop={"__loop__": "l1"}, io={"super.__init__": rz("RuntimeError", "boom")})
case("_PendingRequest.__init__", op="send", loop={"__loop__": "l1"}, io={})
case("_PendingRequest.__init__", op="send", loop={"__loop__": "l1"}, io={"super.__init__": rz("GeneratorExit", "ge")})
case("_PendingRequest.__init__", op="", loop=None, io={"super.__init__": None})

# ── __init__ (WebSocketRelayTransport) ──
case("__init__", url="https://c.x", platform="discord", bot_id="b1",
     io={"_ws_dial_url": "wss://c.x/relay", "asyncio.Event": EV1})
case("__init__", url="https://c.x", platform="discord", bot_id="b1",
     identities=[["discord", "b1"], ["slack", "b2"]], connect_timeout_s=5, outbound_timeout_s=7,
     gateway_id="g1", upgrade_secret="s3cr3t", reconnect=True, reconnect_backoff_s=2, reconnect_max_backoff_s=20,
     io={"_ws_dial_url": "wss://c.x/relay", "asyncio.Event": EV1})
case("__init__", url="https://c.x", platform="discord", bot_id="b1", websockets_available=False, io={})
case("__init__", url="https://c.x", platform="discord", bot_id="b1",
     io={"_ws_dial_url": rz("ValueError", "bad url"), "asyncio.Event": EV1})
case("__init__", url="https://c.x", platform="discord", bot_id="b1", io={"asyncio.Event": EV1})
case("__init__", url="https://c.x", platform="discord", bot_id="b1",
     io={"_ws_dial_url": "wss://c.x/relay", "asyncio.Event": rz("RuntimeError", "ev boom")})
case("__init__", url="https://c.x", platform="discord", bot_id="b1",
     io={"_ws_dial_url": "wss://c.x/relay", "asyncio.Event": rz("SystemExit", "se")})
case("__init__", url="http://c.x", platform="slack", bot_id="b2", identities=[],
     io={"_ws_dial_url": "ws://c.x/relay", "asyncio.Event": EV1})
case("__init__", url="https://c.x", platform="discord", bot_id="b1", connect_timeout_s=None,
     io={"_ws_dial_url": "wss://c.x/relay", "asyncio.Event": EV1})

# ── _fail_pending ──
case("_fail_pending", settle=cb("settle_fn"), state={"_pending": {}}, io={"settle_fn": None})
case("_fail_pending", settle=cb("settle_fn"), state={"_pending": {"r1": fut("f1")}}, io={"settle_fn": None})
case("_fail_pending", settle=cb("settle_fn"), state={"_pending": {"r1": fut("f1", done=True)}}, io={})
case("_fail_pending", settle=cb("settle_fn"),
     state={"_pending": {"r1": fut("f1", done=True), "r2": fut("f2")}}, io={"settle_fn": None})
case("_fail_pending", settle={"__self__": "_settle_connection_lost"},
     state={"_pending": {"r1": fut("f1", op="send")}}, io={"future.set_result": None})
case("_fail_pending", settle={"__self__": "_settle_connection_lost"},
     state={"_pending": {"r1": fut("f1", op="send_media")}}, io={"future.set_result": None})
case("_fail_pending", settle=cb("settle_fn"), state={"_pending": {"r1": fut("f1")}},
     io={"settle_fn": rz("RuntimeError", "s boom")})
case("_fail_pending", settle={"__self__": "_settle_connection_lost"},
     state={"_pending": {"r1": fut("f1")}}, io={"future.set_result": rz("ValueError", "v")})
case("_fail_pending", settle=cb("settle_fn"), state={"_pending": {"r1": fut("f1")}}, io={})
case("_fail_pending", settle=cb("settle_fn"), state={"_pending": {"r1": fut("f1")}},
     io={"settle_fn": rz("KeyboardInterrupt", "ki")})

# ── _settle_connection_lost ──
case("_settle_connection_lost", fut=fut("f1", op="send"), io={"future.set_result": None})
case("_settle_connection_lost", fut=fut("f1", op="send_media"), io={"future.set_result": None})
case("_settle_connection_lost", fut=fut("f1"), io={"future.set_result": None})
case("_settle_connection_lost", fut=fut("f1", op=None), io={"future.set_result": None})
case("_settle_connection_lost", fut=fut("f1", op="send"), io={"future.set_result": rz("RuntimeError", "boom")})
case("_settle_connection_lost", fut=fut("f1", op="send"), io={})
case("_settle_connection_lost", fut=fut("f1", op="send"), io={"future.set_result": rz("SystemExit", "se")})
case("_settle_connection_lost", fut=None, io={})

# ── _dialer_running ──
case("_dialer_running", state={"_supervisor": None})
case("_dialer_running", state={"_supervisor": task("t1")})
case("_dialer_running", state={"_supervisor": task("t1", done=True)})
case("_dialer_running", state={"_supervisor": task("t1", done=True), "_auth_retry": task("t2")})
case("_dialer_running", state={"_supervisor": None, "_auth_retry": task("t2", done=True)})
case("_dialer_running", state={"_supervisor": None, "_auth_retry": None})
case("_dialer_running", state={"_supervisor": task("t1"), "_auth_retry": task("t2")})

# ── _close_reason_of ──
case("_close_reason_of", exc=rz("ConnectionError", "closed", rcvd={"code": 4401, "reason": "expired"}))
case("_close_reason_of", exc=rz("ConnectionError", "closed", rcvd={"code": 4401}, sent={"reason": "bye"}))
case("_close_reason_of", exc=rz("ConnectionError", "closed", rcvd={"reason": 5}, sent={"reason": "x"}))
case("_close_reason_of", exc=rz("ConnectionError", "closed", reason="unauthorized"))
case("_close_reason_of", exc=rz("ConnectionError", "closed"))
case("_close_reason_of", exc=None)
case("_close_reason_of", exc=rz("ConnectionError", "closed", reason=7))
case("_close_reason_of", exc=rz("ConnectionError", "closed", rcvd={"reason": ""}))
case("_close_reason_of", exc=rz("ConnectionError", "closed", rcvd=5, reason="fallback"))

# ── _latch_if_fresh_token_refused ──
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": False}, io={})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x", rcvd={"reason": "expired"}),
     state={"_auth_retry_pending": True}, io={"_close_code_of": 4401})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x", reason="unauthorized"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": 4401, "logger.warning": None})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": 4401, "logger.warning": None})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": 1006})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": None})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": rz("RuntimeError", "cc")})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": 4401, "logger.warning": rz("RuntimeError", "lw")})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": rz("SystemExit", "se")})
case("_latch_if_fresh_token_refused", exc=rz("ConnectionError", "x"),
     state={"_auth_retry_pending": True}, io={"_close_code_of": 4401.0, "logger.warning": None})

# ── hold_redial ──
case("hold_redial", state={"_redial_release": EV1}, io={"event.clear": None})
case("hold_redial", state={"_redial_release": ev("ev1", set=True)}, io={"event.clear": None})
case("hold_redial", state={"_redial_release": EV1}, io={"event.clear": rz("RuntimeError", "boom")})
case("hold_redial", state={"_redial_release": EV1}, io={})
case("hold_redial", state={"_redial_release": EV1}, io={"event.clear": rz("SystemExit", "se")})
case("hold_redial", state={"_redial_held": True, "_redial_release": EV1}, io={"event.clear": None})

# ── release_redial ──
case("release_redial", state={"_redial_held": True, "_redial_release": EV1}, io={"event.set": None})
case("release_redial", state={"_redial_held": True, "_redial_release": EV1}, io={"event.set": rz("RuntimeError", "boom")})
case("release_redial", state={"_redial_held": True, "_redial_release": EV1}, io={})
case("release_redial", state={"_redial_held": True, "_redial_release": EV1}, io={"event.set": rz("KeyboardInterrupt", "ki")})
case("release_redial", state={"_redial_release": ev("ev1", set=True)}, io={"event.set": None})

# ── _await_redial_hold ──
case("_await_redial_hold", state={"_redial_held": False}, io={})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": None, "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": rz("TimeoutError", "t"), "logger.info": None, "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": rz("RuntimeError", "x"), "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": rz("SystemExit", "se"), "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": rz("TimeoutError", "wt"), "logger.info": None, "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": rz("RuntimeError", "wr"), "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": None, "event.clear": rz("RuntimeError", "clear boom")})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": rz("TimeoutError", "t"), "logger.info": rz("RuntimeError", "li"), "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1}, io={})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1},
     io={"event.wait": None, "asyncio.wait_for": rz("CancelledError", "c"), "event.clear": None})
case("_await_redial_hold", state={"_redial_held": True, "_redial_release": EV1, "_redial_hold_max_s": 5},
     io={"event.wait": None, "asyncio.wait_for": None, "event.clear": None})

# ── _redial_with_fresh_token ──
case("_redial_with_fresh_token", state={"_closing": True}, io={})
case("_redial_with_fresh_token", state={}, io={"_dial_and_start": None, "logger.info": None})
case("_redial_with_fresh_token", state={"_reconnect": True},
     io={"_dial_and_start": rz("ConnectionError", "4401"), "logger.warning": None, "asyncio.create_task": TASK1})
case("_redial_with_fresh_token", state={"_auth_retry_pending": True, "_reconnect": True},
     io={"_dial_and_start": rz("ConnectionError", "x", reason="unauthorized"), "_close_code_of": 4401, "logger.warning": None})
case("_redial_with_fresh_token", state={"_reconnect": False},
     io={"_dial_and_start": rz("ConnectionError", "x"), "logger.warning": None})
case("_redial_with_fresh_token", state={"_reconnect": True, "_supervisor": task("t9")},
     io={"_dial_and_start": rz("ConnectionError", "x"), "logger.warning": None})
case("_redial_with_fresh_token", state={}, io={"_dial_and_start": rz("CancelledError", "c")})
case("_redial_with_fresh_token", state={}, io={"_dial_and_start": rz("SystemExit", "se")})
case("_redial_with_fresh_token", state={"_reconnect": False},
     io={"_dial_and_start": None, "logger.info": rz("RuntimeError", "log"), "logger.warning": None})
case("_redial_with_fresh_token", state={}, io={})
case("_redial_with_fresh_token", state={"_reconnect": True},
     io={"_dial_and_start": rz("ConnectionError", "x"), "logger.warning": None, "asyncio.create_task": rz("RuntimeError", "ct")})
case("_redial_with_fresh_token", state={"_closing": True},
     io={"_dial_and_start": rz("ConnectionError", "x")})
case("_redial_with_fresh_token", state={"_reconnect": True, "_supervisor": task("t9", done=True)},
     io={"_dial_and_start": rz("ConnectionError", "x"), "logger.warning": None, "asyncio.create_task": TASK1})

# ── _on_descriptor ──
case("_on_descriptor", frame={"descriptor": {"platform": "discord", "caps": 1}},
     state={"_descriptor_ready": FUT1},
     io={"json.dumps": "{\"platform\": \"discord\", \"caps\": 1}",
         "CapabilityDescriptor.from_json": {"platform": "discord", "caps": 1}, "future.set_result": None})
case("_on_descriptor", frame={"descriptor": {"platform": "slack"}},
     state={"_descriptor": {"platform": "discord"}, "_descriptors_by_platform": {"discord": {"platform": "discord"}}},
     io={"json.dumps": "{\"platform\": \"slack\"}", "CapabilityDescriptor.from_json": {"platform": "slack"}})
case("_on_descriptor", frame={"descriptor": {"platform": ""}},
     io={"json.dumps": "{\"platform\": \"\"}", "CapabilityDescriptor.from_json": {"platform": ""}})
case("_on_descriptor", frame={},
     io={"json.dumps": "{}", "CapabilityDescriptor.from_json": {"platform": "relay"}})
case("_on_descriptor", frame={"descriptor": None},
     io={"json.dumps": "null", "CapabilityDescriptor.from_json": {"platform": "relay"}})
case("_on_descriptor", frame={"descriptor": {"platform": "discord"}},
     io={"json.dumps": "s", "CapabilityDescriptor.from_json": rz("ValueError", "bad desc")})
case("_on_descriptor", frame={"descriptor": {"platform": "discord"}}, io={"CapabilityDescriptor.from_json": {"platform": "discord"}})
case("_on_descriptor", frame={"descriptor": {"platform": "discord"}},
     state={"_descriptor_ready": fut("f1", done=True)},
     io={"json.dumps": "s", "CapabilityDescriptor.from_json": {"platform": "discord"}})
case("_on_descriptor", frame={"descriptor": {"platform": "discord"}},
     state={"_descriptor_ready": None},
     io={"json.dumps": "s", "CapabilityDescriptor.from_json": {"platform": "discord"}})
case("_on_descriptor", frame={"descriptor": {"platform": "discord"}},
     state={"_descriptor_ready": FUT1},
     io={"json.dumps": "s", "CapabilityDescriptor.from_json": {"platform": "discord"}, "future.set_result": rz("RuntimeError", "sr")})
case("_on_descriptor", frame={"descriptor": {"caps": 1}},
     io={"json.dumps": "s", "CapabilityDescriptor.from_json": {"caps": 1}})
case("_on_descriptor", frame={"descriptor": {"platform": "discord"}},
     io={"json.dumps": "s", "CapabilityDescriptor.from_json": rz("SystemExit", "se")})
case("_on_descriptor", frame=None, io={})

# ── _on_inbound ──
case("_on_inbound", frame={"event": {"text": "hi"}}, state={"_inbound": None}, io={})
case("_on_inbound", frame={"event": {"text": "hi"}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {"text": "hi"}, "relay_callback": None, "inbound_handler": ADM1, "task.add_done_callback": None})
case("_on_inbound", frame={"event": {"text": "hi"}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {"text": "hi"}, "relay_callback": None, "inbound_handler": None, "_send_inbound_ack": None})
case("_on_inbound", frame={"event": {"text": "hi"}}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {"text": "hi"}, "relay_callback": None, "inbound_handler": None})
case("_on_inbound", frame={"event": {}, "bufferId": 7}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": None, "_send_inbound_ack": None})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": rz("ValueError", "h"), "logger.exception": None})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": rz("SystemExit", "se")})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": rz("RuntimeError", "rc"), "logger.exception": None})
case("_on_inbound", frame={"event": {}}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": rz("RuntimeError", "e")})
case("_on_inbound", frame={"event": {}}, state={"_inbound": cb("inbound_handler")}, io={})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": ADM1, "task.add_done_callback": rz("RuntimeError", "cb")})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": None, "_send_inbound_ack": rz("RuntimeError", "ack")})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": "x"})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": rz("ValueError", "h"), "logger.exception": rz("RuntimeError", "le")})
case("_on_inbound", frame={"event": {}, "bufferId": ""}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": None, "inbound_handler": None})
case("_on_inbound", frame={"event": {}, "bufferId": "b1"}, state={"_inbound": cb("inbound_handler")},
     io={"_event_from_wire": {}, "relay_callback": rz("KeyboardInterrupt", "ki")})

# ── _ack_admitted ──
case("_ack_admitted", admission=task("a1", done=True, cancelled=True), buffer_id="b1", io={})
case("_ack_admitted", admission=task("a1", done=True, exception=rz("ValueError", "bad")), buffer_id="b1",
     io={"logger.error": None})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="b1",
     io={"asyncio.create_task": ACK1, "task.add_done_callback": None})
case("_ack_admitted", admission=task("a1", done=True), buffer_id=None, io={})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="", io={})
case("_ack_admitted", admission=task("a1", done=True), buffer_id=9,
     io={"asyncio.create_task": ACK1, "task.add_done_callback": None})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="b1",
     io={"asyncio.create_task": rz("RuntimeError", "ct")})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="b1",
     io={"asyncio.create_task": ACK1, "task.add_done_callback": rz("RuntimeError", "cb")})
case("_ack_admitted", admission=task("a1", done=True, exception=rz("ValueError", "bad")), buffer_id="b1",
     io={"logger.error": rz("RuntimeError", "le")})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="b1", io={})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="b1",
     io={"asyncio.create_task": rz("SystemExit", "se")})
case("_ack_admitted", admission=task("a1", done=True), buffer_id="b1", state={"_inbound_acks": ["a0"]},
     io={"asyncio.create_task": ACK1, "task.add_done_callback": None})
case("_ack_admitted", admission=task("a1", done=True, cancelled=True, exception=rz("ValueError", "bad")), buffer_id="b1", io={})
case("_ack_admitted", admission=task("a1", done=True, exception=rz("ConnectionError", "closed", rcvd={"code": 4401})),
     buffer_id=None, io={"logger.error": None})

# ── _on_going_idle_ack ──
case("_on_going_idle_ack", frame={}, state={"_going_idle_ack": None}, io={})
case("_on_going_idle_ack", frame={}, state={"_going_idle_ack": FUT1}, io={"future.set_result": None})
case("_on_going_idle_ack", frame={}, state={"_going_idle_ack": fut("f1", done=True)}, io={})
case("_on_going_idle_ack", frame={}, state={"_going_idle_ack": FUT1}, io={"future.set_result": rz("RuntimeError", "boom")})
case("_on_going_idle_ack", frame={}, state={"_going_idle_ack": FUT1}, io={})
case("_on_going_idle_ack", frame={}, state={"_going_idle_ack": FUT1}, io={"future.set_result": rz("SystemExit", "se")})

# ── _on_outbound_result ──
case("_on_outbound_result", frame={"requestId": "r1", "result": {"success": True}},
     state={"_pending": {"r1": fut("f1")}}, io={"future.set_result": None})
case("_on_outbound_result", frame={"result": {"success": True}},
     state={"_pending": {"": fut("f0")}}, io={"future.set_result": None})
case("_on_outbound_result", frame={"requestId": "zz", "result": {}},
     state={"_pending": {"r1": fut("f1")}}, io={})
case("_on_outbound_result", frame={"requestId": "r1", "result": {}},
     state={"_pending": {"r1": fut("f1", done=True)}}, io={})
case("_on_outbound_result", frame={"requestId": "r1"},
     state={"_pending": {"r1": fut("f1")}}, io={"future.set_result": None})
case("_on_outbound_result", frame={"requestId": "r1", "result": None},
     state={"_pending": {"r1": fut("f1")}}, io={"future.set_result": None})
case("_on_outbound_result", frame={"requestId": "r1", "result": {}},
     state={"_pending": {"r1": fut("f1")}}, io={"future.set_result": rz("RuntimeError", "boom")})
case("_on_outbound_result", frame={"requestId": "r1", "result": {}},
     state={"_pending": {"r1": fut("f1")}}, io={})
case("_on_outbound_result", frame={"requestId": "r1", "result": {}},
     state={"_pending": {"r1": fut("f1")}}, io={"future.set_result": rz("KeyboardInterrupt", "ki")})
case("_on_outbound_result", frame=None, io={})
case("_on_outbound_result", frame={"requestId": None, "result": {}},
     state={"_pending": {"r1": fut("f1")}}, io={})

# ── _on_interrupt_inbound ──
case("_on_interrupt_inbound", frame={"session_key": "sk", "chat_id": "c1"},
     state={"_interrupt_inbound_handler": None}, io={})
case("_on_interrupt_inbound", frame={"session_key": "sk", "chat_id": "c1"},
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={"interrupt_handler": None})
case("_on_interrupt_inbound", frame={},
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={"interrupt_handler": None})
case("_on_interrupt_inbound", frame={"session_key": None, "chat_id": "c1"},
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={"interrupt_handler": None})
case("_on_interrupt_inbound", frame={"session_key": "sk"},
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={"interrupt_handler": rz("RuntimeError", "h")})
case("_on_interrupt_inbound", frame={},
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={})
case("_on_interrupt_inbound", frame={},
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={"interrupt_handler": rz("SystemExit", "se")})
case("_on_interrupt_inbound", frame=None,
     state={"_interrupt_inbound_handler": cb("interrupt_handler")}, io={})

# ── _on_passthrough_forward ──
case("_on_passthrough_forward", frame={"forward": {"path": "/p"}},
     state={"_passthrough_handler": None}, io={})
case("_on_passthrough_forward", frame={"forward": {"path": "/p"}, "bufferId": "b"},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": {"path": "/p"}, "relay_callback": None, "passthrough_handler": None})
case("_on_passthrough_forward", frame={"forward": {"path": "/p"}},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": {"path": "/p"}, "relay_callback": None, "passthrough_handler": None})
case("_on_passthrough_forward", frame={"forward": {}},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": rz("ValueError", "bad fwd")})
case("_on_passthrough_forward", frame={"forward": {}},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": {"path": "/p"}, "relay_callback": rz("RuntimeError", "rc")})
case("_on_passthrough_forward", frame={"forward": {}, "bufferId": "b"},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": {"path": "/p"}, "relay_callback": None, "passthrough_handler": rz("RuntimeError", "h")})
case("_on_passthrough_forward", frame={"forward": {}},
     state={"_passthrough_handler": cb("passthrough_handler")}, io={})
case("_on_passthrough_forward", frame={"forward": {}, "bufferId": "b"},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": {"path": "/p"}, "relay_callback": None, "passthrough_handler": rz("SystemExit", "se")})
case("_on_passthrough_forward", frame={},
     state={"_passthrough_handler": cb("passthrough_handler")},
     io={"_passthrough_from_wire": {"path": "/p"}, "relay_callback": None, "passthrough_handler": None})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func coverage check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"gen_relay_protocol_flow_cases: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    if short:
        raise SystemExit(f"FUNCS WITH <5 CASES: {short}")
    print("per-func:", json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
