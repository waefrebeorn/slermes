#!/usr/bin/env python3
"""Generate tests/fixtures/google_chat_adapter_flow2.jsonl for the Google Chat
adapter remainder FLOW port (src/gateway/platforms/google_chat_adapter_flow2.c).

Case shape: {"func": <live name>, "in": {"args": {...}, "state": {...},
"io": {seam-name: script}}}. A script is a plain JSON value (constant), a
LIST consumed one element per call (wrap list/tuple RESULTS one level deeper),
or {"__script__": true, "ret": v} / {"__script__": true, "raise": {type,...}}.
Missing scripts are fail-closed RuntimeErrors on both sides.

Every ported function gets >= 5 cases; error paths, BaseException propagation
and fail-closed scripts are covered per flow. ASCII corpus; integral numbers.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "google_chat_adapter_flow2.jsonl")

CASES = []


def add(func, args=None, state=None, io=None, fc=False):
    c = {"func": func, "in": {"args": args or {}, "state": state or {}, "io": io or {}}}
    if fc:
        c["fc"] = True
    CASES.append(c)


def rs(kind, msg, **extra):
    r = {"type": kind, "message": msg}
    r.update(extra)
    return {"__script__": True, "raise": r}


def ret(v):
    return {"__script__": True, "ret": v}


def exc(cls, msg="", attrs=None):
    return {"cls": cls, "msg": msg, "attrs": attrs or {}}


def obj(attrs):
    return {"__obj__": attrs}


def sa_err(kind, detail=None):
    return {"__script__": True, "raise": {"type": "_SACredentialError", "message": kind,
                                          "kind": kind, "detail": detail}}


def sr(mid=None, success=True, **kw):
    d = {"success": success}
    if mid is not None:
        d["message_id"] = mid
    d.update(kw)
    return d


SCOPE = "https://www.googleapis.com/auth/chat.bot"
SCOPE2 = "https://www.googleapis.com/auth/pubsub"

# ── _load_sa_credentials_from ──
add("_load_sa_credentials_from", {"sa_value": '{"a":1}'},
    io={"json.loads": {"a": 1}, "service_account.Credentials.from_service_account_info": obj({"token": "T"})})
add("_load_sa_credentials_from", {"sa_value": "{bad"},
    io={"json.loads": rs("JSONDecodeError", "Expecting value: line 1 column 2 (char 1)")})
add("_load_sa_credentials_from", {"sa_value": "/no.json"}, io={"os.path.exists": False})
add("_load_sa_credentials_from", {"sa_value": "/sa.json"},
    io={"os.path.exists": True, "path.read_text": '{"k":2}', "json.loads": {"k": 2},
        "service_account.Credentials.from_service_account_info": obj({"token": "T"})})
add("_load_sa_credentials_from", {"sa_value": "/sa.json"},
    io={"os.path.exists": True, "path.read_text": "bad",
        "json.loads": rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)")})
add("_load_sa_credentials_from", {"sa_value": None}, io={"_adc_would_borrow_foreign_credentials": True})
add("_load_sa_credentials_from", {"sa_value": None},
    io={"_adc_would_borrow_foreign_credentials": False, "import.google.auth": rs("ImportError", "no module named google")})
add("_load_sa_credentials_from", {"sa_value": None},
    io={"_adc_would_borrow_foreign_credentials": False, "import.google.auth": None,
        "google_auth.default": rs("RuntimeError", "adc boom")})
add("_load_sa_credentials_from", {"sa_value": None},
    io={"_adc_would_borrow_foreign_credentials": False, "import.google.auth": None,
        "google_auth.default": obj({"token": "ADC"})})
add("_load_sa_credentials_from", {"sa_value": '{"a":1}'}, io={"json.loads": rs("SystemExit", "bye")})
add("_load_sa_credentials_from", {"sa_value": '{"a":1}'}, fc=True)
add("_load_sa_credentials_from", {"sa_value": "/sa.json"},
    io={"os.path.exists": True, "path.read_text": rs("OSError", "permission denied", errno=13)})
add("_load_sa_credentials_from", {"sa_value": "  {\"x\":1}"},
    io={"json.loads": {"x": 1}, "service_account.Credentials.from_service_account_info": obj({"token": "T"})})

# ── store.load ──
add("store.load", state={"_path": "/h/counts.json", "_counts": {}}, io={"path.exists": False})
add("store.load", state={"_path": "/h/counts.json"},
    io={"path.exists": True, "path.read_text": '{"c":{"t":2}}', "json.loads": {"c": {"t": 2}}})
add("store.load", state={"_path": "/h/counts.json"},
    io={"path.exists": True, "path.read_text": "bad",
        "json.loads": rs("JSONDecodeError", "Expecting value: line 1 column 1 (char 0)"), "logger.warning": None})
add("store.load", state={"_path": "/h/counts.json"},
    io={"path.exists": True, "path.read_text": rs("OSError", "io error", errno=5), "logger.warning": None})
add("store.load", state={"_path": "/h/counts.json"}, io={"path.exists": True, "path.read_text": "   "})
add("store.load", state={"_path": "/h/counts.json"},
    io={"path.exists": True, "path.read_text": "x",
        "json.loads": {"c": {"t": 2, "bad": "x", "u": True}, "d": 5, "e": {"v": 1.5}}})
add("store.load", state={"_path": "/h/counts.json"},
    io={"path.exists": True, "path.read_text": "x", "json.loads": [1, 2]})
add("store.load", state={"_path": "/h/counts.json"}, io={"path.exists": True, "path.read_text": rs("KeyboardInterrupt", "x")})
add("store.load", state={"_path": "/h/counts.json"}, io={"path.exists": True}, fc=True)
add("store.load", state={"_path": "/h/counts.json"},
    io={"path.exists": True, "path.read_text": "\ufeff{\"c\":{\"t\":1}}", "json.loads": {"c": {"t": 1}}})

# ── store.get ──
add("store.get", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 3}}})
add("store.get", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {}})
add("store.get", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {}}})
add("store.get", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 0}}})
add("store.get", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": True}}})
add("store.get", {"chat_id": "x", "thread_name": "y"}, state={"_counts": {"c": {"t": 3}}})

# ── store.incr ──
add("store.incr", {"chat_id": "c", "thread_name": "t"}, state={"_path": "/h/c.json", "_counts": {"c": {"t": 2}}}, io={"store._save": None})
add("store.incr", {"chat_id": "c", "thread_name": "t"}, state={"_path": "/h/c.json", "_counts": {}}, io={"store._save": None})
add("store.incr", {"chat_id": "c", "thread_name": "t"}, state={"_path": "/h/c.json", "_counts": {"c": {}}}, io={"store._save": None})
add("store.incr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 2}}}, io={"store._save": rs("OSError", "x", errno=5)})
add("store.incr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {}}, io={"store._save": rs("SystemExit", "bye")})
add("store.incr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {}}, fc=True)

# ── store.decr ──
add("store.decr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 2}}}, io={"store._save": None})
add("store.decr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 0}}})
add("store.decr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {}})
add("store.decr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {}}})
add("store.decr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 1}}}, io={"store._save": rs("OSError", "x")})
add("store.decr", {"chat_id": "c", "thread_name": "t"}, state={"_counts": {"c": {"t": 1}}}, io={"store._save": None})

# ── store._save ──
add("store._save", state={"_path": "/h/c.json", "_counts": {"c": {"t": 1}}},
    io={"path.mkdir": None, "path.write_text": None, "os.replace": None})
add("store._save", state={"_path": "/h/c.json", "_counts": {}},
    io={"path.mkdir": rs("OSError", "x", errno=13), "logger.warning": None})
add("store._save", state={"_path": "/h/c.json", "_counts": {"c": {"t": 1}}},
    io={"path.mkdir": None, "path.write_text": rs("OSError", "x", errno=28), "logger.warning": None})
add("store._save", state={"_path": "/h/c.json", "_counts": {"c": {"t": 1}}},
    io={"path.mkdir": None, "path.write_text": None, "os.replace": rs("OSError", "x"), "logger.warning": None})
add("store._save", state={"_path": "/h/c.json", "_counts": {}}, io={"path.mkdir": rs("SystemExit", "bye")})
add("store._save", state={"_path": "/h/c.json", "_counts": {}}, io={"path.mkdir": rs("RuntimeError", "x")})
add("store._save", state={"_path": "/h/c.json", "_counts": {}}, fc=True)

# ── _load_sa_credentials ──
CFG_SA = {"config": obj({"extra": {"service_account_json": "/p.json"}})}
add("_load_sa_credentials", state=CFG_SA, io={"_load_sa_credentials_from": obj({"token": "T"})})
add("_load_sa_credentials", state={"config": obj({"extra": {}})},
    io={"get_scoped_secret": "/p.json", "_load_sa_credentials_from": obj({"token": "T"})})
add("_load_sa_credentials", state=CFG_SA, io={"_load_sa_credentials_from": sa_err("not_found")})
add("_load_sa_credentials", state=CFG_SA,
    io={"_load_sa_credentials_from": sa_err("inline_invalid", {"__raise__": "JSONDecodeError", "message": "Expecting value"})})
add("_load_sa_credentials", state=CFG_SA,
    io={"_load_sa_credentials_from": sa_err("adc_failed", {"__raise__": "RuntimeError", "message": "boom"})})
add("_load_sa_credentials", state={"config": obj({"extra": {}})},
    io={"get_scoped_secret": None, "_load_sa_credentials_from": obj({"token": "ADC"}), "logger.info": None})
add("_load_sa_credentials", state=CFG_SA, io={"_load_sa_credentials_from": rs("SystemExit", "bye")})
add("_load_sa_credentials", state=CFG_SA, fc=True)

# ── _bot_id_cache_path ──
add("_bot_id_cache_path", io={"import.hermes_constants": None, "get_hermes_home": "/home/u"})
add("_bot_id_cache_path", io={"import.hermes_constants": None, "get_hermes_home": "/home/u/"})
add("_bot_id_cache_path", io={"import.hermes_constants": None, "get_hermes_home": "/"})
add("_bot_id_cache_path", io={"import.hermes_constants": rs("ImportError", "x")})
add("_bot_id_cache_path", io={"import.hermes_constants": None, "get_hermes_home": rs("RuntimeError", "x")})
add("_bot_id_cache_path", io={"import.hermes_constants": None, "get_hermes_home": rs("SystemExit", "bye")})
add("_bot_id_cache_path", io={"import.hermes_constants": None}, fc=True)

# ── _save_cached_bot_id ──
add("_save_cached_bot_id", {"bot_user_id": "users/1"},
    io={"_bot_id_cache_path": "/h/bot.json", "path.mkdir": None, "path.write_text": None})
add("_save_cached_bot_id", {"bot_user_id": "users/1"},
    io={"_bot_id_cache_path": "/h/bot.json", "path.mkdir": rs("OSError", "x", errno=13), "logger.debug": None})
add("_save_cached_bot_id", {"bot_user_id": "users/1"},
    io={"_bot_id_cache_path": "/h/bot.json", "path.mkdir": None, "path.write_text": rs("OSError", "x"), "logger.debug": None})
add("_save_cached_bot_id", {"bot_user_id": "users/1"},
    io={"_bot_id_cache_path": rs("OSError", "x"), "logger.debug": None})
add("_save_cached_bot_id", {"bot_user_id": "users/1"},
    io={"_bot_id_cache_path": "/h/bot.json", "path.mkdir": rs("SystemExit", "bye")})
add("_save_cached_bot_id", {"bot_user_id": "users/1"},
    io={"_bot_id_cache_path": "/h/bot.json", "path.mkdir": rs("RuntimeError", "x")})
add("_save_cached_bot_id", {"bot_user_id": "users/1"}, io={"_bot_id_cache_path": "/h/bot.json"}, fc=True)

# ── _resolve_bot_user_id ──
CFG_HOME = {"config": obj({"home_channel": obj({"chat_id": "spaces/H"})})}
BOT_MEMBERS = {"memberships": [{"member": {"type": "BOT", "name": "users/9"}}]}
add("_resolve_bot_user_id", state=CFG_HOME, io={"asyncio.to_thread": BOT_MEMBERS})
add("_resolve_bot_user_id", state={"config": obj({}), "_bootstrap_spaces": "spaces/B, spaces/C"},
    io={"asyncio.to_thread": [{"memberships": []}, BOT_MEMBERS]})
add("_resolve_bot_user_id", state={"config": obj({}), "_bootstrap_spaces": ""})
add("_resolve_bot_user_id", state=CFG_HOME,
    io={"asyncio.to_thread": [rs("HttpError", "boom"), BOT_MEMBERS], "_exc_text": "x", "logger.debug": None})
add("_resolve_bot_user_id", state=CFG_HOME,
    io={"asyncio.to_thread": {"memberships": [{"member": {"type": "HUMAN", "name": "u"}}]}})
add("_resolve_bot_user_id", state=CFG_HOME, io={"asyncio.to_thread": rs("SystemExit", "bye")})
add("_resolve_bot_user_id", state=CFG_HOME, io={"asyncio.to_thread": None})
add("_resolve_bot_user_id", state=CFG_HOME, fc=True)

# ── _load_user_oauth ──
add("_load_user_oauth",
    io={"import.oauth": [None, None],
        "asyncio.to_thread": [obj({"token": "U"}), obj({"api": 1}), ["a@b.com", "c@d.com"]],
        "logger.info": [None, None]})
add("_load_user_oauth",
    io={"import.oauth": [None, None], "asyncio.to_thread": [None, ["a@b.com"]], "logger.info": None})
add("_load_user_oauth",
    io={"import.oauth": [None, None], "asyncio.to_thread": [None, []], "logger.info": None})
add("_load_user_oauth",
    io={"import.oauth": rs("ImportError", "x"), "_exc_text": "x", "logger.warning": None})
add("_load_user_oauth",
    io={"import.oauth": [None, None], "asyncio.to_thread": rs("RuntimeError", "boom"),
        "_exc_text": "x", "logger.warning": None})
add("_load_user_oauth", io={"import.oauth": [None, None], "asyncio.to_thread": rs("SystemExit", "bye")})
add("_load_user_oauth", io={"import.oauth": [None, None]}, fc=True)

# ── _check_subscription ──
SUB_ARGS = {"subscription_path": "projects/p/subscriptions/s", "credentials": obj({"token": "T"})}
add("_check_subscription", SUB_ARGS, io={"pubsub_v1.SubscriberClient": obj({"id": "sub"}), "asyncio.to_thread": None})
add("_check_subscription", SUB_ARGS,
    io={"pubsub_v1.SubscriberClient": obj({"id": "sub"}), "asyncio.to_thread": rs("NotFound", "x"), "_set_fatal_error": None})
add("_check_subscription", SUB_ARGS,
    io={"pubsub_v1.SubscriberClient": obj({"id": "sub"}), "asyncio.to_thread": rs("PermissionDenied", "x"), "_set_fatal_error": None})
add("_check_subscription", SUB_ARGS,
    io={"pubsub_v1.SubscriberClient": obj({"id": "sub"}), "asyncio.to_thread": rs("RuntimeError", "x"), "_connect_failed": False})
add("_check_subscription", SUB_ARGS,
    io={"pubsub_v1.SubscriberClient": obj({"id": "sub"}), "asyncio.to_thread": rs("SystemExit", "bye")})
add("_check_subscription", SUB_ARGS, io={"pubsub_v1.SubscriberClient": rs("RuntimeError", "x")})
add("_check_subscription", SUB_ARGS, io={"pubsub_v1.SubscriberClient": obj({"id": "sub"})}, fc=True)

# ── _handle_pubsub_message ──
def msg(data, ce="MESSAGE", attrs=None):
    a = {"data": data}
    a["attributes"] = attrs if attrs is not None else {"ce-type": ce}
    return obj(a)


ENV_MSG = {"type": "MESSAGE", "message": {"name": "m", "sender": {"type": "HUMAN"}}, "space": {"name": "spaces/A"}}
add("_handle_pubsub_message", {"message": msg('{"type":"MESSAGE"}')},
    state={"_shutting_down": False, "_debug_raw": False},
    io={"json.loads": ENV_MSG, "logger.debug": None, "_prepare_inbound": [[{"name": "m"}, ENV_MSG]],
        "_submit_on_loop": None, "message.ack": None})
add("_handle_pubsub_message", {"message": msg('{"type":"MESSAGE"}')},
    state={"_shutting_down": True}, io={"message.nack": None})
add("_handle_pubsub_message", {"message": msg("bad")},
    state={"_shutting_down": False},
    io={"json.loads": rs("JSONDecodeError", "x"), "logger.exception": None, "message.ack": None})
MEMB_ENV = {"chat": {"membershipPayload": {"space": {"name": "spaces/S"},
            "membership": {"member": {"type": "BOT", "name": "users/7"}}}}}
add("_handle_pubsub_message", {"message": msg("x", ce="google.chat.membership.created.v1")},
    state={"_shutting_down": False, "_debug_raw": False, "_bot_user_id": None},
    io={"json.loads": MEMB_ENV, "logger.debug": None, "_save_cached_bot_id": None,
        "logger.info": None, "message.ack": None})
add("_handle_pubsub_message", {"message": msg("x", ce="google.chat.card.v1")},
    state={"_shutting_down": False, "_debug_raw": False},
    io={"json.loads": {"a": 1}, "logger.debug": None, "logger.info": None, "message.ack": None})
add("_handle_pubsub_message", {"message": msg('{"type":"MESSAGE"}')},
    state={"_shutting_down": False, "_debug_raw": True},
    io={"json.loads": ENV_MSG, "logger.debug": [None, None], "import.agent.redact": None,
        "redact_sensitive_text": "REDACTED", "_prepare_inbound": [[{"name": "m"}, ENV_MSG]],
        "_submit_on_loop": None, "message.ack": None})
add("_handle_pubsub_message", {"message": msg('{"type":"MESSAGE"}')},
    state={"_shutting_down": False, "_debug_raw": False},
    io={"json.loads": ENV_MSG, "logger.debug": None, "_prepare_inbound": None, "message.ack": None})
add("_handle_pubsub_message", {"message": msg("x")},
    state={"_shutting_down": False}, io={"json.loads": rs("SystemExit", "bye")})
add("_handle_pubsub_message", {"message": msg('{"type":"MESSAGE"}')},
    state={"_shutting_down": False, "_debug_raw": False},
    io={"json.loads": ENV_MSG, "logger.debug": None, "_prepare_inbound": rs("RuntimeError", "x"),
        "logger.exception": None, "message.ack": rs("OSError", "ack fail")})
add("_handle_pubsub_message", {"message": msg('{"type":"MESSAGE"}')},
    state={"_shutting_down": False, "_debug_raw": False}, io={"json.loads": ENV_MSG}, fc=True)

# ── dispatch_http_event ──
PREP = [[{"name": "m"}, {"type": "MESSAGE"}]]
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": None, "_prepare_inbound": None})
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": None, "_prepare_inbound": PREP,
        "retryable_ingress": {"refused": False}, "_dispatch_message": None})
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": None, "_prepare_inbound": PREP,
        "retryable_ingress": {"refused": True}, "_dispatch_message": None, "_release_unadmitted": None})
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": None, "_prepare_inbound": PREP,
        "retryable_ingress": {"refused": False}, "_dispatch_message": rs("RuntimeError", "x")})
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": None, "_prepare_inbound": PREP,
        "retryable_ingress": {"refused": False}, "_dispatch_message": rs("SystemExit", "bye")})
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": rs("ImportError", "x")})
add("dispatch_http_event", {"envelope": {"type": "MESSAGE"}},
    io={"import.gateway.parked_route_refusal": None}, fc=True)

# ── _dispatch_message ──
def event(text="hi", source=True):
    src = obj({"user_id": "u", "chat_type": "dm", "chat_id": "c", "thread_id": None, "user_id_alt": "users/1"}) if source else None
    return obj({"text": text, "source": src})


add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}},
    io={"_build_message_event": event(), "handle_message": None})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}}, io={"_build_message_event": None})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}},
    io={"_build_message_event": event("/setup-files now"), "_is_sender_authorized": True, "_handle_setup_files_command": True})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}},
    io={"_build_message_event": event("/setup-files now"), "_is_sender_authorized": False, "handle_message": None})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}},
    io={"_build_message_event": rs("RuntimeError", "x"), "logger.exception": None})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}}, io={"_build_message_event": rs("SystemExit", "bye")})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}},
    io={"_build_message_event": event(), "handle_message": rs("RuntimeError", "x"), "logger.exception": None})
add("_dispatch_message", {"msg": {"name": "m"}, "envelope": {}}, fc=True)

# ── _build_message_event ──
def bm_msg(text="hi", **kw):
    d = {"name": "m", "text": text, "sender": {"name": "users/1", "email": "A@B.com"}}
    d.update(kw)
    return d


DM_ENV = {"space": {"name": "spaces/A", "spaceType": "DIRECT_MESSAGE"}}
add("_build_message_event", {"msg": bm_msg(thread={"name": "spaces/A/threads/T"}), "envelope": DM_ENV},
    io={"thread_count_store.incr": 0, "build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": "hi"})})
add("_build_message_event", {"msg": bm_msg(thread={"name": "spaces/A/threads/T"}), "envelope": DM_ENV},
    io={"thread_count_store.incr": 1, "build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": "hi"})})
add("_build_message_event", {"msg": bm_msg(thread={"name": "spaces/A/threads/T"}),
                             "envelope": {"space": {"name": "spaces/A", "spaceType": "SPACE"}}},
    io={"thread_count_store.incr": 0, "build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": "hi"})})
add("_build_message_event", {"msg": bm_msg(text="", attachment=[{"contentType": "image/png"}]), "envelope": DM_ENV},
    io={"_is_sender_authorized": True, "_download_attachment": [["/cache/x.jpg", "image/png"]],
        "_mime_for_message_type": "photo", "build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": ""})})
add("_build_message_event", {"msg": bm_msg(attachment=[{"contentType": "image/png"}]), "envelope": DM_ENV},
    io={"_is_sender_authorized": True, "_download_attachment": rs("RuntimeError", "x"),
        "_exc_text": "x", "_proxy_error": None, "logger.error": None,
        "build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": "hi"})})
add("_build_message_event", {"msg": bm_msg(attachment=[{"contentType": "image/png"}]), "envelope": DM_ENV},
    io={"_is_sender_authorized": False, "build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": "hi"})})
add("_build_message_event", {"msg": bm_msg(slashCommand={"commandId": "1"}), "envelope": DM_ENV},
    io={"build_source": obj({"chat_id": "spaces/A"}), "MessageEvent": obj({"text": "/cmd_1 hi"})})
add("_build_message_event", {"msg": bm_msg(thread={"name": "spaces/A/threads/T"}), "envelope": DM_ENV},
    io={"thread_count_store.incr": rs("SystemExit", "bye")})
add("_build_message_event", {"msg": bm_msg(), "envelope": DM_ENV}, fc=True)

# ── _download_attachment ──
ATT_IMG = {"contentType": "image/png", "name": "spaces/a/messages/m/attachments/att",
           "attachmentDataRef": {"resourceName": "RN"}}
add("_download_attachment", {"attachment": ATT_IMG},
    io={"asyncio.to_thread": "DATA", "cache_image_from_bytes_async": "/c/x.jpg"})
add("_download_attachment", {"attachment": {"contentType": "image/png", "source": "DRIVE_FILE"}},
    io={"logger.info": None})
add("_download_attachment", {"attachment": {"contentType": "image/png", "attachmentDataRef": {"resourceName": "RN"},
                                            "downloadUri": "https://chat.google.com/u"}},
    io={"asyncio.to_thread": [rs("HttpError", "boom"), "DATA"], "_exc_text": "x", "logger.warning": None,
        "_is_google_owned_host": True, "cache_image_from_bytes_async": "/c/x.jpg"})
add("_download_attachment", {"attachment": {"contentType": "image/png", "downloadUri": "https://evil.com/u"}},
    io={"_is_google_owned_host": False, "logger.warning": None})
add("_download_attachment", {"attachment": {"contentType": "image/png", "downloadUri": "https://chat.google.com/u"}},
    io={"_is_google_owned_host": True, "asyncio.to_thread": rs("RuntimeError", "x"),
        "_exc_text": "x", "logger.warning": None})
add("_download_attachment", {"attachment": {"contentType": "application/pdf", "name": "att.pdf",
                                            "attachmentDataRef": {"resourceName": "RN"}}},
    io={"asyncio.to_thread": "DATA", "cache_document_from_bytes_async": "/c/f.pdf"})
add("_download_attachment", {"attachment": {"contentType": "image/png"}})
add("_download_attachment", {"attachment": ATT_IMG}, io={"asyncio.to_thread": rs("SystemExit", "bye")})
add("_download_attachment", {"attachment": ATT_IMG}, fc=True)

# ── send ──
SEND_IO = {"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
           "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
           "_create_message": sr("M"), "resume_typing_for_chat": None}
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None}, io=SEND_IO)
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    state={"_typing_messages": {"c": "spaces/A/messages/T"}},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
        "_patch_message": sr("T"), "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "",
        "_chunk_text": [[]], "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": "t", "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["a", "b"]], "_thread_body": [{"text": "a"}, {"text": "b"}],
        "_create_message": [sr("M1"), sr("M2")], "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
        "_create_message": rs("HttpError", "forbidden"), "_http_status": 403,
        "_set_fatal_error": None, "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    state={"_typing_messages": {"c": "T"}},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
        "_patch_message": rs("HttpError", "gone"), "_http_status": 404, "logger.info": None,
        "_create_message": sr("M"), "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
        "_create_message": rs("HttpError", "gone"), "_http_status": 404, "logger.info": None,
        "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
        "_create_message": rs("HttpError", "slow"), "_http_status": 429, "_note_rate_limit": 5,
        "logger.warning": None, "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None, "format_message": "hi",
        "_chunk_text": [["hi"]], "_thread_body": {"text": "hi"},
        "_create_message": rs("SystemExit", "bye"), "resume_typing_for_chat": None})
add("send", {"chat_id": "c", "content": "hi", "reply_to": None, "metadata": None},
    io={"_resolve_thread_id": None, "pause_typing_for_chat": None}, fc=True)

# ── send_card ──
add("send_card", {"chat_id": "c", "card": {"card": {}}, "metadata": None},
    io={"_resolve_thread_id": None, "_create_message": sr("M")})
add("send_card", {"chat_id": "c", "card": {"card": {}}, "metadata": None},
    io={"_resolve_thread_id": "t", "_create_message": sr("M")})
add("send_card", {"chat_id": "c", "card": {"card": {}}, "metadata": None},
    io={"_resolve_thread_id": None, "_create_message": rs("HttpError", "x"), "_exc_text": "x", "_http_status": 503})
add("send_card", {"chat_id": "c", "card": {"card": {}}, "metadata": None},
    io={"_resolve_thread_id": None, "_create_message": rs("RuntimeError", "x"),
        "_proxy_error": None, "logger.debug": None, "_exc_text": "x", "_is_retryable_error": False})
add("send_card", {"chat_id": "c", "card": {"card": {}}, "metadata": None},
    io={"_resolve_thread_id": None, "_create_message": rs("SystemExit", "bye")})
add("send_card", {"chat_id": "c", "card": {"card": {}}, "metadata": None}, io={"_resolve_thread_id": None}, fc=True)

# ── edit_message ──
add("edit_message", {"chat_id": "c", "message_id": "M", "content": "x"}, io={"_patch_message": sr("M")})
add("edit_message", {"chat_id": "c", "message_id": "", "content": "x"})
add("edit_message", {"chat_id": "c", "message_id": "M", "content": "a" * 4001}, io={"_patch_message": sr("M")})
add("edit_message", {"chat_id": "c", "message_id": "M", "content": "x"},
    io={"_patch_message": rs("HttpError", "slow"), "_http_status": 429, "_note_rate_limit": None, "_exc_text": "x"})
add("edit_message", {"chat_id": "c", "message_id": "M", "content": "x"},
    io={"_patch_message": rs("RuntimeError", "x"), "_proxy_error": None, "logger.debug": None, "_exc_text": "x"})
add("edit_message", {"chat_id": "c", "message_id": "M", "content": "x"}, io={"_patch_message": rs("SystemExit", "bye")})
add("edit_message", {"chat_id": "c", "message_id": "M", "content": "x"}, fc=True)

# ── delete_message ──
add("delete_message", {"chat_id": "c", "message_id": "M"}, io={"asyncio.to_thread": None})
add("delete_message", {"chat_id": "c", "message_id": ""})
add("delete_message", {"chat_id": "c", "message_id": "M"},
    io={"asyncio.to_thread": rs("HttpError", "gone"), "_http_status": 404})
add("delete_message", {"chat_id": "c", "message_id": "M"},
    io={"asyncio.to_thread": rs("HttpError", "boom"), "_http_status": 500, "_exc_text": "x", "logger.debug": None})
add("delete_message", {"chat_id": "c", "message_id": "M"},
    io={"asyncio.to_thread": rs("RuntimeError", "x"), "_proxy_error": None, "logger.debug": None})
add("delete_message", {"chat_id": "c", "message_id": "M"}, io={"asyncio.to_thread": rs("SystemExit", "bye")})
add("delete_message", {"chat_id": "c", "message_id": "M"}, fc=True)

# ── _patch_message ──
add("_patch_message", {"message_name": "spaces/A/messages/M", "body": {"text": "hi"}},
    io={"asyncio.to_thread": {"name": "spaces/A/messages/M2"}})
add("_patch_message", {"message_name": "M", "body": {"cardsV2": []}}, io={"asyncio.to_thread": {"name": "M2"}})
add("_patch_message", {"message_name": "M", "body": {"text": "hi", "cardsV2": [], "thread": {"name": "t"}}},
    io={"asyncio.to_thread": {"name": "M2"}})
add("_patch_message", {"message_name": "M", "body": {"text": "hi"}}, io={"asyncio.to_thread": {}})
add("_patch_message", {"message_name": "M", "body": {"text": "hi"}}, io={"asyncio.to_thread": rs("HttpError", "x")})
add("_patch_message", {"message_name": "M", "body": {"text": "hi"}}, io={"asyncio.to_thread": None})
add("_patch_message", {"message_name": "M", "body": {"text": "hi"}}, fc=True)

# ── _create_message ──
add("_create_message", {"chat_id": "spaces/A", "body": {"text": "hi"}},
    io={"_create_kwargs": {"parent": "spaces/A", "body": {"text": "hi"}},
        "_call_with_retry": {"name": "spaces/A/messages/N", "thread": {"name": "t"}}, "_track_outbound_thread": None})
add("_create_message", {"chat_id": "spaces/A", "body": {"text": "hi"}},
    io={"_create_kwargs": {"parent": "spaces/A", "body": {"text": "hi"}},
        "_call_with_retry": {}, "_track_outbound_thread": None})
add("_create_message", {"chat_id": "spaces/A", "body": {"text": "hi"}},
    io={"_create_kwargs": {"parent": "spaces/A", "body": {"text": "hi"}}, "_call_with_retry": rs("HttpError", "x")})
add("_create_message", {"chat_id": "spaces/A", "body": {"text": "hi"}},
    io={"_create_kwargs": {"parent": "spaces/A", "body": {"text": "hi"}},
        "_call_with_retry": {"name": "N"}, "_track_outbound_thread": rs("RuntimeError", "x")})
add("_create_message", {"chat_id": "spaces/A", "body": {"text": "hi"}},
    io={"_create_kwargs": {"parent": "spaces/A", "body": {"text": "hi"}}, "_call_with_retry": rs("SystemExit", "bye")})
add("_create_message", {"chat_id": "spaces/A", "body": {"text": "hi"}}, fc=True)

# ── _call_with_retry ──
add("_call_with_retry", {"sync_fn": {"__callable__": "op"}, "op_name": "messages.create"},
    io={"asyncio.to_thread": {"ok": 1}})
add("_call_with_retry", {"sync_fn": {"__callable__": "op"}, "op_name": "messages.create"},
    io={"asyncio.to_thread": [rs("HttpError", "boom"), {"ok": 1}], "_is_retryable_error": True,
        "random.random": 0.5, "_exc_text": "x", "logger.warning": None, "asyncio.sleep": None})
add("_call_with_retry", {"sync_fn": {"__callable__": "op"}, "op_name": "messages.create"},
    io={"asyncio.to_thread": rs("HttpError", "boom"), "_is_retryable_error": False})
add("_call_with_retry", {"sync_fn": {"__callable__": "op"}, "op_name": "messages.create"},
    io={"asyncio.to_thread": [rs("HttpError", "b"), rs("HttpError", "b"), rs("HttpError", "b")],
        "_is_retryable_error": True, "random.random": [0.5, 0.25], "_exc_text": ["x", "x"],
        "logger.warning": [None, None], "asyncio.sleep": [None, None]})
add("_call_with_retry", {"sync_fn": {"__callable__": "op"}, "op_name": "messages.create"},
    io={"asyncio.to_thread": rs("SystemExit", "bye")})
add("_call_with_retry", {"sync_fn": {"__callable__": "op"}, "op_name": "messages.create"}, fc=True)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "  <-- <5!" if counts[fn] < 5 else ""
        print(f"  {fn}: {counts[fn]}{flag}")


if __name__ == "__main__":
    main()