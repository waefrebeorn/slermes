#!/usr/bin/env python3
"""Generate tests/base_flow_cases.h — embedded JSONL cases for t_port_base_flow.
3-6 cases per ported function, edges included. Inputs are consumed identically
by the C harness and tests/oracle_base_flow.py (which drives LIVE quarry code).
"""
import json
from pathlib import Path

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def st(**kw):
    base = {"platform": {"value": "telegram", "name": "TELEGRAM"}}
    base.update(kw)
    return base


def event(**kw):
    base = {"text": "hi", "message_type": "text", "internal": False,
            "allow_gateway_control": False,
            "source": {"platform": "telegram", "chat_id": "c1", "chat_type": "dm"}}
    base.update(kw)
    return base


# ── or_default ──
add("or_default", thunk={"value": 5, "exc": 0}, default=9, exc_mask=3)
add("or_default", thunk={"value": None, "exc": 2}, default=9, exc_mask=3)
add("or_default", thunk={"value": None, "exc": 1}, default=9, exc_mask=3)
add("or_default", thunk={"value": None, "exc": 3}, default=9, exc_mask=7)
add("or_default", thunk={"value": None, "exc": 3}, default=9, exc_mask=3)  # KeyError uncaught
add("or_default", thunk={"value": "x", "exc": 0}, default=9, exc_mask=3)

# ── thread_metadata_for_source ──
add("thread_metadata_for_source", source={"platform": "telegram", "thread_id": "5", "chat_type": "dm", "message_id": "m1"}, reply_to=None)
add("thread_metadata_for_source", source={"platform": "telegram", "thread_id": "1", "chat_type": "dm"}, reply_to="r9")
add("thread_metadata_for_source", source={"platform": "slack", "scope_id": "T1", "thread_id": "t"}, reply_to=None)
add("thread_metadata_for_source", source={"platform": "discord"}, reply_to=None)
add("thread_metadata_for_source", source={"platform": "telegram", "thread_id": "7", "chat_type": "group", "profile": " p2 "}, reply_to=None)
add("thread_metadata_for_source", source=None, reply_to=None)

# ── thread_metadata_for_event ──
add("thread_metadata_for_event", event=event(source={"platform": "telegram", "thread_id": "3", "chat_type": "dm", "message_id": "m"}), seams={"reply_anchor": "a1"})
add("thread_metadata_for_event", event=event(source={"platform": "slack", "scope_id": "T"}), seams={"reply_anchor": None})
add("thread_metadata_for_event", event=event(source={"platform": "discord"}), seams={"reply_anchor": None})

# ── mark_notify_metadata ──
add("mark_notify_metadata", metadata={"thread_id": "1"})
add("mark_notify_metadata", metadata=None)
add("mark_notify_metadata", metadata={})
add("mark_notify_metadata", metadata={"notify": False, "x": 1})

# ── media_failure_text ──
add("media_failure_text", kind="audio", file_name=None, seams={"t": ["Audio", "failed audio"]})
add("media_failure_text", kind="file", file_name="r.pdf", seams={"t": ["File", "failed r.pdf"]})
add("media_failure_text", kind="weird", file_name=None, seams={"t": ["failed weird"]})
add("media_failure_text", kind="image", file_name="p.png", seams={"t": ["Image", "failed p.png"]})

# ── should_send_media_as_audio ──
add("should_send_media_as_audio", platform="telegram", ext=".mp3", is_voice=False)
add("should_send_media_as_audio", platform="telegram", ext=".ogg", is_voice=False)
add("should_send_media_as_audio", platform="telegram", ext=".ogg", is_voice=True)
add("should_send_media_as_audio", platform="discord", ext=".flac", is_voice=False)
add("should_send_media_as_audio", platform="telegram", ext=".pdf", is_voice=True)
add("should_send_media_as_audio", platform={"value": "Telegram"}, ext=".M4A", is_voice=False)

# ── custom_unit_to_cp / fit_prefix / prefix_within_utf16_limit ──
add("custom_unit_to_cp", s="hello", budget=10, seams={"measure": [5]})
add("custom_unit_to_cp", s="hello", budget=3, seams={"measure": [5, 3, 2, 3]})
add("custom_unit_to_cp", s="abc", budget=0, seams={"measure": [3, 1, 0, 1]})
add("fit_prefix", text="hello world", budget=5, suffix="...", seams={"measure": [11, 6, 3, 5, 5]})
add("fit_prefix", text="hi", budget=10, suffix="...", seams={"measure": [2]})
add("fit_prefix", text="abcdef", budget=3, suffix="!", seams={"measure": [6, 4, 2, 3]})
add("prefix_within_utf16_limit", s="abc", limit=10)
add("prefix_within_utf16_limit", s="a\U0001f600b", limit=2)
add("prefix_within_utf16_limit", s="\U0001f600\U0001f600", limit=3)
add("prefix_within_utf16_limit", s="hello", limit=0)

# ── has_media_directives / terminal_sentinel_start ──
add("has_media_directives", text="MEDIA:/x.png")
add("has_media_directives", text="[[audio_as_voice]] hi")
add("has_media_directives", text="plain text")
add("has_media_directives", text="[[as_document]]")
add("terminal_sentinel_start", text="done<|eos|>")
add("terminal_sentinel_start", text="done<|eos|><|eos|>  ")
add("terminal_sentinel_start", text="no sentinel")
add("terminal_sentinel_start", text="<|eos|>")

# ── code_spans / blank_spans / delete_spans ──
add("code_spans", content="a `code` b")
add("code_spans", content="```py\nx=1\n``` tail")
add("code_spans", content="no code here")
add("code_spans", content="`a` and `b`")
add("blank_spans", text="abcdef", spans=[[1, 3]])
add("blank_spans", text="a\nb\nc", spans=[[0, 5]])
add("blank_spans", text="abc", spans=[])
add("delete_spans", text="abcdef", spans=[[1, 3]])
add("delete_spans", text="abcdef", spans=[[1, 3], [2, 5]])
add("delete_spans", text="abc", spans=[])
add("delete_spans", text="abcdef", spans=[[4, 6], [0, 2]])

# ── append_text ──
add("append_text", existing="a", new="b")
add("append_text", existing="", new="b")
add("append_text", existing=None, new="b")
add("append_text", existing="a", new=None)
add("append_text", existing=None, new=None)

# ── any_in / classify_send_error ──
add("any_in", blob="hello world", needles=["world"])
add("any_in", blob="hello", needles=["x", "y"])
add("any_in", blob="", needles=["a"])
add("classify_send_error", exc=None, error_text="Message is too long")
add("classify_send_error", exc=None, error_text="Can't parse entities")
add("classify_send_error", exc=None, error_text="Forbidden: bot was blocked")
add("classify_send_error", exc=None, error_text="Bad Request: chat not found")
add("classify_send_error", exc=None, error_text="Flood control exceeded")
add("classify_send_error", exc={"class": "ConnectionError", "str": "connection reset"}, error_text="")
add("classify_send_error", exc=None, error_text="something weird")

# ── split_post_delivery_entry ──
add("split_post_delivery_entry", entry=[3, "cb"])
add("split_post_delivery_entry", entry="bare_cb")
add("split_post_delivery_entry", entry=None)
add("split_post_delivery_entry", entry=[1, 2, 3])

# ── extract_images ──
add("extract_images", content="![alt](https://x.com/a.png) tail")
add("extract_images", content='<img src="https://x.com/b.jpg"> rest')
add("extract_images", content="no images here")
add("extract_images", content="![a](https://x.com/a.png) and ![b](https://y.com/b.gif)")
add("extract_images", content="![notimg](https://x.com/page.html)")

# ── coerce_float_extra ──
add("coerce_float_extra", st=st(config={"extra": {}}), key="k", value="1.5", default=0.3, max_value=2.0)
add("coerce_float_extra", st=st(config={"extra": {}}), key="k", value=None, default=0.3, max_value=2.0)
add("coerce_float_extra", st=st(config={"extra": {}}), key="k", value="9", default=0.3, max_value=2.0, seams={"log": None})
add("coerce_float_extra", st=st(config={"extra": {}}), key="k", value="-1", default=0.3)
add("coerce_float_extra", st=st(config={"extra": {}}), key="k", value="nan", default=0.3)
add("coerce_float_extra", st=st(config={"extra": {}}), key="k", value="abc", default=0.3)

# ── configure_text_batch_delays / text_batch_delay_for ──
add("configure_text_batch_delays", st=st(config={"extra": {}}))
add("configure_text_batch_delays", st=st(config={"extra": {"text_batch_delay_seconds": "0.5", "text_batch_split_delay_seconds": "2"}}))
add("configure_text_batch_delays", st=st(config={"extra": {"text_batch_delay_seconds": "9"}}), seams={"log": None})
add("text_batch_delay_for", st=st(_text_batch_delay_seconds=0.3, _text_batch_split_delay_seconds=1.0), pending={"_last_chunk_len": 4000})
add("text_batch_delay_for", st=st(_text_batch_delay_seconds=0.3, _text_batch_split_delay_seconds=1.0), pending={"_last_chunk_len": 10})
add("text_batch_delay_for", st=st(_text_batch_delay_seconds=0.3, _text_batch_split_delay_seconds=1.0), pending=None)

# ── owner_transport_profile ──
add("owner_transport_profile", st=st(_owner_profile="p2"))
add("owner_transport_profile", st=st(_owner_profile="  "))
add("owner_transport_profile", st=st(_owner_profile=None))
add("owner_transport_profile", st=st())

# ── retry predicates ──
add("is_rate_limited_error", error="Flood wait 10")
add("is_rate_limited_error", error="too long")
add("is_rate_limited_error", error=None)
add("is_timeout_error", error="Read timed out")
add("is_timeout_error", error="ReadTimeout")
add("is_timeout_error", error="connection reset")
add("send_retry_is_final", result={"success": False, "error": "x"})
add("send_retry_is_final", result={"success": True})
add("send_retry_is_final", result={"success": False, "error_kind": "forbidden"})
add("is_partial_delivery", result={"success": False, "raw_response": {"partial_overflow": True}})
add("is_partial_delivery", result={"success": False, "raw_response": {"partial_overflow": False}})
add("is_partial_delivery", result={"success": False, "raw_response": None})
add("is_partial_delivery", result={"success": False})

# ── merge_caption ──
add("merge_caption", existing=None, new="cap")
add("merge_caption", existing="cap", new="cap")
add("merge_caption", existing="a\n\nb", new="b")
add("merge_caption", existing="Meeting agenda", new="Meeting")
add("merge_caption", existing="a", new="b")

# ── requeue_backoff_delay ──
add("requeue_backoff_delay", st=st(_requeue_counts={}), session_key="k",
    pending={"message_id": "m", "timestamp": 1}, dispatched={"message_id": "m", "timestamp": 1})
add("requeue_backoff_delay", st=st(_requeue_counts={"k": 1}), session_key="k",
    pending={"message_id": "m", "timestamp": 1}, dispatched={"message_id": "m", "timestamp": 1}, seams={"log": None})
add("requeue_backoff_delay", st=st(_requeue_counts={"k": 3}), session_key="k",
    pending={"message_id": "m", "timestamp": 1}, dispatched={"message_id": "m", "timestamp": 1}, seams={"log": None})
add("requeue_backoff_delay", st=st(_requeue_counts={"k": 2}), session_key="k",
    pending={"message_id": "n", "timestamp": 2}, dispatched={"message_id": "m", "timestamp": 1})
add("requeue_backoff_delay", st=st(_requeue_counts={}), session_key="k",
    pending={"message_id": None, "timestamp": 5}, dispatched={"message_id": None, "timestamp": 5})

# ── get_human_delay ──
add("get_human_delay", st=st(_human_delay_range_ms=None))
add("get_human_delay", st=st(_human_delay_range_ms=[100, 200]), seams={"uniform": 0.15})
add("get_human_delay", st=st(_human_delay_range_ms=[0, 0]), seams={"uniform": 0.0})

# ── wants_auto_tts ─
add("wants_auto_tts", st=st(), event=event(message_type="voice", source={"chat_id": "c"}),
    session_key="k", interrupt_event={"id": 1, "set": False}, text_content="hi", media_files=[],
    seams={"should_auto_tts": True, "tts_turn_completed": False})
add("wants_auto_tts", st=st(), event=event(message_type="text"), session_key="k",
    interrupt_event={"id": 1, "set": False}, text_content="hi", media_files=[],
    seams={"should_auto_tts": True})
add("wants_auto_tts", st=st(), event=event(message_type="voice"), session_key="k",
    interrupt_event={"id": 1, "set": False}, text_content="hi", media_files=["x"],
    seams={"should_auto_tts": True})
add("wants_auto_tts", st=st(), event=event(message_type="voice"), session_key="k",
    interrupt_event={"id": 1, "set": False}, text_content="hi", media_files=[],
    seams={"should_auto_tts": True, "tts_turn_completed": True})

# ── shared_runtime_owns_input ──
add("shared_runtime_owns_input", st=st(_message_handler_self={"session_authority": True}),
    event=event(), has_handler_self=True, session_authority=True, seams={"get_command": None})
add("shared_runtime_owns_input", st=st(_message_handler_self={"session_authority": True}),
    event=event(), has_handler_self=True, session_authority=True, seams={"get_command": "stop"})
add("shared_runtime_owns_input", st=st(), event=event(), has_handler_self=False, seams={"get_command": None})

# ── clear_session_guard ──
add("clear_session_guard", st=st(_active_sessions={"k": {"id": 1, "set": True}}), session_key="k")
add("clear_session_guard", st=st(_active_sessions={}), session_key="k")
add("clear_session_guard", st=st(_active_sessions={"k": {"id": 1, "set": False}}), session_key="k")

# ── warning policy / text ──
add("warning_notifications_enabled", st=st(), logical_platform=None, seams={"warning_policy": True})
add("warning_notifications_enabled", st=st(), logical_platform={"value": "slack"}, seams={"warning_policy": False})
add("warning_notifications_enabled", st=st(), logical_platform=None, seams={"warning_policy": False})
add("warning_text", st=st(), visible="V", hidden="H", seams={"warning_policy": True})
add("warning_text", st=st(), visible="V", hidden="H", seams={"warning_policy": False})
add("warning_text", st=st(), visible="V", hidden=None, seams={"warning_policy": False})

# ── exec approval actions / choices ──
add("exec_approval_actions", allow_permanent=True, allow_session=True, smart_denied=False,
    seams={"ea_labels": {"once": "Once", "session": "Session", "always": "Always", "deny": "Deny"}})
add("exec_approval_actions", allow_permanent=True, allow_session=True, smart_denied=True,
    seams={"ea_labels": {"once": "Once", "deny": "Deny"}})
add("exec_approval_actions", allow_permanent=False, allow_session=True, smart_denied=False,
    seams={"ea_labels": {"once": "Once", "session": "Session", "deny": "Deny"}})
add("exec_approval_actions", allow_permanent=True, allow_session=False, smart_denied=False,
    seams={"ea_labels": {"once": "Once", "deny": "Deny"}})
add("exec_approval_prompt_choices", actions=[["Once", "once", "primary"], ["Deny", "deny", "danger"]])
add("exec_approval_prompt_choices", actions=[])
add("exec_approval_prompt_choices", actions=[["S", "session", ""]])

# ── text_debounce_cancel_timer ──
add("text_debounce_cancel_timer", state={"task": {"id": 1, "done": False}}, unless=None)
add("text_debounce_cancel_timer", state={"task": {"id": 1, "done": False}}, unless={"id": 1}, unless_id=1)
add("text_debounce_cancel_timer", state={"task": {"id": 1, "done": True}}, unless=None)
add("text_debounce_cancel_timer", state={"task": None}, unless=None)

# ── unauthorized_action_notice ──
add("unauthorized_action_notice", platform={"value": "telegram"}, seams={"t": "notice telegram"})
add("unauthorized_action_notice", platform="slack", seams={"t": "notice slack"})
add("unauthorized_action_notice", platform=None, seams={"t": "notice <platform>"})

# ── inbound media cap ──
add("get_inbound_media_max_bytes", seams={"config_section": {"max_inbound_media_bytes": 1000}})
add("get_inbound_media_max_bytes", seams={"config_section": {}})
add("get_inbound_media_max_bytes", seams={"config_section": {"max_inbound_media_bytes": "abc"}})
add("get_inbound_media_max_bytes", seams={"config_section": {"max_inbound_media_bytes": "2048"}})
add("validate_inbound_media_size", size=2000, media_type="image", max_bytes=1000)
add("validate_inbound_media_size", size=500, media_type="image", max_bytes=1000)
add("validate_inbound_media_size", size=500, media_type="media", max_bytes=0)
add("validate_inbound_media_size", size=2000, media_type="video", max_bytes=None, seams={"config_section": {"max_inbound_media_bytes": 1000}})
add("read_httpx_body_with_limit", headers={"content-length": "2000"}, chunks=["ab"], media_type="image", seams={"config_section": {"max_inbound_media_bytes": 1000}})
add("read_httpx_body_with_limit", headers={"content-length": "5"}, chunks=["ab", "cd"], media_type="image", seams={"config_section": {"max_inbound_media_bytes": 1000}})
add("read_httpx_body_with_limit", headers={"content-length": "bad"}, chunks=["ab"], media_type="image", seams={"config_section": {"max_inbound_media_bytes": 1000}})
add("read_httpx_body_with_limit", headers={}, chunks=["ab", "cd"], media_type="media", seams={"config_section": {"max_inbound_media_bytes": 100}})

# ── is_network_accessible ──
add("is_network_accessible", host="127.0.0.1")
add("is_network_accessible", host="::1")
add("is_network_accessible", host="::ffff:127.0.0.1")
add("is_network_accessible", host="8.8.8.8")
add("is_network_accessible", host="localhost", seams={"getaddrinfo": [["127.0.0.1"]]})
add("is_network_accessible", host="example.com", seams={"getaddrinfo": [["127.0.0.1", "8.8.8.8"]]})
add("is_network_accessible", host="bad.host", seams={"getaddrinfo": {"__raise__": "gaierror"}})

# ── macos proxy ──
add("detect_macos_system_proxy", darwin=False, cache=None, seams={"is_darwin": False})
add("detect_macos_system_proxy", darwin=True, cache=None, seams={"is_darwin": True, "monotonic": 100.0, "scutil": "HTTPSEnable : 1\nHTTPSProxy : h\nHTTPSPort : 8080\n"})
add("detect_macos_system_proxy", darwin=True, cache={"ts": 90.0, "value": "http://old:1"}, seams={"is_darwin": True, "monotonic": 100.0})
add("detect_macos_system_proxy", darwin=True, cache=None, seams={"is_darwin": True, "monotonic": 100.0, "scutil": {"__raise__": "RuntimeError"}})
add("reset_macos_proxy_cache", cache={"ts": 1.0, "value": "x"})
add("resolve_proxy_url", platform_env_var="TG_PROXY", target_hosts=None, configured=None,
    seams={"scoped_secret": "http://p:1", "normalize_proxy": "http://p:1", "bypass": False})
add("resolve_proxy_url", platform_env_var=None, target_hosts=None, configured="http://c:2",
    seams={"normalize_proxy": "http://c:2", "bypass": False})
add("resolve_proxy_url", platform_env_var=None, target_hosts=["h"], configured=None,
    seams={"trust_env": True, "first_proxy_env": "http://e:3", "normalize_proxy": "http://e:3", "bypass": True})
add("resolve_proxy_url", platform_env_var=None, target_hosts=None, configured=None,
    seams={"trust_env": False})
add("config_section", name="gateway", seams={"load_config": {"gateway": {"trust_env": True}}})
add("config_section", name="gateway", seams={"load_config": {"__raise__": "RuntimeError"}})
add("config_section", name="missing", seams={"load_config": {"gateway": {}}})
add("proxy_kwargs_for_aiohttp", proxy_url=None)
add("proxy_kwargs_for_aiohttp", proxy_url="http://p:1", seams={"socks_connector": None})
add("proxy_kwargs_for_aiohttp", proxy_url="socks5://p:1", seams={"socks_connector": {"url": "socks5://p:1"}})
add("proxy_kwargs_for_aiohttp", proxy_url="socks5://p:1", seams={"socks_connector": None})
add("aiohttp_socks_connector", proxy_url="socks5://p:1", seams={"import_connector": {"__raise__": "ImportError"}, "safe_proxy_url": "socks5://p:1", "log": None})
add("aiohttp_socks_connector", proxy_url="http://p:1", seams={"import_connector": {"__raise__": "ImportError"}})
add("aiohttp_socks_connector", proxy_url="socks5://p:1", seams={"import_connector": {"ok": True}})
add("should_bypass_proxy", target_hosts=["h"], seams={"bypass_matcher": True})
add("should_bypass_proxy", target_hosts=None, seams={"bypass_matcher": False})
add("should_bypass_proxy", target_hosts="h", seams={"bypass_matcher": False})
add("is_host_excluded_by_no_proxy", hostname="h", no_proxy_value="h", seams={"bypass_matcher": True})
add("is_host_excluded_by_no_proxy", hostname="h", no_proxy_value=None, seams={"bypass_matcher": False})
add("is_host_excluded_by_no_proxy", hostname="x", no_proxy_value="h", seams={"bypass_matcher": False})

# ── trivial defaults ──
add("format_message", content="hello")
add("format_message", content="")
add("format_message", content="**bold**")
add("message_len_fn", s="hello")
add("message_len_fn", s="\U0001f600a")
add("message_len_fn", s="")
add("supports_draft_streaming", chat_type="dm", metadata=None, chat_id="c")
add("supports_draft_streaming", chat_type=None, metadata=None, chat_id=None)
add("supports_draft_streaming", chat_type="group", metadata={}, chat_id="c")
add("send_draft", class_name="FooAdapter")
add("send_draft", class_name="BasePlatformAdapter")
add("send_draft", class_name="X")
add("mark_degraded", st=st(), seams={"write_status": None})
add("lazy_attr", st=st(), name="_status_text", seams={"factory": {}})
add("lazy_attr", st=st(_status_text={"c": "x"}), name="_status_text", seams={"factory": {}})
add("lazy_attr", st=st(), name="_x", seams={"factory": [1, 2]})
add("accepts_kwarg", params=[{"name": "chat_id"}, {"name": "metadata"}], name="metadata", var_kw=True, unknown=False)
add("accepts_kwarg", params=[{"name": "chat_id"}, {"name": "kw", "var_keyword": True}], name="metadata", var_kw=True, unknown=False)
add("accepts_kwarg", params=[{"name": "chat_id"}], name="metadata", var_kw=False, unknown=False)
add("accepts_kwarg", params=None, name="metadata", var_kw=True, unknown=True)
add("prepare_tts_text", text="hello", seams={"spoken": "HELLO"})
add("prepare_tts_text", text="a *b* _c_ `d`", seams={"spoken": {"__raise__": "RuntimeError"}})
add("prepare_tts_text", text="<think>x</think>hi", seams={"spoken": {"__raise__": "RuntimeError"}})
add("media_delivery_scope", st=st(gateway_runner=None), source={"chat_id": "c"})
add("media_delivery_scope", st=st(gateway_runner={"scope_resolve": True}), source={"chat_id": "c"}, seams={"scope_resolve": {"home": "/h"}})
add("media_delivery_scope", st=st(gateway_runner={"scope_resolve": True}), source={"chat_id": "c"}, seams={"scope_resolve": {"__raise__": "RuntimeError"}, "log": None})
add("pending_recovery_owners", st=st(gateway_runner=None), event=event(), session_key="k")
add("pending_recovery_owners", st=st(gateway_runner={}), event=event(), session_key="k", seams={"delivery_adapter": None})
add("pending_recovery_owners", st=st(gateway_runner={}), event=event(), session_key="k", seams={"delivery_adapter": {"id": "d2", "active_sessions": []}})
add("track_session_task", st=st(_session_tasks={}, _background_tasks=[]), session_key="k", task={"id": 1, "done": False})
add("track_session_task", st=st(_session_tasks={}, _background_tasks=[]), session_key="k", task={"id": 1, "done": False, "unhashable": True})
add("track_session_task", st=st(_session_tasks={}, _background_tasks=[1]), session_key="k", task={"id": 1, "done": False})
add("stop_typing_quietly", chat_id="c", metadata=None, seams={"stop_typing_meta": None})
add("stop_typing_quietly", chat_id="c", metadata={"thread_id": "t"}, seams={"stop_typing_meta": {"__raise__": "RuntimeError"}})
add("stop_typing_quietly", chat_id="c", metadata=None, seams={"stop_typing_meta": {"__raise__": "ValueError"}})
add("start_typing_refresh", st=st(), event=event(), interrupt_event={"id": 1, "set": False}, metadata=None,
    seams={"typing_wanted": False})
add("start_typing_refresh", st=st(), event=event(), interrupt_event={"id": 1, "set": False}, metadata={"t": 1},
    seams={"typing_wanted": True, "accepts_stop_event": True, "create_task": {"id": 9, "done": False}})
add("start_typing_refresh", st=st(), event=event(), interrupt_event={"id": 1, "set": False}, metadata=None,
    seams={"typing_wanted": True, "accepts_stop_event": False, "create_task": {"id": 9, "done": False}})
add("chain_callbacks", n=2, seams={"cb_0": None, "cb_1": None})
add("chain_callbacks", n=2, seams={"cb_0": {"__raise__": "RuntimeError"}, "cb_1": None, "log": None})
add("chain_callbacks", n=1, seams={"cb_0": {"awaitable": True}, "await_0": None})
add("send_plain_fallback", st=st(), chat_id="c", content="hello", reply_to=None, metadata=None,
    seams={"t": "prefix: hello", "warning_policy": True, "send": {"success": True, "message_id": "m"}})
add("send_plain_fallback", st=st(), chat_id="c", content="hello", reply_to=None, metadata=None,
    seams={"t": "prefix: hello", "warning_policy": False, "send": {"success": True}})
add("send_plain_fallback", st=st(), chat_id="c", content="x" * 4000, reply_to=None, metadata=None,
    seams={"t": "p", "warning_policy": True, "send": {"success": False, "error": "e"}})
add("resume_partial_send", st=st(), chat_id="c", result={"success": False}, reply_to=None, metadata=None)
add("play_tts_file", st=st(), event=event(), text_content="hi", tts_path="p.ogg", first=True, metadata=None,
    seams={"play_tts": {"success": True}, "record_delivery": None})
add("play_tts_file", st=st(platform={"value": "discord", "name": "DISCORD"}), event=event(), text_content="hi", tts_path="p", first=True, metadata=None,
    seams={"play_tts": {"success": True}, "record_delivery": None})
add("play_tts_file", st=st(), event=event(), text_content="x" * 1025, tts_path="p", first=True, metadata=None,
    seams={"play_tts": {"success": True}, "record_delivery": None})
add("record_delivery_obligation", st=st(), event=event(), session_key="k", text_content="hi", delivery_adapter={"id": "self"}, is_ephemeral_response=False, seams={"record_final_obligation": "ob1"})
add("finalize_delivery_obligation", st=st(), obligation_id="ob1", result={"success": True}, event=event(), delivery_adapter={"id": "self"}, seams={"finalize_final_obligation": None})
add("fire_post_delivery_callback", st=st(), session_key="k", generation=1, seams={"pop_post_delivery": "cb", "run_post_delivery": None})
add("release_turn_marker", st=st(gateway_runner={}), event=event(_turn_marker_handoff=True, _gateway_active_turn_token="tok", _gateway_marker_released={"id": 5, "set": False}), seams={"clear_durable_turn": None})
add("release_turn_marker", st=st(gateway_runner=None), event=event(_gateway_marker_released={"id": 5, "set": False}))
add("release_turn_marker", st=st(gateway_runner=None), event=event())
add("ledger_attempted_final", st=st(), event=event(), session_key="k", response="hi", error="e", seams={"extract_text": {"text_content": "hi"}, "ledger_attempted": True})
add("ledger_attempted_final", st=st(), event=event(), session_key="k", response="", error="e", seams={"extract_text": {"text_content": ""}})
add("ledger_attempted_final", st=st(), event=event(), session_key="k", response="hi", error="e", seams={"extract_text": {"text_content": "hi"}, "ledger_attempted": False})
add("session_key_profile", st=st(), source={"profile": "p1"}, seams={"identity_of": None})
add("session_key_profile", st=st(_owner_profile="p2"), source=None, seams={"identity_of": None})
add("session_key_profile", st=st(), source={"profile": "p1"}, seams={"identity_of": {"session_key_profile": "ip"}})
add("session_key_profile", st=st(_session_store={"has_resolver": True}), source=None, seams={"identity_of": None, "store_resolve": "rp"})
add("canonicalize", st=st(gateway_runner=None), source={"chat_id": "c"}, seams={"identity_of": None})
add("canonicalize", st=st(gateway_runner=None), source=None)
add("canonicalize", st=st(gateway_runner={"has_transport_owner": True}), source={"chat_id": "c"}, seams={"identity_of": None, "canonical_identity": {"id": "ident"}})
add("canonicalize", st=st(gateway_runner={"has_transport_owner": True}), source={"chat_id": "c"}, seams={"identity_of": None, "canonical_identity": {"__raise__": "RuntimeError"}, "log": None})
add("drop_unresolved", st=st(gateway_runner=None), event=event(source={"profile_route_rejected": True}), seams={"identity_of": None, "drop_rejected_route": None})
add("drop_unresolved", st=st(gateway_runner=None), event=event(source={"profile_route_rejected": False}), seams={"identity_of": None})
add("drop_unresolved", st=st(gateway_runner=None), event=event(), seams={"identity_of": {"id": "x"}})
add("ea_fit", st=st(), text="hello", budget=3, suffix="...", seams={"ea_escape": {"identity": True}, "measure": {"len": True}})
add("ea_fit", st=st(), text="hi", budget=10, suffix="...", seams={"ea_escape": {"identity": True}, "measure": {"len": True}})
add("ea_fit", st=st(), text="a&b", budget=5, suffix="...", seams={"ea_escape": {"identity": True}, "measure": {"len": True}})
add("ea_deadline_line", st=st(), seams={"approval_timeout": 60, "deadline_line": "deadline 60", "ea_escape": "deadline 60"})
add("exec_approval_cmd_budget", st=st(), description="d", smart_denied=False, seams={"cmd_budget": 3000})
add("supports_exec_approval_buttons", overrides_prompt=True)
add("supports_exec_approval_buttons", overrides_prompt=False)
add("event_session_key", st=st(config={"extra": {}}), event=event(), seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "agent:main:telegram:c", "dm_thread_batch_suffix": ""})
add("source_session_key", st=st(config={"extra": {"group_sessions_per_user": False}}), source={"chat_id": "c"}, seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "key1"})
add("text_batch_key", st=st(config={"extra": {}}), event=event(), seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "k", "dm_thread_batch_suffix": ":suf"})
add("pop_text_batch", st=st(_pending_text_batches={"k": {"text": "a"}}), key="k")
add("pop_text_batch", st=st(_pending_text_batches={}), key="k")
add("pop_text_batch", st=st(_pending_text_batches={"k": {"text": "a"}, "j": {"text": "b"}}), key="k")
add("dispatch_text_batch", st=st(), event=event(), seams={"handle_message": None})

# ── FLOW: handle_message_while_active ──
add("handle_message_while_active",
    st=st(_message_handler_self={"session_authority": True}, _active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}),
    event=event(text="hello"), session_key="k", has_handler_self=True, session_authority=True,
    seams={"get_command": None, "dispatch_busy_burst": None})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}, _busy_session_handler=None),
    event=event(text="/stop", message_type="text"), session_key="k",
    seams={"identity_of": None, "get_command": "stop", "should_bypass": True, "is_interrupt": True, "dispatch_active_session_command": None})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}),
    event=event(text="/model", message_type="text"), session_key="k",
    seams={"identity_of": None, "get_command": "model", "should_bypass": True, "is_interrupt": False, "dispatch_inline_reply": None, "log": None})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}),
    event=event(text="answer", allow_gateway_control=True), session_key="k",
    seams={"identity_of": None, "get_command": None, "should_bypass": False, "clarify_pending": {"id": "cp"}, "dispatch_inline_reply": None, "log": None})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}, _busy_session_handler=True),
    event=event(text="follow up"), session_key="k", has_busy_handler=True,
    seams={"identity_of": None, "get_command": None, "should_bypass": False, "clarify_pending": None, "busy_handler": True})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}),
    event=event(message_type="photo", text="cap"), session_key="k",
    seams={"identity_of": None, "get_command": None, "should_bypass": False, "clarify_pending": None, "merge_pending": None, "log": None})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={}, _busy_text_mode="queue"),
    event=event(text="more text"), session_key="k",
    seams={"identity_of": None, "get_command": None, "should_bypass": False, "clarify_pending": None, "is_command": False, "queue_debounce": None, "log": None})
add("handle_message_while_active",
    st=st(_active_sessions={"k": {"id": 1, "set": False}}, _pending_messages={"k": {"text": "old"}}),
    event=event(text="wake", internal=True), session_key="k",
    seams={"identity_of": None, "get_command": None, "should_bypass": False, "clarify_pending": None})

# ── FLOW: deliver_media_attachments / send_image_batch ──
add("send_image_batch", st=st(), event=event(), images=[["file:///a.png", ""]], metadata=None, human_delay=0.0,
    seams={"send_multiple_images": {"success": True}, "record_delivery": None, "send_footer": None})
add("send_image_batch", st=st(), event=event(), images=[["file:///a.png", ""]], metadata=None, human_delay=0.0,
    seams={"send_multiple_images": {"success": False, "error": "e"}, "record_delivery": None, "send_footer": None, "log": None})
add("send_image_batch", st=st(), event=event(), images=[], metadata=None, human_delay=0.0,
    seams={"send_multiple_images": {"__raise__": "RuntimeError", "msg": "boom"}, "record_delivery": None, "send_footer": None, "log": None})
add("deliver_media_attachments", st=st(), event=event(), media_files=[["/a.png", False]], local_files=[],
    force_document_attachments=False, human_delay=0.0, metadata=None,
    seams={"send_multiple_images": {"success": True}, "record_delivery": None, "send_footer": None})
add("deliver_media_attachments", st=st(), event=event(), media_files=[["/v.mp4", False], ["/s.ogg", True]], local_files=["/d.pdf"],
    force_document_attachments=False, human_delay=0.0, metadata=None,
    seams={"send_video": {"success": True}, "send_voice": {"success": True}, "send_document": {"success": True}, "record_delivery": None, "log": None})
add("deliver_media_attachments", st=st(), event=event(), media_files=[["/x.png", False]], local_files=[],
    force_document_attachments=True, human_delay=0.5, metadata=None,
    seams={"send_document": {"success": False, "error": "e"}, "notify_failure": None, "record_delivery": None, "sleep": None, "log": None})
add("deliver_media_attachments", st=st(), event=event(), media_files=[["/x.mp3", False]], local_files=[],
    force_document_attachments=False, human_delay=0.0, metadata=None,
    seams={"send_voice": {"__raise__": "RuntimeError", "msg": "boom"}, "record_delivery": None, "log": None})

# ── FLOW: acquire/release platform lock ──
add("acquire_platform_lock", st=st(_platform_lock_takeover_allowed=False, _platform_lock_takeover_attempted=False),
    scope="telegram", identity="id1", resource_desc="Telegram bot",
    seams={"acquire_lock": {"acquired": True, "existing": None}, "set_fatal_error": None})
add("acquire_platform_lock", st=st(_platform_lock_takeover_allowed=False, _platform_lock_takeover_attempted=False),
    scope="telegram", identity="id1", resource_desc="Telegram bot",
    seams={"acquire_lock": {"acquired": False, "existing": {"pid": 99}}, "owner_label": "p2", "set_fatal_error": None, "log": None})
add("acquire_platform_lock", st=st(_platform_lock_takeover_allowed=True, _platform_lock_takeover_attempted=False),
    scope="telegram", identity="id1", resource_desc="Telegram bot",
    seams={"acquire_lock": [{"acquired": False, "existing": {"pid": 99}}, {"acquired": True, "existing": None}], "takeover": 99, "set_fatal_error": None, "log": None})
add("acquire_platform_lock", st=st(_platform_lock_takeover_allowed=False, _platform_lock_takeover_attempted=False),
    scope="telegram", identity="id1", resource_desc="Telegram bot",
    seams={"acquire_lock": {"__raise__": "ScopedLockDirUnusable", "filename": "/locks"}, "dir_remedy": "chmod it", "set_fatal_error": None})
add("release_platform_lock", st=st(_platform_lock_scope="telegram", _platform_lock_identity="id1"), seams={"release_lock": None})
add("release_platform_lock", st=st(_platform_lock_scope="telegram", _platform_lock_identity=None), seams={"release_lock": None})
add("release_platform_lock", st=st(_platform_lock_scope="telegram", _platform_lock_identity="id1"), seams={"release_lock": None})

# ── FLOW: wire/rewire plugin handlers ──
add("wire_plugin_handlers", st=st(_plugin_handler_native=None, _plugin_handlers_wired=None), native_id="n1",
    seams={"get_factories": [{"id": "f1", "plugin": "p1", "qualname": "make", "repr": "<f1>"}], "factory": None, "log": None})
add("wire_plugin_handlers", st=st(_plugin_handler_native="n1", _plugin_handlers_wired=[["p1", "make"]]), native_id="n1",
    seams={"get_factories": [{"id": "f1", "plugin": "p1", "qualname": "make", "repr": "<f1>"}], "log": None})
add("wire_plugin_handlers", st=st(_plugin_handler_native=None, _plugin_handlers_wired=None), native_id="n1",
    seams={"get_factories": [{"id": "f1", "plugin": "p1", "qualname": "make", "repr": "<f1>"}], "factory": {"__raise__": "RuntimeError", "msg": "bad"}, "log": None})
add("wire_plugin_handlers", st=st(_plugin_handler_native=None, _plugin_handlers_wired=None), native_id="n1",
    seams={"get_factories": {"__raise__": "RuntimeError", "msg": "nope"}, "log": None})
add("rewire_plugin_handlers", st=st(_plugin_handler_native="n1", _plugin_handlers_wired=[["p1", "make"]]),
    seams={"get_factories": [{"id": "f2", "plugin": "p2", "qualname": "mk2", "repr": "<f2>"}], "factory": None, "log": None})
add("rewire_plugin_handlers", st=st(_plugin_handler_native=None, _plugin_handlers_wired=None), seams={})

# ── FLOW: text batch ──
add("enqueue_text_event", st=st(config={"extra": {}}, _pending_text_batches={}, _pending_text_batch_tasks={}),
    event=event(text="chunk1"),
    seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "k", "dm_thread_batch_suffix": "", "create_task": {"id": 1, "done": False}})
add("enqueue_text_event", st=st(config={"extra": {}}, _pending_text_batches={"k": {"text": "a", "media_urls": []}}, _pending_text_batch_tasks={"k": {"id": 1, "done": False}}),
    event=event(text="b"),
    seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "k", "dm_thread_batch_suffix": "", "absorb_turn_input": None, "create_task": {"id": 2, "done": False}})
add("enqueue_text_event", st=st(config={"extra": {}}, _pending_text_batches={}, _pending_text_batch_tasks={}),
    event=event(source={"profile_route_rejected": True}),
    seams={"identity_of": None, "drop_rejected_route": None})
add("flush_text_batch", st=st(_pending_text_batches={"k": {"text": "abc", "_last_chunk_len": 3}}, _pending_text_batch_tasks={"k": {"id": 1, "done": False}}, _text_batch_delay_seconds=0.3, _text_batch_split_delay_seconds=1.0),
    key="k", current_task_id=1, seams={"sleep": None, "handle_message": None, "log": None})
add("flush_text_batch", st=st(_pending_text_batches={"k": {"text": "abc"}}, _pending_text_batch_tasks={"k": {"id": 2, "done": False}}, _text_batch_delay_seconds=0.3),
    key="k", current_task_id=1, seams={"sleep": None})
add("flush_text_batch", st=st(_pending_text_batches={}, _pending_text_batch_tasks={"k": {"id": 1, "done": False}}, _text_batch_delay_seconds=0.3),
    key="k", current_task_id=1, seams={"sleep": None})
add("flush_text_batch", st=st(_pending_text_batches={"k": {"text": "a"}}, _pending_text_batch_tasks={"k": {"id": 1, "done": False}}, _text_batch_delay_seconds=0.3),
    key="k", current_task_id=1, seams={"sleep": {"__raise__": "CancelledError"}})
add("flush_text_batch_now", st=st(_pending_text_batches={"k": {"text": "a"}}), key="k", seams={"handle_message": None})
add("flush_text_batch_now", st=st(_pending_text_batches={}), key="k", seams={})

# ── FLOW: session task ownership ──
add("finish_session_task", st=st(_pending_messages={"k": {"text": "late"}}, _session_tasks={"k": {"id": 2, "done": False}}, _active_sessions={"k": {"id": 1, "set": False}}, _background_tasks=[]),
    session_key="k", current_task_id=1, interrupt_event={"id": 1, "set": False},
    seams={"create_task": {"id": 3, "done": False}, "log": None})
add("finish_session_task", st=st(_pending_messages={"k": {"text": "late"}}, _session_tasks={"k": {"id": 1, "done": False}}, _active_sessions={"k": {"id": 1, "set": False}}, _background_tasks=[]),
    session_key="k", current_task_id=1, interrupt_event={"id": 1, "set": False},
    seams={"create_task": {"id": 3, "done": False}, "log": None})
add("finish_session_task", st=st(_pending_messages={}, _session_tasks={"k": {"id": 1, "done": True}}, _active_sessions={"k": {"id": 1, "set": False}}),
    session_key="k", current_task_id=1, interrupt_event={"id": 1, "set": False}, seams={"cleanup_finished": None})
add("spawn_drain_task", st=st(_active_sessions={"k": {"id": 1, "set": True}}, _session_tasks={}, _background_tasks=[]),
    pending_event={"text": "p"}, session_key="k", delay=0.0, seams={"create_task": {"id": 3, "done": False}})
add("drain_after", st=st(_pending_messages={"k": {"text": "new"}}), pending_event={"text": "old"}, session_key="k", delay=0.5, guard={"id": 1, "set": False},
    seams={"sleep": None, "flush_debounce": False, "process_background": None})
add("drain_after", st=st(_pending_messages={}), pending_event={"text": "old"}, session_key="k", delay=0.5, guard={"id": 1, "set": False},
    seams={"sleep": None, "flush_debounce": False, "cleanup_finished": None})
add("drain_after", st=st(_pending_messages={}), pending_event={"text": "p"}, session_key="k", delay=0.0, guard=None,
    seams={"process_background": None})

# ── FLOW: final delivery ──
add("send_final_ledgered", st=st(gateway_runner=None), event=event(), session_key="k", text_content="final", metadata=None, reply_to=None, is_ephemeral_response=False,
    seams={"ledger_bracket": {"obligation_id": "ob1", "delivery_adapter": {"id": "self", "name": "Telegram"}}, "send_with_retry": {"success": True, "message_id": "m"}, "stop_clock": None, "ledger_finalize": None, "log": None})
add("send_final_ledgered", st=st(gateway_runner=None), event=event(_turn_marker_handoff=True, _gateway_active_turn_token="t", _gateway_marker_released={"id": 5, "set": False}), session_key="k", text_content="final", metadata=None, reply_to=None, is_ephemeral_response=False,
    seams={"ledger_bracket": {"obligation_id": "ob1", "delivery_adapter": {"id": "self", "name": "Telegram"}}, "clear_durable_turn": None, "send_with_retry": {"success": True}, "stop_clock": None, "ledger_finalize": None, "log": None})
add("send_final_ledgered", st=st(gateway_runner=None), event=event(), session_key="k", text_content="final", metadata=None, reply_to=None, is_ephemeral_response=False,
    seams={"ledger_bracket": {"obligation_id": None, "delivery_adapter": {"id": "self", "name": "Telegram"}}, "send_with_retry": {"success": False, "error": "e"}, "stop_clock": None, "ledger_finalize": None, "log": None})
add("send_final_text", st=st(gateway_runner=None), event=event(), session_key="k", text_content="final", metadata=None, is_ephemeral_response=False, ephemeral_ttl=60,
    seams={"reply_anchor": None, "ledger_bracket": {"obligation_id": "ob1", "delivery_adapter": {"id": "self", "name": "Telegram"}}, "send_with_retry": {"success": True, "message_id": "m"}, "stop_clock": None, "ledger_finalize": None, "record_delivery": None, "schedule_delete": None, "log": None})
add("send_final_text", st=st(gateway_runner=None), event=event(), session_key="k", text_content="final", metadata=None, is_ephemeral_response=False, ephemeral_ttl=0,
    seams={"reply_anchor": None, "ledger_bracket": {"obligation_id": None, "delivery_adapter": {"id": "self", "name": "Telegram"}}, "send_with_retry": {"success": True}, "stop_clock": None, "ledger_finalize": None, "record_delivery": None, "log": None})
add("notify_turn_error", st=st(gateway_runner=None), event=event(),
    seams={"reply_anchor": None, "wake_muted": False, "t": ["generic failed", "turn error"], "warning_policy": True, "send": {"success": True}})
add("notify_turn_error", st=st(gateway_runner=None), event=event(),
    seams={"reply_anchor": None, "wake_muted": True})
add("notify_turn_error", st=st(gateway_runner=None), event=event(),
    seams={"reply_anchor": None, "wake_muted": False, "t": ["g", "h"], "warning_policy": False, "send": {"success": True}})
add("deliver_attachments", st=st(_human_delay_range_ms=None), event=event(),
    extracted={"text_content": "t", "images": [["https://x/a.png", ""]], "media_files": [], "local_files": [], "force_document_attachments": False, "pre_extract": "t"},
    metadata=None, anything_sent=False,
    seams={"final_adapter": {"id": "self"}, "send_multiple_images": {"success": True}, "record_delivery": None, "send_footer": None, "log": None})
add("deliver_attachments", st=st(_human_delay_range_ms=None), event=event(),
    extracted={"text_content": "", "images": [], "media_files": [], "local_files": [], "force_document_attachments": False, "pre_extract": "lost"},
    metadata=None, anything_sent=False, seams={"final_adapter": {"id": "self"}, "log": None})
add("deliver_attachments", st=st(_human_delay_range_ms=None), event=event(),
    extracted={"text_content": "t", "images": [], "media_files": [["/v.mp4", False]], "local_files": [], "force_document_attachments": False, "pre_extract": "t"},
    metadata=None, anything_sent=True, seams={"final_adapter": {"id": "self"}, "send_video": {"success": True}, "record_delivery": None, "log": None})

# ── FLOW: warnings / media fallback ──
add("emit_warning", st=st(), chat_id="c", content="warn", reply_to=None, metadata=None, logical_platform=None,
    seams={"warning_policy": True, "send": {"success": True, "message_id": "m"}})
add("emit_warning", st=st(), chat_id="c", content="warn", reply_to=None, metadata=None, logical_platform=None,
    seams={"warning_policy": False})
add("emit_warning", st=st(), chat_id="c", content="warn", reply_to="r", metadata={"t": 1}, logical_platform=None,
    seams={"warning_policy": True, "send": {"success": False, "error": "e"}})
add("emit_media_warning", st=st(), chat_id="c", notice="notice", caption="cap", reply_to=None, metadata=None, shown_metadata=None,
    seams={"warning_policy": True, "send": {"success": True}})
add("emit_media_warning", st=st(), chat_id="c", notice="notice", caption="cap", reply_to=None, metadata={"m": 1}, shown_metadata=None,
    seams={"warning_policy": False, "send": {"success": True}})
add("emit_media_warning", st=st(), chat_id="c", notice="notice", caption=None, reply_to=None, metadata=None, shown_metadata=None,
    seams={"warning_policy": False})
add("send_media_fallback_notice", st=st(), method="send_voice", kind="audio", path="/a.ogg", chat_id="c", caption="cap", reply_to=None, metadata=None, file_name=None,
    seams={"log": None, "t": ["Audio", "failed audio"], "warning_policy": True, "send": {"success": True}})
add("send_media_fallback_notice", st=st(), method="send_document", kind="file", path="/d.pdf", chat_id="c", caption=None, reply_to=None, metadata=None, file_name="d.pdf",
    seams={"log": None, "t": ["File", "failed d.pdf"], "warning_policy": False})

# ── FLOW: exec approval / clarify ──
add("send_exec_approval", st=st(), chat_id="c", command="rm -rf /", session_key="k", description=None, metadata=None,
    allow_permanent=True, allow_session=True, smart_denied=False, request_id="rq",
    seams={"ea_default_reason": "reason", "ea_labels": {"once": "Once", "session": "Session", "always": "Always", "deny": "Deny"},
           "render": {"text": "card", "truncated": False}, "card_whole": True, "send_prompt": {"success": True}})
add("send_exec_approval", st=st(), chat_id="c", command="long", session_key="k", description="d", metadata=None,
    allow_permanent=True, allow_session=True, smart_denied=False, request_id=None,
    seams={"ea_labels": {"once": "Once", "session": "Session", "always": "Always", "deny": "Deny"},
           "render": [{"text": "card", "truncated": True}, {"text": "deny card", "truncated": False}], "card_whole": False, "send_prompt": {"success": True}})
add("send_exec_approval", st=st(), chat_id="c", command="x", session_key="k", description=None, metadata=None,
    allow_permanent=True, allow_session=True, smart_denied=True, request_id=None,
    seams={"ea_default_reason": "r", "ea_labels": {"once": "Once", "deny": "Deny"}, "render": {"text": "c", "truncated": False}, "card_whole": True, "send_prompt": {"success": False, "error": "e"}})
add("send_clarify", st=st(), chat_id="c", question="Q?", choices=["a", "b"], clarify_id="cid", session_key="k", metadata=None,
    seams={"clarify_get_pending": {"multi_select": False}, "t": ["hint single", "1. a", "2. b", "Q?"], "mark_awaiting": None, "send": {"success": True}})
add("send_clarify", st=st(), chat_id="c", question="Q?", choices=None, clarify_id="cid", session_key="k", metadata=None,
    seams={"t": ["Q?"], "send": {"success": True}})
add("send_clarify", st=st(), chat_id="c", question="Q?", choices=["a"], clarify_id="cid", session_key="k", metadata=None,
    seams={"clarify_get_pending": {"__raise__": "RuntimeError"}, "t": ["hint single", "1. a", "Q?"], "mark_awaiting": None, "send": {"success": True}})

# ── FLOW: build_source ──
add("build_source", st=st(_owner_profile=None, gateway_runner=None),
    fields={"chat_id": "c1", "chat_type": "dm", "user_id": "u1", "thread_id": "t1", "chat_topic": " topic "},
    seams={"configured_bot": False})
add("build_source", st=st(_owner_profile="p2", gateway_runner=None),
    fields={"chat_id": "c1", "chat_type": "group", "user_id": "", "is_bot": False},
    seams={"configured_bot": True})
add("build_source", st=st(_owner_profile=None, gateway_runner={}),
    fields={"chat_id": "c1", "chat_type": "dm", "user_id": "u1"},
    seams={"configured_bot": False, "profile_for_source": "p3"})
add("build_source", st=st(_owner_profile=None, gateway_runner={}),
    fields={"chat_id": "c1", "chat_type": "dm", "user_id": "u1"},
    seams={"configured_bot": False, "profile_for_source": {"__raise__": "ProfileRouteRejected", "profile": "px"}})
add("build_source", st=st(_owner_profile=None, gateway_runner={}),
    fields={"chat_id": "c1", "chat_type": "dm", "user_id": "u1"},
    seams={"configured_bot": False, "profile_for_source": {"__raise__": "RuntimeError"}, "log": None})

# ── FLOW: dispatch_inline_reply ──
add("dispatch_inline_reply", st=st(), event=event(), log_cmd=None,
    seams={"reply_anchor": [None, None], "message_handler": "reply", "unwrap_ephemeral": {"text": "reply", "ttl": 0}, "send_with_retry": {"success": True, "message_id": "m"}})
add("dispatch_inline_reply", st=st(), event=event(), log_cmd="stop",
    seams={"reply_anchor": [None, "r"], "message_handler": "reply", "unwrap_ephemeral": {"text": "reply", "ttl": 60}, "send_with_retry": {"success": True, "message_id": "m"}, "schedule_delete": None, "log": None})
add("dispatch_inline_reply", st=st(), event=event(), log_cmd=None,
    seams={"reply_anchor": None, "message_handler": None, "unwrap_ephemeral": {"text": None, "ttl": 0}})

# ── default send delegations ──
add("send_image", st=st(), chat_id="c", image_url="https://x/a.png", caption="cap", reply_to=None, metadata=None,
    seams={"send": {"success": True}})
add("send_image", st=st(), chat_id="c", image_url="https://x/a.png", caption=None, reply_to="r", metadata=None,
    seams={"send": {"success": True}})
add("send_image", st=st(), chat_id="c", image_url="u", caption="", reply_to=None, metadata=None, seams={"send": {"success": False}})
add("send_animation", st=st(), chat_id="c", animation_url="https://x/a.gif", caption="cap", reply_to=None, metadata=None,
    seams={"send_image": {"success": True}})
add("send_animation", st=st(), chat_id="c", animation_url="u", caption=None, reply_to=None, metadata=None, seams={"send_image": {"success": True}})
add("send_animation", st=st(), chat_id="c", animation_url="u", caption="c", reply_to="r", metadata={"m": 1}, seams={"send_image": {"success": False}})
add("send_voice", st=st(), chat_id="c", audio_path="/a.ogg", caption="cap", reply_to=None, metadata=None, seams={"fallback_notice": {"success": False, "error": "e"}})
add("send_voice", st=st(), chat_id="c", audio_path="/a.ogg", caption=None, reply_to=None, metadata=None, seams={"fallback_notice": {"success": True}})
add("send_voice", st=st(), chat_id="c", audio_path="/a.ogg", caption="c", reply_to="r", metadata=None, seams={"fallback_notice": {"success": False}})
add("send_video", st=st(), chat_id="c", video_path="/v.mp4", caption=None, reply_to=None, metadata=None, seams={"fallback_notice": {"success": False}})
add("send_video", st=st(), chat_id="c", video_path="/v.mp4", caption="c", reply_to=None, metadata=None, seams={"fallback_notice": {"success": True}})
add("send_video", st=st(), chat_id="c", video_path="/v.mp4", caption=None, reply_to="r", metadata=None, seams={"fallback_notice": {"success": False}})
add("send_document", st=st(), chat_id="c", file_path="/d.pdf", caption=None, file_name="d.pdf", reply_to=None, metadata=None, seams={"fallback_notice": {"success": False}})
add("send_document", st=st(), chat_id="c", file_path="/d.pdf", caption="c", file_name=None, reply_to=None, metadata=None, seams={"fallback_notice": {"success": True}})
add("send_document", st=st(), chat_id="c", file_path="/d.pdf", caption=None, file_name="n", reply_to="r", metadata=None, seams={"fallback_notice": {"success": False}})
add("send_image_file", st=st(), chat_id="c", image_path="/i.png", caption=None, reply_to=None, metadata=None, seams={"fallback_notice": {"success": False}})
add("send_image_file", st=st(), chat_id="c", image_path="/i.png", caption="c", reply_to=None, metadata=None, seams={"fallback_notice": {"success": True}})
add("send_image_file", st=st(), chat_id="c", image_path="/i.png", caption=None, reply_to="r", metadata=None, seams={"fallback_notice": {"success": False}})
add("play_tts", st=st(), chat_id="c", audio_path="/a.ogg", kwargs={"caption": "cap", "metadata": None}, seams={"send_voice": {"success": True}})
add("play_tts", st=st(), chat_id="c", audio_path="/a.ogg", kwargs={}, seams={"send_voice": {"success": False}})
add("play_tts", st=st(), chat_id="c", audio_path="/a.ogg", kwargs={"caption": None}, seams={"send_voice": {"success": True}})
add("send_slash_confirm", st=st(), chat_id="c", title="t", message="m", session_key="k", confirm_id="cid", metadata=None)
add("send_slash_confirm", st=st(), chat_id="c", title="t", message="m", session_key="k", confirm_id="cid", metadata={"x": 1})
add("send_slash_confirm", st=st(), chat_id="c2", title="t2", message="m2", session_key="k2", confirm_id="cid2", metadata=None)
add("send_exec_approval_prompt_default", st=st(), prompt={"chat_id": "c"})
add("send_exec_approval_prompt_default", st=st(), prompt=None)
add("send_exec_approval_prompt_default", st=st(), prompt={"text": "x"})


# ── supplementary cases to reach >=3 per function ──
add("reset_macos_proxy_cache", cache=None)
add("reset_macos_proxy_cache", cache={"ts": 5.0, "value": None})
add("mark_degraded", st=st(platform={"value": "discord", "name": "DISCORD"}), seams={"write_status": None})
add("mark_degraded", st=st(_runtime_status_platform_key="p2:telegram"), seams={"write_status": None})
add("resume_partial_send", st=st(), chat_id="c2", result={"success": False, "raw_response": {"partial_overflow": True}}, reply_to="r", metadata={"m": 1})
add("resume_partial_send", st=st(), chat_id="c", result={"success": True}, reply_to=None, metadata=None)
add("record_delivery_obligation", st=st(), event=event(), session_key="k", text_content="", delivery_adapter={"id": "d2"}, is_ephemeral_response=True, seams={"record_final_obligation": None})
add("record_delivery_obligation", st=st(), event=event(), session_key="k2", text_content="x", delivery_adapter=None, is_ephemeral_response=False, seams={"record_final_obligation": "ob2"})
add("finalize_delivery_obligation", st=st(), obligation_id=None, result={"success": False, "error": "e"}, event=event(), delivery_adapter={"id": "d2"}, seams={"finalize_final_obligation": None})
add("finalize_delivery_obligation", st=st(), obligation_id="ob2", result={"success": True, "message_id": "m"}, event=event(), delivery_adapter=None, seams={"finalize_final_obligation": None})
add("fire_post_delivery_callback", st=st(), session_key="k", generation=None, seams={"pop_post_delivery": None, "run_post_delivery": None})
add("fire_post_delivery_callback", st=st(), session_key="", generation=2, seams={"pop_post_delivery": "cb2", "run_post_delivery": None})
add("ea_deadline_line", st=st(_EA_DEADLINE_PREFIX="\n"), seams={"approval_timeout": 30, "deadline_line": "d30", "ea_escape": "d30"})
add("ea_deadline_line", st=st(), seams={"approval_timeout": 0, "deadline_line": "", "ea_escape": ""})
add("exec_approval_cmd_budget", st=st(), description="", smart_denied=True, seams={"cmd_budget": 1500})
add("exec_approval_cmd_budget", st=st(), description="long desc", smart_denied=False, seams={"cmd_budget": 0})
add("supports_exec_approval_buttons", overrides_prompt=True)
add("event_session_key", st=st(config={"extra": {"group_sessions_per_user": False}}), event=event(source={"chat_id": "c2"}), seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c2"}, "build_session_key": "k2", "dm_thread_batch_suffix": ""})
add("event_session_key", st=st(config={"extra": {}}), event=event(), seams={"identity_of": {"session_key_profile": "ip"}, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "k3", "dm_thread_batch_suffix": ""})
add("source_session_key", st=st(config={"extra": {}}, _owner_profile="p2"), source={"chat_id": "c"}, seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "key2"})
add("source_session_key", st=st(config={"extra": {"thread_sessions_per_user": True}}), source={"chat_id": "c", "thread_id": "t"}, seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c", "thread_id": "t"}, "build_session_key": "key3"})
add("text_batch_key", st=st(config={"extra": {}}), event=event(), seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c"}, "build_session_key": "base", "dm_thread_batch_suffix": ":t"})
add("text_batch_key", st=st(config={"extra": {}}), event=event(source={"chat_id": "c2"}), seams={"identity_of": None, "dm_thread_key_source": {"chat_id": "c2"}, "build_session_key": "b2", "dm_thread_batch_suffix": ""})
add("dispatch_text_batch", st=st(), event=event(text="a"), seams={"handle_message": None})
add("dispatch_text_batch", st=st(), event=event(text="b"), seams={"handle_message": None})
add("rewire_plugin_handlers", st=st(_plugin_handler_native="n1", _plugin_handlers_wired=[]),
    seams={"get_factories": [{"id": "f1", "plugin": "p1", "qualname": "mk", "repr": "<f1>"}], "factory": None, "log": None})
add("flush_text_batch_now", st=st(_pending_text_batches={"k": {"text": "xyz"}}), key="k", seams={"handle_message": None})
add("spawn_drain_task", st=st(_active_sessions={"k": {"id": 1, "set": False}}, _session_tasks={}, _background_tasks=[]),
    pending_event={"text": "p"}, session_key="k", delay=0.5, seams={"create_task": {"id": 4, "done": False}})
add("spawn_drain_task", st=st(_active_sessions={}, _session_tasks={}, _background_tasks=[]),
    pending_event={"text": "p"}, session_key="k", delay=0.0, seams={"create_task": {"id": 5, "done": False}})
add("send_final_text", st=st(gateway_runner=None), event=event(), session_key="k", text_content="final", metadata=None, is_ephemeral_response=True, ephemeral_ttl=60,
    seams={"reply_anchor": "r", "ledger_bracket": {"obligation_id": "ob1", "delivery_adapter": {"id": "self", "name": "Telegram"}}, "send_with_retry": {"success": False, "error": "e"}, "stop_clock": None, "ledger_finalize": None, "record_delivery": None, "log": None})
add("send_media_fallback_notice", st=st(), method="send_video", kind="video", path="/v.mp4", chat_id="c", caption="cap", reply_to="r", metadata={"m": 1}, file_name=None,
    seams={"log": None, "t": ["Video", "failed video"], "warning_policy": True, "send": {"success": False, "error": "e"}})


# ── emit C header ──
def c_str(s):
    """Byte-exact C string literal: hex-escape >=0x80 / control, split before hex digits."""
    raw = s.encode("utf-8")
    out = []
    hexdigits = set(b"0123456789abcdefABCDEF")
    i = 0
    while i < len(raw):
        b = raw[i]
        if b == 0x22:
            out.append('\\"')
        elif b == 0x5C:
            out.append('\\\\')
        elif b == 0x0A:
            out.append('\\n')
        elif b == 0x0D:
            out.append('\\r')
        elif b == 0x09:
            out.append('\\t')
        elif 0x20 <= b < 0x7F:
            out.append(chr(b))
        else:
            out.append('\\x%02x' % b)
            # split if next byte is an ASCII hex digit (would bleed into escape)
            if i + 1 < len(raw) and raw[i + 1] in hexdigits:
                out.append('" "')
        i += 1
    return '"' + ''.join(out) + '"'


lines = ["/* Generated by tests/gen_base_flow_cases.py — embedded oracle cases. */",
         "#ifndef BASE_FLOW_CASES_H", "#define BASE_FLOW_CASES_H",
         "static const char *const BF_CASES[] = {"]
for c in CASES:
    js = json.dumps(c, ensure_ascii=False, separators=(",", ":"))
    lines.append("    " + c_str(js) + ",")
lines.append("};")
lines.append("#endif /* BASE_FLOW_CASES_H */")
target = Path(r"C:/Users/eman5/slermes/tests/base_flow_cases.h")
target.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="")
print("cases:", len(CASES))
from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("functions covered:", len(cnt))
low = {k: v for k, v in cnt.items() if v < 3}
print("funcs with <3 cases:", low)