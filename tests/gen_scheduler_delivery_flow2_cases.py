#!/usr/bin/env python3
"""Generate tests/fixtures/scheduler_delivery_flow2.jsonl (>=5 cases/function).

Every case is {"func": <name>, "in": {..., "io": {seam: result|queue}}}.
`result` = {"ret": v, "raise": {"type","message"}, "state": {...}}.
Trace is NAME-ONLY; scripts follow in.io. Deterministic subset documented in
src/cron/scheduler_delivery_flow2.c (integral floats excluded, platform ids as
POSIX strings, seam queue semantics one-result-per-element).
"""
import json, os
OUT = os.path.join(os.path.dirname(__file__), "fixtures", "scheduler_delivery_flow2.jsonl")
C = []
def case(func, **inp):
    inp.setdefault("io", {})
    C.append({"func": func, "in": inp})
def R(ret=None, **kw):
    d = {"ret": ret}; d.update(kw); return d
def E(t, msg, **kw):
    return {"raise": {"type": t, "message": msg, **kw}}
def T(**kw):
    """_TargetDelivery fixture object (all dataclass fields present)."""
    d = dict(job={"id": "j1", "name": "cron"}, platform="telegram", platform_name="telegram",
             chat_id="c1", thread_id=None, transport=None,
             pconfig={"enabled": True, "extra": {}}, runtime_adapter=None,
             target_adapters={}, config={"platforms": {}}, loop=None, notify_delivery=False,
             origin={}, origin_target=False, origin_user_id=None, is_dm_target=False,
             mirror_text="[Cron delivery: cron]\nx", mirror_this_target=False,
             in_channel_surface=False, inchannel_continuable=False, opened_thread_id=None,
             live_adapter_ready=False, force_document=False, live_error=None,
             live_retryable=None, standalone_in_flight=False)
    d.update(kw); return d


# ---- 1 _resolve_cron_surface_mode (pure) ----
case("_resolve_cron_surface_mode", pconfig={"extra":{"cron_continuable_surface":"in_channel"}}, logical_name="telegram")
case("_resolve_cron_surface_mode", pconfig={"extra":{"telegram":{"cron_continuable_surface":"IN_CHANNEL "}}}, logical_name="Telegram")
case("_resolve_cron_surface_mode", pconfig={"extra":{"telegram":{},"cron_continuable_surface":"in_channel"}}, logical_name="telegram")
case("_resolve_cron_surface_mode", pconfig={"extra":{}}, logical_name="telegram")
case("_resolve_cron_surface_mode", pconfig={}, logical_name=None)
case("_resolve_cron_surface_mode", pconfig={"extra":"bad"}, logical_name="telegram")

# ---- 2 _fresh_thread_overrides_origin_topic ----
case("_fresh_thread_overrides_origin_topic", pconfig={"extra":{"cron_fresh_thread_per_run":True}}, io={"logger.warning":R()})
case("_fresh_thread_overrides_origin_topic", pconfig={"extra":{"cron_fresh_thread_per_run":False}}, io={"logger.warning":R()})
case("_fresh_thread_overrides_origin_topic", pconfig={"extra":{}}, io={"logger.warning":R()})
case("_fresh_thread_overrides_origin_topic", pconfig={"extra":{"cron_fresh_thread_per_run":"yes"}}, io={"logger.warning":R()})
case("_fresh_thread_overrides_origin_topic", pconfig={"extra":"bad"}, io={"logger.warning":R()})
case("_fresh_thread_overrides_origin_topic", pconfig={"extra":{"cron_fresh_thread_per_run":1}}, io={"logger.warning":R()})

# ---- 3 _origin_thread_is (seam resolve_origin) ----
case("_origin_thread_is", job={}, thread_id="t1", io={"resolve_origin":R({"thread_id":"t1"})})
case("_origin_thread_is", job={}, thread_id="t2", io={"resolve_origin":R({"thread_id":"t1"})})
case("_origin_thread_is", job={}, thread_id="t1", io={"resolve_origin":R(None)})
case("_origin_thread_is", job={}, thread_id="3", io={"resolve_origin":R({"thread_id":3})})
case("_origin_thread_is", job={}, thread_id="t1", io={"resolve_origin":R({})})
case("_origin_thread_is", job={}, thread_id="t1", io={"resolve_origin":[E("RuntimeError","boom")]})

# ---- 4 _resolve_origin (pure) ----
case("_resolve_origin", job={"origin":{"platform":"telegram","chat_id":"c1"}})
case("_resolve_origin", job={"origin":"combined-digest"})
case("_resolve_origin", job={"origin":{"platform":"telegram"}})
case("_resolve_origin", job={"origin":{"chat_id":"c1"}})
case("_resolve_origin", job={"origin":{"platform":"api_server","chat_id":"c1"}})
case("_resolve_origin", job={"origin":{"platform":"API_SERVER","chat_id":"c1"}})

# ---- 5 _redact_cron_payload ----
case("_redact_cron_payload", text="", what="job name", io={"redact_sensitive_text":R("x"),"logger.warning":R()})
case("_redact_cron_payload", text="secret", what="job name", io={"redact_sensitive_text":R("[scrubbed]"),"logger.warning":R()})
case("_redact_cron_payload", text="secret", what="x", io={"redact_sensitive_text":[E("ValueError","bad"),R("ok")],"logger.warning":R()})
case("_redact_cron_payload", text="secret", what="x", io={"redact_sensitive_text":[E("SystemExit","halt")],"logger.warning":R()})
case("_redact_cron_payload", text=None, what="x", io={"redact_sensitive_text":R("x"),"logger.warning":R()})

# ---- 6 _cron_display_name ----
case("_cron_display_name", job={"name":"n"}, io={"redact_cron_payload":[R("N")]})
case("_cron_display_name", job={"id":"i"}, io={"redact_cron_payload":[R("I")]})
case("_cron_display_name", job={}, io={"redact_cron_payload":[R("cron")]})
case("_cron_display_name", job={"name":None,"id":"i"}, io={"redact_cron_payload":[R("NN")]})
case("_cron_display_name", job={"name":"n"}, io={"redact_cron_payload":[E("RuntimeError","x"),R("y")]})

# ---- 7 _cron_mirror_message ----
case("_cron_mirror_message", job={"name":"n"}, text="hi", io={"cron_display_name":[R("J")]})
case("_cron_mirror_message", job={}, text="line1\nline2", io={"cron_display_name":[R("A")]})
case("_cron_mirror_message", job={}, text="", io={"cron_display_name":[R("B")]})
case("_cron_mirror_message", job={}, text="x", io={"cron_display_name":[E("RuntimeError","e")]})

case("_cron_mirror_message", job={"id":"i"}, text="tail", io={"cron_display_name":[R("ID")]})
case("_home_target", platform_name="slack", chat_id="D2", resolved_from="home", io={"get_home_target_thread_id":[R("t3")]})
case("_home_target", platform_name="discord", chat_id="c2", resolved_from=None, io={"get_home_target_thread_id":[E("RuntimeError","x")]})
case("_seed_cron_session", job={"name":"n"}, adapter={}, platform_name="telegram", chat_id="c1", text="hi",
     thread_id=None, chat_type="channel", user_id=None, user_name=None, chat_name=None, scope_id=None,
     discord_keys_on_thread=False, io={"cron_mirror_message":R("msg"),"mirror_to_session":[E("RuntimeError","x")]})

# ---- 8 _seed_cron_session ----
case("_seed_cron_session", job={"name":"j"}, adapter={}, platform_name="telegram", chat_id="c1", text="hi",
     thread_id=None, chat_type="channel", user_id=None, user_name=None, chat_name=None, scope_id=None,
     discord_keys_on_thread=False, io={"cron_mirror_message":R("msg"),"mirror_to_session":R(True)})
case("_seed_cron_session", job={}, adapter={"_session_store":True,"session_id":"s1"}, platform_name="discord",
     chat_id="c1", text="hi", thread_id="t1", chat_type="channel", user_id=None, user_name=None,
     chat_name=None, scope_id="123", discord_keys_on_thread=True,
     io={"Platform":R("discord"),"SessionSource":R(),"get_or_create_session":R({"session_id":"s1"}),
         "cron_mirror_message":R("msg"),"mirror_to_session":R(True)})
case("_seed_cron_session", job={}, adapter={"_session_store":True}, platform_name="telegram", chat_id="c1",
     text="hi", thread_id=None, chat_type="dm", user_id="u1", user_name="U", chat_name="C", scope_id=None,
     discord_keys_on_thread=False,
     io={"Platform":[E("ValueError","bad")],"cron_mirror_message":R("msg"),"mirror_to_session":R(False)})
case("_seed_cron_session", job={}, adapter={"_session_store":True}, platform_name="telegram", chat_id="c1",
     text="hi", thread_id=None, chat_type="channel", user_id=None, user_name=None, chat_name=None,
     scope_id=None, discord_keys_on_thread=False,
     io={"Platform":R("telegram"),"SessionSource":R(),"get_or_create_session":[E("RuntimeError","x")],
         "cron_mirror_message":R("msg"),"mirror_to_session":R(True)})

# ---- 9 _home_env_lookup ----
case("_home_env_lookup", env_var="TELEGRAM_HOME_CHANNEL", suffix="", strip=False, io={"get_secret":[R("ch1")]})
case("_home_env_lookup", env_var="QQBOT_HOME_CHANNEL", suffix="_ID", strip=False, io={"get_secret":[R(""),R("legacy")]})
case("_home_env_lookup", env_var="TELEGRAM_HOME_CHANNEL", suffix="", strip=True, io={"get_secret":[R("  ch  ")]})
case("_home_env_lookup", env_var="TELEGRAM_HOME_CHANNEL", suffix="", strip=False, io={"get_secret":[R("")]})
case("_home_env_lookup", env_var="QQBOT_HOME_CHANNEL", suffix="", strip=False, io={"get_secret":[R(None),R("QQ")]})

# ---- 10 _relay_fronted_delivery_platforms ----
case("_relay_fronted_delivery_platforms", connected=["telegram"], io={"relay_fronted_platforms":R(["telegram"]),"logger.warning":R()})
case("_relay_fronted_delivery_platforms", connected=[], io={"relay_fronted_platforms":R(["telegram"]),"logger.warning":R()})
case("_relay_fronted_delivery_platforms", connected=["relay"], io={"relay_fronted_platforms":R(["slack","telegram"]),"logger.warning":R()})
case("_relay_fronted_delivery_platforms", connected=["relay"], io={"relay_fronted_platforms":[E("RuntimeError","x")],"logger.warning":R(),"logger.debug":R()})
case("_relay_fronted_delivery_platforms", connected=["relay"], io={"relay_fronted_platforms":[E("SystemExit","x")],"logger.warning":R(),"logger.debug":R()})

# ---- 11 _home_target ----
case("_home_target", platform_name="telegram", chat_id="c1", resolved_from=None, io={"get_home_target_thread_id":[R("t1")]})
case("_home_target", platform_name="telegram", chat_id="c1", resolved_from="home", io={"get_home_target_thread_id":[R(None)]})
case("_home_target", platform_name="slack", chat_id="D1", resolved_from="origin", io={"get_home_target_thread_id":[R("t2")]})

# ---- 12 _get_standalone_send_timeout ----
case("_get_standalone_send_timeout", io={"load_config":[R({"cron":{"standalone_send_timeout_seconds":30}})]})
case("_get_standalone_send_timeout", io={"load_config":[R({"cron":{"standalone_send_timeout_seconds":0}})]})
case("_get_standalone_send_timeout", io={"load_config":[R({"cron":{"standalone_send_timeout_seconds":-5}})]})
case("_get_standalone_send_timeout", io={"load_config":[E("RuntimeError","x")]})
case("_get_standalone_send_timeout", io={"load_config":[R({"cron":{"standalone_send_timeout_seconds":"45"}})]})
case("_get_standalone_send_timeout", io={"load_config":[R({"cron":{"standalone_send_timeout_seconds":"abc"}})]})



# ---- 13 bot_chat_message ----
case("bot_chat_message", job={"name":"n"}, content="body", io={"redact_cron_payload":[R("N"),R("B")]})
case("bot_chat_message", job={"id":"i"}, content="body", io={"redact_cron_payload":[R("I"),R("B2")]})
case("bot_chat_message", job={"name":"n"}, content="body", io={"redact_cron_payload":[E("RuntimeError","x")]})
case("bot_chat_message", job={"name":"n"}, content="body", io={"redact_cron_payload":[R("N"),E("RuntimeError","y")]})
case("bot_chat_message", job={"name":None,"id":"i"}, content="body", io={"redact_cron_payload":[R("N2"),R("B3")]})

# ---- 14 _deliver_to_bot_chat ----
def botio(**kw):
    d = {"bot_chat_message":R("MSG"), "get_hermes_home":R("/h1"),
         "sha256":R("key1"), "read_delivery_result":R({"message":"MSG","status":"settled"}),
         "logger.info":R()}
    d.update(kw); return d
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=False, io=botio())
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=False, io=botio(read_delivery_result=R({"message":"MSG","status":"queued","error":"slow"})))
case("_deliver_to_bot_chat", job={"id":"j1"}, content="c", profile="p1", for_failure=False,
     io=botio(get_profile_dir=R("/p1"), uuid4=R("uuid1")))
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=True, io=botio(read_delivery_result=R(None),
         load_user_config_effective=R({}), warning_notifications_enabled=R(False)))
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=False, io=botio(read_delivery_result=R(None), find_canonical_live_owner=R(None)))
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=False, io=botio(read_delivery_result=R(None),
         find_canonical_live_owner=[E("ValueError","gateway down")], defer=R()))
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=False, io=botio(read_delivery_result=R(None),
         find_canonical_live_owner=R("owner"),
         deliver_to_live_owner=R({"message":"OTHER","status":"settled"})))
case("_deliver_to_bot_chat", job={"id":"j1","execution_id":"e1"}, content="c", profile="",
     for_failure=False, io=botio(get_hermes_home=[E("SystemExit","halt")]))

# ---- 15 _resolve_bot_chat_target ----
case("_resolve_bot_chat_target", job={"id":"j1"}, profile_arg="", io={"logger.warning":R()})
case("_resolve_bot_chat_target", job={"id":"j1"}, profile_arg="p1",
     io={"normalize_profile_name":[R("p1")],"profile_exists":[R(True)],"logger.warning":R()})
case("_resolve_bot_chat_target", job={"id":"j1"}, profile_arg="bad",
     io={"normalize_profile_name":[R("bad")],"profile_exists":[R(False)],"logger.warning":R()})
case("_resolve_bot_chat_target", job={"id":"j1"}, profile_arg="p2",
     io={"normalize_profile_name":[E("RuntimeError","x")],"logger.warning":R()})
case("_resolve_bot_chat_target", job={"id":"j1"}, profile_arg="p3",
     io={"normalize_profile_name":[R("p3")],"profile_exists":[E("SystemExit","x")],"logger.warning":R()})

# ---- 16 _result_field ----
case("_result_field", send_result={"error":"e1"}, key="error", default="D")
case("_result_field", send_result={"error":"e1"}, key="missing", default="D")
case("_result_field", send_result={"__attr__":{"error":"e2"}}, key="error", default="D")
case("_result_field", send_result={"__attr__":{"x":1}}, key="missing", default="D")
case("_result_field", send_result=None, key="error", default="D")

# ---- 17 _note_target_error ----
for m in ["failed", "timeout", "other"]:
    case("_note_target_error", job={"id":"j1"}, msg=m, errors=[], io={"logger.warning":R()})
case("_note_target_error", job={"id":"j2"}, msg="x", errors=["prev"], io={"logger.warning":R()})
case("_note_target_error", job={"id":"j3"}, msg="y", errors=[], io={"logger.warning":[E("RuntimeError","x")]})

# ---- 18 _warn_live_lane_failure ----
for relay, m in [(True,"a"),(False,"b"),(True,"c"),(False,"d"),(True,"e")]:
    case("_warn_live_lane_failure", job={"id":"j1"}, msg=m, is_relay=relay, io={"logger.warning":R()})

# ---- 19 _resolve_target_transport ----
TR = lambda config=None, adapter=None, relay=False: {"config":config or {"enabled":True}, "adapter":adapter, "is_relay":relay}
case("_resolve_target_transport", job={}, platform="telegram", platform_name="telegram", target={},
     adapters={}, config={"platforms":{}},
     io={"resolve_delivery_transport":[R(TR({"enabled":True},"adapter1"))]})
case("_resolve_target_transport", job={}, platform="slack", platform_name="slack", target={},
     adapters={}, config={"platforms":{}},
     io={"resolve_delivery_transport":[R(None)],"relay_fronted_platforms":[R(["slack"])]})
case("_resolve_target_transport", job={}, platform="slack", platform_name="slack", target={},
     adapters={}, config={"platforms":{}},
     io={"resolve_delivery_transport":[R(None)],"relay_fronted_platforms":[R([])],
         "platforms_get":[R({"enabled":True})]})
case("_resolve_target_transport", job={}, platform="slack", platform_name="slack", target={},
     adapters={}, config={"platforms":{}},
     io={"resolve_delivery_transport":[R(None)],"relay_fronted_platforms":[R([])],
         "platforms_get":[R({"enabled":False})]})
case("_resolve_target_transport", job={}, platform="discord", platform_name="discord", target={"chat_id":"c1"},
     adapters={"__shared__":True}, config={"platforms":{}},
     io={"shared_get":[R("shared")],"platforms_get":[R({"enabled":True})],"replace":[R({"enabled":True})],
         "DeliveryTransport":[R(TR({"enabled":True},"shared"))]})
case("_resolve_target_transport", job={}, platform="discord", platform_name="discord", target={},
     adapters={"__shared__":True}, config={"platforms":{}},
     io={"shared_get":[R(None)],"resolve_delivery_transport":[R(TR({"enabled":True},"a"))]})

# ---- 20 _inchannel_surface_supported ----
case("_inchannel_surface_supported", adapter={"supports":True}, platform_name="telegram", io={})
case("_inchannel_surface_supported", adapter={"supports":False}, platform_name="telegram", io={})
case("_inchannel_surface_supported", adapter={"per_platform":True,"supports":False}, platform_name="telegram",
     io={"supports_inchannel_continuable_for_platform":[R(True)]})
case("_inchannel_surface_supported", adapter={"per_platform":True,"supports":True}, platform_name="telegram",
     io={"supports_inchannel_continuable_for_platform":[R(False)]})
case("_inchannel_surface_supported", adapter={"per_platform":True,"supports":True}, platform_name="telegram",
     io={"supports_inchannel_continuable_for_platform":[E("RuntimeError","x")]})
case("_inchannel_surface_supported", adapter={"per_platform":True,"supports":False}, platform_name="telegram",
     io={"supports_inchannel_continuable_for_platform":[E("SystemExit","x")]})

# ---- 21 _named_dm_topic ----
case("_named_dm_topic", t=T(platform="telegram",thread_id="t1",chat_id="12345"),
     io={"looks_like_telegram_private_chat_id":[R(True)],"_looks_like_int":[R(False)]})
case("_named_dm_topic", t=T(platform="telegram",thread_id="7",chat_id="12345"),
     io={"looks_like_telegram_private_chat_id":[R(True)],"_looks_like_int":[R(True)]})
case("_named_dm_topic", t=T(platform="telegram",thread_id="t1",chat_id="12345"),
     io={"looks_like_telegram_private_chat_id":[R(False)]})
case("_named_dm_topic", t=T(platform="discord",thread_id="t1",chat_id="12345"), io={})
case("_named_dm_topic", t=T(platform="telegram",thread_id=None,chat_id="12345"), io={})
case("_named_dm_topic", t=T(platform="telegram",thread_id="t1",chat_id="12345"),
     io={"looks_like_telegram_private_chat_id":[E("RuntimeError","x")]})

# ---- 22 _live_route_metadata ----
case("_live_route_metadata", t=T(thread_id=None,notify_delivery=False),
     io={"stamp_origin_discriminators":[R({})]})
case("_live_route_metadata", t=T(platform="telegram",thread_id="7",chat_id="12345",notify_delivery=True),
     io={"looks_like_telegram_private_chat_id":[R(True)],"_looks_like_int":[R(True)],
         "is_channel_dm_topic":[R(True)],"stamp_origin_discriminators":[R({})]})
case("_live_route_metadata", t=T(platform="telegram",thread_id="7",chat_id="12345",notify_delivery=True),
     io={"looks_like_telegram_private_chat_id":[R(True)],"_looks_like_int":[R(True)],
         "is_channel_dm_topic":[R(False)],"named_dm_topic":[R(None)],"stamp_origin_discriminators":[R({})]})
case("_live_route_metadata", t=T(platform="telegram",thread_id="name",chat_id="12345",notify_delivery=True),
     io={"looks_like_telegram_private_chat_id":[R(True)],"_looks_like_int":[R(False)],
         "named_dm_topic":[R("name")],"stamp_origin_discriminators":[R({})]})
case("_live_route_metadata", t=T(thread_id="t1",notify_delivery=False),
     io={"named_dm_topic":[R(None)],
         "stamp_origin_discriminators":[R({"route_metadata":{"scope_id":"s1"},"media_metadata":{"user_id":"u1"}})]})
case("_live_route_metadata", t=T(thread_id="t1"),
     io={"named_dm_topic":[R(None)],"stamp_origin_discriminators":[E("RuntimeError","x")]})

# ---- 23 _live_send_text ----
def lseams(**kw):
    d = {"DeliveryRouter":R(), "DeliveryTarget":R(), "safe_schedule_threadsafe":[R("fut")],
         "future.result":[R({"raw_response":{},"message_id":"m1"})],
         "result_field":[R({}),R("m1")],
         "confirm_adapter_delivery":[R({"ok":True,"evidence":[]})],
         "logger.warning":R()}
    d.update(kw); return d
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(safe_schedule_threadsafe=[R(None)]))
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[], io=lseams())
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(confirm_adapter_delivery=[R({"ok":True,"evidence":["w1"]})]))
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(**{"future.result":[R({"error":"bad"})]}, confirm_adapter_delivery=[R({"ok":False,"evidence":[]})],
               warn_live_lane_failure=R()))
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(**{"future.result":[E("TimeoutError","slow",started=False)]}))
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(**{"future.result":[R({"raw_response":{},"message_id":"m2"}),
                   R({"raw_response":{},"message_id":"m3"})]}))
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(**{"future.result":[E("PartialDeliveryError","partial",
         result={"raw_response":{"delivered_chunks":1,"total_chunks":3}})]}, note_target_error=R()))
case("_live_send_text", t=T(), text="hi", route_thread_id=None, route_metadata={},
     target_errors=[], delivery_errors=[], unverified_targets=[],
     io=lseams(**{"future.result":[E("RuntimeError","send failed",send_retryable=True)]}))

# ---- 24 _resolve_named_dm_topic ----
case("_resolve_named_dm_topic", t=T(), topic_name="name", metadata={}, delivery_errors=[], n_media=1, refresh=False,
     io={"_ensure_named_dm_topic":R(),"safe_schedule_threadsafe":[R("f")],"future.result":[R("t9")]})
case("_resolve_named_dm_topic", t=T(), topic_name="name", metadata={}, delivery_errors=[], n_media=0, refresh=True,
     io={"_ensure_named_dm_topic":R(),"safe_schedule_threadsafe":[R(None)],"logger.warning":R()})
case("_resolve_named_dm_topic", t=T(), topic_name="name", metadata={}, delivery_errors=[], n_media=2, refresh=False,
     io={"_ensure_named_dm_topic":R(),"safe_schedule_threadsafe":[R("f")],
         "future.result":[E("ValueError","gone")],"logger.warning":R()})
case("_resolve_named_dm_topic", t=T(), topic_name="name", metadata={}, delivery_errors=[], n_media=0, refresh=False,
     io={"_ensure_named_dm_topic":R(),"safe_schedule_threadsafe":[R("f")],
         "future.result":[E("SystemExit","halt")]})
case("_resolve_named_dm_topic", t=T(), topic_name="other", metadata={"x":1}, delivery_errors=[], n_media=1, refresh=True,
     io={"_ensure_named_dm_topic":R(),"safe_schedule_threadsafe":[R("f")],"future.result":[R("t10")]})

# ---- 25 _live_send_media ----
case("_live_send_media", t=T(), media_metadata={"notify":False}, media_files=[], delivery_errors=[],
     unverified_targets=[], topic_name=None,
     io={"send_media_via_adapter":[R([])]})
case("_live_send_media", t=T(), media_metadata={"notify":True}, media_files=["f1"], delivery_errors=[],
     unverified_targets=[], topic_name=None,
     io={"send_media_via_adapter":[R(["thread missing"])]})
case("_live_send_media", t=T(transport={"is_relay":True},config={"platforms":{}}),
     media_metadata={"notify":False}, media_files=[], delivery_errors=[], unverified_targets=[], topic_name=None,
     io={"get_home_channel":[R({"chat_id":"c1","user_id":"u1","scope_id":"s1"})],"send_media_via_adapter":[R([])]})
case("_live_send_media", t=T(), media_metadata={"notify":False}, media_files=["f1"], delivery_errors=[],
     unverified_targets=[], topic_name="name",
     io={"send_media_via_adapter":[R(["thread not found"]),R([])],
         "resolve_named_dm_topic":[R(True)]})
case("_live_send_media", t=T(job={"id":"j1","_media_ack_lost":True}), media_metadata={}, media_files=[],
     delivery_errors=[], unverified_targets=[], topic_name=None, io={"send_media_via_adapter":[R([])]})
case("_live_send_media", t=T(), media_metadata={}, media_files=[], delivery_errors=[], unverified_targets=[],
     topic_name=None, io={"send_media_via_adapter":[E("RuntimeError","x")]})

# ---- 26 _seed_live_delivery_sessions ----
case("_seed_live_delivery_sessions", t=T(opened_thread_id="th1",mirror_this_target=True), delivered_message_id=None,
     io={"seed_cron_thread_session":R(),"maybe_mirror_cron_delivery":R()})
case("_seed_live_delivery_sessions", t=T(in_channel_surface=True,inchannel_continuable=True,mirror_this_target=True,
     origin={"user_id":"u1"}), delivered_message_id="m1",
     io={"seed_cron_channel_session":[R(True)],"seed_cron_thread_session":R(),"maybe_mirror_cron_delivery":R()})
case("_seed_live_delivery_sessions", t=T(in_channel_surface=True,inchannel_continuable=True,mirror_this_target=True),
     delivered_message_id=None,
     io={"seed_cron_channel_session":[R(False)],"logger.warning":R(),"maybe_mirror_cron_delivery":R()})
case("_seed_live_delivery_sessions", t=T(in_channel_surface=True,inchannel_continuable=False), delivered_message_id=None,
     io={"logger.warning":R(),"maybe_mirror_cron_delivery":R()})
case("_seed_live_delivery_sessions", t=T(mirror_this_target=True), delivered_message_id=None,
     io={"maybe_mirror_cron_delivery":R()})

# ---- 27 _deliver_via_live_adapter ----
def dva(**kw):
    d = {"live_route_metadata":R({"route_thread_id":None,"route_metadata":{},"media_metadata":{}}),
         "named_dm_topic":[R(None)], "logger.info":R(), "seed_live_delivery_sessions":R()}
    d.update(kw); return d
case("_deliver_via_live_adapter", t=T(), cleaned_text="", media_files=[], target_errors=[], delivery_errors=[],
     unverified_targets=[], io=dva(note_target_error=R()))
case("_deliver_via_live_adapter", t=T(), cleaned_text=" hi ", media_files=[], target_errors=[], delivery_errors=[],
     unverified_targets=[], io=dva(live_send_text=[R([True,False,"m1"])]))
case("_deliver_via_live_adapter", t=T(), cleaned_text="hi", media_files=["f1"], target_errors=[], delivery_errors=[],
     unverified_targets=[], io=dva(named_dm_topic=[R("name")],live_send_text=[R([True,False,"m1"])],
         resolve_named_dm_topic=[R(False)],live_send_media=R()))
case("_deliver_via_live_adapter", t=T(), cleaned_text="hi", media_files=[], target_errors=[], delivery_errors=[],
     unverified_targets=[], io=dva(live_send_text=[E("RuntimeError","fail")],
         warn_live_lane_failure=R()))
case("_deliver_via_live_adapter", t=T(), cleaned_text="hi", media_files=["f1"], target_errors=[], delivery_errors=[],
     unverified_targets=[], io=dva(live_send_text=[R([True,True,None])],note_target_error=R()))
case("_deliver_via_live_adapter", t=T(), cleaned_text="hi", media_files=[], target_errors=[], delivery_errors=[],
     unverified_targets=[], io=dva(live_route_metadata=[E("RuntimeError","meta")],warn_live_lane_failure=R()))

# ---- 28 _deliver_standalone ----
case("_deliver_standalone", t=T(transport={"is_relay":True}), content="c", media_files=[], target_errors=[],
     delivery_errors=[], io={})
case("_deliver_standalone", t=T(), content="c", media_files=[], target_errors=[], delivery_errors=[],
     io={"standalone_send":[R({"result":{},"err":None})],"logger.info":R(),
         "record_cron_delivery":R(),"maybe_mirror_cron_delivery":R()})
case("_deliver_standalone", t=T(), content="c", media_files=[], target_errors=[], delivery_errors=[],
     io={"standalone_send":[R({"result":None,"err":"no route"})],"record_cron_delivery":R(),
         "queue_for_live_reconnect":R()})
case("_deliver_standalone", t=T(), content="c", media_files=[], target_errors=[], delivery_errors=[],
     io={"standalone_send":[R({"result":{"error":"boom"},"err":None})],"logger.error":R(),
         "delivery_state":[R("partial")],"record_cron_delivery":R(),"logger.warning":R()})
case("_deliver_standalone", t=T(), content="c", media_files=[], target_errors=[], delivery_errors=[],
     io={"standalone_send":[R({"result":{"warnings":["w1"]},"err":None})],"logger.error":R(),
         "logger.info":R(),"record_cron_delivery":R(),"maybe_mirror_cron_delivery":R()})
case("_deliver_standalone", t=T(), content="c", media_files=[], target_errors=[], delivery_errors=[],
     io={"standalone_send":[E("RuntimeError","x")]})

# ---- 29 _prepare_target_delivery ----
def pdi(**kw):
    d = {"resolve_origin":[R({})], "target_matches_origin":[R(False)],
         "Platform":[R("telegram")],
         "resolve_target_transport":[R({"ok":True,"error":None,"transport":None,
             "pconfig":{"enabled":True},"runtime_adapter":None,"target_adapters":{}})],
         "resolve_cron_surface_mode":[R("thread")]}
    d.update(kw); return d
case("_prepare_target_delivery", job={"id":"j1"}, target={"platform":"telegram","chat_id":"c1"},
     adapters={}, loop="loop1", config={"platforms":{}}, notify_delivery=False, mirror_enabled=False,
     mirror_text="x", delivery_errors=[], io=pdi())
case("_prepare_target_delivery", job={"id":"j1"}, target={"platform":"bogus","chat_id":"c1"},
     adapters={}, loop=None, config={"platforms":{}}, notify_delivery=False, mirror_enabled=False,
     mirror_text="x", delivery_errors=[], io=pdi(Platform=[E("ValueError","bad")],note_target_error=R()))
case("_prepare_target_delivery", job={"id":"j1"}, target={"platform":"telegram","chat_id":"c1"},
     adapters={}, loop=None, config={"platforms":{}}, notify_delivery=False, mirror_enabled=False,
     mirror_text="x", delivery_errors=[],
     io=pdi(resolve_target_transport=[R({"ok":False,"error":"not enabled","transport":None,"pconfig":None,
         "runtime_adapter":None,"target_adapters":{}})],note_target_error=R()))
case("_prepare_target_delivery", job={"id":"j1"}, target={"platform":"telegram","chat_id":"c1"},
     adapters={}, loop=None, config={"platforms":{}}, notify_delivery=False, mirror_enabled=False,
     mirror_text="x", delivery_errors=[],
     io=pdi(resolve_origin=[R({"thread_id":"t0"})],**{"logger.warning":R()}))
case("_prepare_target_delivery", job={"id":"j1"}, target={"platform":"telegram","chat_id":"c1","thread_id":"t1"},
     adapters={}, loop=None, config={"platforms":{}}, notify_delivery=False, mirror_enabled=False,
     mirror_text="x", delivery_errors=[], io=pdi(**{"logger.debug":R()}))
case("_prepare_target_delivery", job={"id":"j1"}, target={"platform":"telegram","chat_id":"c1"},
     adapters={}, loop="loop1", config={"platforms":{}}, notify_delivery=True, mirror_enabled=True,
     mirror_text="x", delivery_errors=[],
     io=pdi(resolve_origin=[R({"user_id":"u1","thread_id":None})],target_matches_origin=[R(True)],
         target_mirror_eligible=[R(True)],resolve_cron_surface_mode=[R("in_channel")],
         loop_is_running=[R(True)],inchannel_surface_supported=[R(True)],
         open_continuable_cron_thread=[R("newth")],
         resolve_target_transport=[R({"ok":True,"error":None,"transport":{"is_relay":False},
             "pconfig":{"enabled":True},"runtime_adapter":"ad1","target_adapters":{}})]))

# ---- 30 _unresolved_delivery_outcome ----
case("_unresolved_delivery_outcome", job={"id":"j1"}, for_failure=False,
     io={"delivery_lane_value":[R("local")],"normalize_deliver_value":[R("local")]})
case("_unresolved_delivery_outcome", job={"name":"n"}, for_failure=False,
     io={"delivery_lane_value":[R("origin")],"normalize_deliver_value":[R("origin")],"logger.info":R()})
case("_unresolved_delivery_outcome", job={"id":"j1"}, for_failure=True,
     io={"delivery_lane_value":[R("telegram")],"normalize_deliver_value":[R("telegram")],"logger.warning":R()})
case("_unresolved_delivery_outcome", job={"id":"j1"}, for_failure=False,
     io={"delivery_lane_value":[R("")],"normalize_deliver_value":[R("")],"logger.warning":R()})
case("_unresolved_delivery_outcome", job={"id":"j1"}, for_failure=False,
     io={"delivery_lane_value":[E("RuntimeError","x")]})

# ---- 31 _should_wrap_cron_response ----
case("_should_wrap_cron_response", job={"wrap_response":True}, user_cfg={"cron":{"wrap_response":False}})
case("_should_wrap_cron_response", job={"wrap_response":False}, user_cfg={"cron":{"wrap_response":True}})
case("_should_wrap_cron_response", job={"wrap_response":1}, user_cfg={"cron":{"wrap_response":False}})
case("_should_wrap_cron_response", job={}, user_cfg={"cron":{"wrap_response":False}})
case("_should_wrap_cron_response", job={}, user_cfg={})
case("_should_wrap_cron_response", job={}, user_cfg=None)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in C:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
from collections import Counter
cnt = Counter(c["func"] for c in C)
assert all(v >= 5 for v in cnt.values()), dict(cnt)
print(f"wrote {len(C)} cases across {len(cnt)} functions -> {OUT}")
