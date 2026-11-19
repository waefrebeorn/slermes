#!/usr/bin/env python3
"""Generate tests/fixtures/gateway_session_flow.jsonl for the
gateway/session.py FLOW oracle. >=5 cases per ported function (22 funcs).

Each case is {"func", "in": {"args", "state", "io"}}. io scripts the universal
seam (a plain value reused, a list consumed per call, or {"__script__": true,
"ret"/"raise"}). The oracle re-drives the LIVE reference from "in"; the C
harness drives the port from the same "in".

Domain notes:
- ASCII fixture corpus; datetime fields are canonical round-tripping ISO strings.
- SessionSource/SessionEntry/_RouteDecision dicts may be PARTIAL (dataclass
  defaults are filled identically on both sides).
- _entry_locked scripts gate null/raise; the returned object is the STORED
  _entries[key] double (live identity), so the script need only carry the fields
  the trace records (the stored entry is authoritative for logic/state).
- Scripted raise kinds are plain-message classes (KeyError excluded).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "gateway_session_flow.jsonl")

NOW = "2026-11-18T12:00:00+00:00"
T0 = "2026-11-18T10:00:00+00:00"


def src(platform="telegram", chat_id="c1", **kw):
    d = {"platform": platform, "chat_id": chat_id}
    d.update(kw)
    return d


def ent(key="agent:main:telegram:c1", sid="s1", **kw):
    d = {"session_key": key, "session_id": sid, "created_at": T0, "updated_at": T0}
    d.update(kw)
    return d


def dec(**kw):
    return dict(kw)


def rs(type_, message=""):
    return {"__script__": True, "raise": {"type": type_, "message": message}}


CASES = []


def add(func, args, io=None, state=None):
    CASES.append({"func": func, "in": {"args": args, "state": state or {}, "io": io or {}}})


def store_state(entries=None, **kw):
    st = {"_entries": entries or {}, "_rows_pending": [], "_session_owner_hints": {},
          "_has_active_processes_fn": None}
    st.update(kw)
    return st


# ── _is_path_unsafe ──
add("_is_path_unsafe", {"value": "../x", "strict": True})
add("_is_path_unsafe", {"value": "a/b", "strict": True})
add("_is_path_unsafe", {"value": "a/b", "strict": False})
add("_is_path_unsafe", {"value": "/lead", "strict": False})
add("_is_path_unsafe", {"value": "C:foo", "strict": True})
add("_is_path_unsafe", {"value": "", "strict": True})
add("_is_path_unsafe", {"value": None, "strict": True})
add("_is_path_unsafe", {"value": "safe", "strict": False})
add("_is_path_unsafe", {"value": "a\\b", "strict": True})
add("_is_path_unsafe", {"value": ".hidden", "strict": False})

# ── _session_key_namespace ──
add("_session_key_namespace", {"profile": None})
add("_session_key_namespace", {"profile": ""})
add("_session_key_namespace", {"profile": "default"})
add("_session_key_namespace", {"profile": "main"})
add("_session_key_namespace", {"profile": "work"})

# ── _describe ──
add("_describe", {"chat_type": "dm", "user_label": "Alice", "chat_label": "c"})
add("_describe", {"chat_type": "group", "user_label": "u", "chat_label": "General"})
add("_describe", {"chat_type": "channel", "user_label": "u", "chat_label": "News"})
add("_describe", {"chat_type": "thread", "user_label": "u", "chat_label": "T"})
add("_describe", {"chat_type": "dm", "user_label": "", "chat_label": "c"})

# ── _canonical_participant ──
add("_canonical_participant", {"source": src(user_id="u1")})
add("_canonical_participant", {"source": src(platform="whatsapp", user_id="123@s.whatsapp.net")},
    {"canonical_whatsapp_identifier": "12345"})
add("_canonical_participant", {"source": src(platform="whatsapp", user_id_alt="alt", user_id="u")},
    {"canonical_whatsapp_identifier": None})
add("_canonical_participant", {"source": src(platform="whatsapp", user_id="u")},
    {"canonical_whatsapp_identifier": ""})
add("_canonical_participant", {"source": src()})
add("_canonical_participant", {"source": src(platform="whatsapp", user_id="u")},
    {"canonical_whatsapp_identifier": rs("RuntimeError", "boom")})
add("_canonical_participant", {"source": src(platform="whatsapp", user_id="u")},
    {"canonical_whatsapp_identifier": rs("SystemExit", "x")})

# ── _should_redact_pii ──
add("_should_redact_pii", {"platform": "discord", "enabled": False})
add("_should_redact_pii", {"platform": "telegram", "enabled": True})
add("_should_redact_pii", {"platform": "discord", "enabled": True},
    {"platform_registry.get": {"pii_safe": True}})
add("_should_redact_pii", {"platform": "discord", "enabled": True},
    {"platform_registry.get": {"pii_safe": False}})
add("_should_redact_pii", {"platform": "discord", "enabled": True},
    {"platform_registry.get": None})
add("_should_redact_pii", {"platform": "discord", "enabled": True},
    {"platform_registry.get": {}})
add("_should_redact_pii", {"platform": "discord", "enabled": True}, {})
add("_should_redact_pii", {"platform": "discord", "enabled": True},
    {"platform_registry.get": rs("SystemExit", "x")})

# ── label_shared_sender ──
add("label_shared_sender", {"source": src(user_name=None), "text": "hi"})
add("label_shared_sender", {"source": src(user_name="Alice"), "text": "hi"},
    {"is_shared_multi_user_session": False})
add("label_shared_sender", {"source": src(user_name="Alice"), "text": "hi"},
    {"is_shared_multi_user_session": True, "tag_safe_display_name": "Alice"})
add("label_shared_sender", {"source": src(platform="slack", user_name="Alice", user_id="U1"), "text": "hi"},
    {"is_shared_multi_user_session": True, "tag_safe_display_name": "Alice"})
add("label_shared_sender", {"source": src(user_name="Alice"), "text": "hi"},
    {"is_shared_multi_user_session": rs("RuntimeError", "boom")})
add("label_shared_sender", {"source": src(user_name="Alice"), "text": "hi"},
    {"is_shared_multi_user_session": True, "tag_safe_display_name": rs("SystemExit", "x")})
add("label_shared_sender", {"source": src(platform="slack", user_name="Alice", user_id=None), "text": "hi"},
    {"is_shared_multi_user_session": True, "tag_safe_display_name": "Alice"})

# ── _slack_platform_notes ──
CTX = {"shared_multi_user_session": False, "source": src(platform="slack")}
CTXS = {"shared_multi_user_session": True, "source": src(platform="slack")}
add("_slack_platform_notes", {"context": CTX}, {"_slack_tools_loaded": True})
add("_slack_platform_notes", {"context": CTX}, {"_slack_tools_loaded": False})
add("_slack_platform_notes", {"context": CTXS}, {"_slack_tools_loaded": True})
add("_slack_platform_notes", {"context": CTXS}, {"_slack_tools_loaded": False})
add("_slack_platform_notes", {"context": CTX}, {"_slack_tools_loaded": rs("SystemExit", "x")})
add("_slack_platform_notes", {"context": CTX}, {})

# ── _discord_platform_notes ──
def dctx(**skw):
    return {"source": src(platform="discord", **skw)}


add("_discord_platform_notes", {"context": dctx(guild_id="g1")}, {"_discord_tools_loaded": True})
add("_discord_platform_notes", {"context": dctx(guild_id="g1", thread_id="t1", parent_chat_id="p1")},
    {"_discord_tools_loaded": True})
add("_discord_platform_notes", {"context": dctx()}, {"_discord_tools_loaded": True})
add("_discord_platform_notes", {"context": dctx()}, {"_discord_tools_loaded": False})
add("_discord_platform_notes", {"context": dctx()}, {"_discord_tools_loaded": rs("SystemExit", "x")})
add("_discord_platform_notes", {"context": dctx(thread_id="t1")}, {"_discord_tools_loaded": True})

# ── schedule_reset ──
add("schedule_reset", {"reason": "expiry", "ended": ent(sid="s1"), "had_activity": True},
    state={"decision": dec()})
add("schedule_reset", {"reason": "expiry", "ended": ent(sid="s1"), "had_activity": False},
    state={"decision": dec()})
add("schedule_reset", {"reason": "", "ended": ent(sid="s2"), "had_activity": True},
    state={"decision": dec()})
add("schedule_reset", {"reason": "manual", "ended": ent(sid="s9"), "had_activity": False},
    state={"decision": dec()})
add("schedule_reset", {"reason": "expiry", "ended": ent(sid="s1"), "had_activity": True},
    state={"decision": dec(needs_save=True, reset_reason="old")})

# ── _lazy ──
add("_lazy", {"name": "_inflight_lock"}, {"factory": {"lock": 1}})
add("_lazy", {"name": "_inflight_lock"}, {}, state={"_inflight_lock": {"x": 1}})
add("_lazy", {"name": "_inflight_lock"}, {"factory": {"lock": 2}}, state={"_inflight_lock": None})
add("_lazy", {"name": "_inflight_lock"}, {"factory": rs("RuntimeError", "boom")})
add("_lazy", {"name": "_inflight_lock"}, {"factory": rs("SystemExit", "x")})
add("_lazy", {"name": "_inflight_sessions"}, {"factory": None})

# ── _has_active_processes_safe ──
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"},
    state=store_state())
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"},
    {"_has_active_processes_fn": True}, state=store_state(_has_active_processes_fn={"fn": 1}))
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"},
    {"_has_active_processes_fn": False}, state=store_state(_has_active_processes_fn={"fn": 1}))
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"},
    {"_has_active_processes_fn": rs("RuntimeError", "boom"), "logger.warning": None},
    state=store_state(_has_active_processes_fn={"fn": 1}))
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"},
    {"_has_active_processes_fn": rs("SystemExit", "x")},
    state=store_state(_has_active_processes_fn={"fn": 1}))
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"}, {},
    state={"_entries": {}})
add("_has_active_processes_safe", {"session_key": "k1", "context": "probe"},
    {"logger.warning": None}, state=store_state(_has_active_processes_fn={"fn": 1}))

# ── lookup_by_session_key ──
E1 = ent(sid="s1")
add("lookup_by_session_key", {"session_key": ""}, state=store_state())
add("lookup_by_session_key", {"session_key": "agent:main:telegram:c1"},
    {"_entry_locked": {"session_id": "s1"}}, state=store_state({"agent:main:telegram:c1": E1}))
add("lookup_by_session_key", {"session_key": "absent"},
    {"_entry_locked": None}, state=store_state())
add("lookup_by_session_key", {"session_key": "k"},
    {"_entry_locked": rs("RuntimeError", "boom")}, state=store_state())
add("lookup_by_session_key", {"session_key": "k"},
    {"_entry_locked": rs("SystemExit", "x")}, state=store_state())
add("lookup_by_session_key", {"session_key": "agent:main:telegram:c1"},
    {"_entry_locked": {"session_id": "s1", "display_name": "D"}},
    state=store_state({"agent:main:telegram:c1": ent(sid="s1", display_name="D")}))

# ── set_session_yolo ──
K = "agent:main:telegram:c1"
add("set_session_yolo", {"session_key": K, "enabled": True},
    {"_entry_locked": {"session_id": "s1"}, "_save": None},
    state=store_state({K: ent(sid="s1", yolo=None)}))
add("set_session_yolo", {"session_key": K, "enabled": True},
    {"_entry_locked": {"session_id": "s1"}},
    state=store_state({K: ent(sid="s1", yolo=True)}))
add("set_session_yolo", {"session_key": K, "enabled": None},
    {"_entry_locked": {"session_id": "s1"}},
    state=store_state({K: ent(sid="s1", yolo=None)}))
add("set_session_yolo", {"session_key": K, "enabled": False},
    {"_entry_locked": {"session_id": "s1"}, "_save": None},
    state=store_state({K: ent(sid="s1", yolo=True)}))
add("set_session_yolo", {"session_key": K, "enabled": True},
    {"_entry_locked": None}, state=store_state())
add("set_session_yolo", {"session_key": K, "enabled": True},
    {"_entry_locked": {"session_id": "s1"}, "_save": rs("RuntimeError", "disk")},
    state=store_state({K: ent(sid="s1", yolo=None)}))
add("set_session_yolo", {"session_key": K, "enabled": True},
    {"_entry_locked": rs("SystemExit", "x")}, state=store_state({K: ent(sid="s1")}))

# ── release_route ──
add("release_route", {"session_key": K, "session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": None},
    state=store_state({K: ent(sid="s1")}))
add("release_route", {"session_key": K, "session_id": "other"},
    {"_entry_locked": {"session_id": "s1"}},
    state=store_state({K: ent(sid="s1")}))
add("release_route", {"session_key": K, "session_id": "s1"},
    {"_entry_locked": None}, state=store_state())
add("release_route", {"session_key": K, "session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": rs("RuntimeError", "disk")},
    state=store_state({K: ent(sid="s1")}))
add("release_route", {"session_key": K, "session_id": "s1"},
    {"_entry_locked": rs("SystemExit", "x")}, state=store_state({K: ent(sid="s1")}))
add("release_route", {"session_key": K, "session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": None},
    state=store_state({K: ent(sid="s1"), "agent:main:telegram:c2": ent(key="agent:main:telegram:c2", sid="s2")}))

# ── purge_profile_routing ──
W = {"agent:work:telegram:a": ent(key="agent:work:telegram:a", sid="a"),
     "agent:work:telegram:b": ent(key="agent:work:telegram:b", sid="b"),
     "agent:main:telegram:c": ent(key="agent:main:telegram:c", sid="c")}
add("purge_profile_routing", {"profile": "work"}, {"_save": None}, state=store_state(dict(W)))
add("purge_profile_routing", {"profile": ""}, state=store_state(dict(W)))
add("purge_profile_routing", {"profile": "   "}, state=store_state(dict(W)))
add("purge_profile_routing", {"profile": "none"}, state=store_state(dict(W)))
add("purge_profile_routing", {"profile": "work"}, {"_save": rs("RuntimeError", "disk")},
    state=store_state(dict(W)))
add("purge_profile_routing", {"profile": "work"}, {"_save": None},
    state=store_state({"agent:work:telegram:a": ent(key="agent:work:telegram:a", sid="a")}))

# ── rekey_profile_routing ──
R = {"agent:work:telegram:a": ent(key="agent:work:telegram:a", sid="a",
                                  origin=src(platform="telegram", chat_id="a", profile="work")),
     "agent:main:telegram:c": ent(key="agent:main:telegram:c", sid="c")}
add("rekey_profile_routing", {"old_name": "work", "new_name": "home"}, {"_save": None},
    state=store_state(dict(R)))
add("rekey_profile_routing", {"old_name": "work", "new_name": "work"}, state=store_state(dict(R)))
add("rekey_profile_routing", {"old_name": "", "new_name": "home"}, state=store_state(dict(R)))
add("rekey_profile_routing", {"old_name": "work", "new_name": "home"}, {},
    state=store_state({"agent:work:telegram:a": ent(key="agent:work:telegram:a", sid="a"),
                       "agent:home:telegram:a": ent(key="agent:home:telegram:a", sid="x")}))
add("rekey_profile_routing", {"old_name": "none", "new_name": "home"}, state=store_state(dict(R)))
add("rekey_profile_routing", {"old_name": "work", "new_name": "home"}, {"_save": None},
    state=store_state({"agent:work:telegram:a": ent(key="agent:work:telegram:a", sid="a",
                                                    origin=src(platform="telegram", chat_id="a", profile="other"))}))
add("rekey_profile_routing", {"old_name": "work", "new_name": "home"},
    {"_save": rs("RuntimeError", "disk")}, state=store_state(dict(R)))

# ── remove_by_session_id ──
M = {"k1": ent(key="k1", sid="s1"), "k2": ent(key="k2", sid="s2")}
add("remove_by_session_id", {"session_id": "s1"},
    {"_ensure_loaded_locked": None, "_save": None, "logger.info": None},
    state=store_state(dict(M), _session_owner_hints={"s1": "k1", "s2": "k2"}))
add("remove_by_session_id", {"session_id": ""}, state=store_state(dict(M)))
add("remove_by_session_id", {"session_id": "none"},
    {"_ensure_loaded_locked": None}, state=store_state(dict(M)))
add("remove_by_session_id", {"session_id": "s1"},
    {"_ensure_loaded_locked": None, "_save": None, "logger.info": None},
    state=store_state({"k1": ent(key="k1", sid="s1"), "k2": ent(key="k2", sid="s1")}))
add("remove_by_session_id", {"session_id": "s1"},
    {"_ensure_loaded_locked": rs("SystemExit", "x")}, state=store_state(dict(M)))
add("remove_by_session_id", {"session_id": "s1"},
    {"_ensure_loaded_locked": None, "_save": rs("RuntimeError", "disk")},
    state=store_state(dict(M)))
add("remove_by_session_id", {"session_id": "s1"},
    {"_ensure_loaded_locked": None, "_save": None, "logger.info": None},
    state=store_state(dict(M)))

# ── restore_route ──
RE = ent(key=K, sid="s1", display_name="Restored")
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": None, "_persistence_for_key": True,
     "_reopen_session_row": None, "_announce_route": None},
    state=store_state({K: ent(sid="s1")}))
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": None}, state=store_state())
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": {"session_id": "other"}}, state=store_state({K: ent(sid="other")}))
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": None, "_persistence_for_key": False,
     "_announce_route": None}, state=store_state({K: ent(sid="s1")}))
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": rs("RuntimeError", "disk")},
    state=store_state({K: ent(sid="s1")}))
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": None, "_persistence_for_key": True,
     "_reopen_session_row": rs("SystemExit", "x")}, state=store_state({K: ent(sid="s1")}))
add("restore_route", {"entry": RE, "expected_session_id": "s1"},
    {"_entry_locked": {"session_id": "s1"}, "_save": None, "_persistence_for_key": True,
     "_reopen_session_row": None, "_announce_route": rs("RuntimeError", "boom")},
    state=store_state({K: ent(sid="s1")}))

# ── _replace_route_locked ──
OLD = ent(sid="old", origin=src(platform="telegram", chat_id="c1"), platform="telegram",
          chat_type="group", transport_profile="default")
add("_replace_route_locked",
    {"session_key": K, "old_entry": OLD, "session_id": "new", "now": NOW,
     "fields": {"display_name": "New", "is_fresh_reset": True}},
    {"_save": None}, state=store_state({K: OLD}))
add("_replace_route_locked",
    {"session_key": K, "old_entry": OLD, "session_id": "new", "now": NOW, "fields": {}},
    {"_save": None}, state=store_state({K: OLD}))
add("_replace_route_locked",
    {"session_key": K, "old_entry": OLD, "session_id": "new", "now": NOW,
     "fields": {"bogus": 1}}, {}, state=store_state({K: OLD}))
add("_replace_route_locked",
    {"session_key": K, "old_entry": OLD, "session_id": "new", "now": NOW,
     "fields": {"session_key": "x"}}, {}, state=store_state({K: OLD}))
add("_replace_route_locked",
    {"session_key": K, "old_entry": OLD, "session_id": "new", "now": NOW,
     "fields": {"display_name": "N"}}, {"_save": rs("RuntimeError", "disk")},
    state=store_state({K: OLD}))
add("_replace_route_locked",
    {"session_key": K, "old_entry": OLD, "session_id": "new", "now": NOW,
     "fields": {"yolo": True, "model_override": {"model": "m"}}},
    {"_save": None}, state=store_state({K: OLD}))

# ── _route_recover ──
REC = ent(key=K, sid="rec")
add("_route_recover", {"session_key": K, "source": src(), "now": NOW},
    {"_query_recoverable_session": REC, "_reopen_session_row": None},
    state=store_state(decision=dec()))
add("_route_recover", {"session_key": K, "source": src(), "now": NOW},
    {"_query_recoverable_session": None}, state=store_state(decision=dec()))
add("_route_recover", {"session_key": K, "source": src(), "now": NOW},
    {"_query_recoverable_session": REC, "_reopen_session_row": None},
    state=store_state({K: ent(sid="existing")}, decision=dec()))
add("_route_recover", {"session_key": K, "source": src(), "now": NOW},
    {"_query_recoverable_session": rs("SystemExit", "x")}, state=store_state(decision=dec()))
add("_route_recover", {"session_key": K, "source": src(), "now": NOW},
    {"_query_recoverable_session": REC, "_reopen_session_row": rs("RuntimeError", "boom")},
    state=store_state(decision=dec()))
add("_route_recover", {"session_key": K, "source": src(), "now": NOW},
    {"_query_recoverable_session": REC, "_reopen_session_row": None},
    state=store_state(decision=dec(needs_save=False, reset_reason="r")))

# ── _route_create ──
CK = {"session_id": "new", "source": "telegram"}
add("_route_create", {"session_key": K, "source": src(chat_name="Chat"), "now": NOW,
                      "force_new": False, "observed": None, "observed_same": False},
    {"_new_session_id": "new", "transport_profile_of": "default", "_session_create_kwargs": CK},
    state=store_state(decision=dec()))
add("_route_create", {"session_key": K, "source": src(), "now": NOW,
                      "force_new": False, "observed": ent(sid="cur"), "observed_same": True},
    {"_new_session_id": "new", "transport_profile_of": "default", "_session_create_kwargs": CK},
    state=store_state({K: ent(sid="cur")}, decision=dec()))
add("_route_create", {"session_key": K, "source": src(), "now": NOW,
                      "force_new": True, "observed": ent(sid="cur"), "observed_same": True},
    {"_new_session_id": "new", "transport_profile_of": "default", "_session_create_kwargs": CK},
    state=store_state({K: ent(sid="cur")}, decision=dec()))
add("_route_create", {"session_key": K, "source": src(), "now": NOW,
                      "force_new": True, "observed": ent(sid="cur"), "observed_same": False},
    {"_new_session_id": "new", "transport_profile_of": "default", "_session_create_kwargs": CK},
    state=store_state({K: ent(sid="cur")}, decision=dec()))
add("_route_create", {"session_key": K, "source": src(), "now": NOW,
                      "force_new": False, "observed": None, "observed_same": False},
    {"_new_session_id": rs("SystemExit", "x")}, state=store_state(decision=dec()))
add("_route_create", {"session_key": K, "source": src(), "now": NOW,
                      "force_new": False, "observed": None, "observed_same": False},
    {"_new_session_id": "new", "transport_profile_of": rs("RuntimeError", "boom")},
    state=store_state(decision=dec()))
add("_route_create", {"session_key": K, "source": src(), "now": NOW,
                      "force_new": False, "observed": None, "observed_same": False},
    {"_new_session_id": "new", "transport_profile_of": "default", "_session_create_kwargs": CK},
    state=store_state(decision=dec(reset_reason="expiry", reset_had_activity=True, prev_session_id="prev")))

# ── _apply_route_checks ──
AE = ent(sid="s1", last_prompt_tokens=5)
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": True,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None}, state=store_state({K: AE}))
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None}, state=store_state())
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": False,
     "transport_profile_of": "default"}, state=store_state({K: ent(sid="s1")}))
add("_apply_route_checks", {"session_key": K, "checks": {"session_id": "s1", "canonical_id": "s1",
                            "is_stale": True, "reset_reason": None}, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": False, "logger.warning": None},
    state=store_state({K: AE}))
add("_apply_route_checks", {"session_key": K, "checks": {"session_id": "s1", "canonical_id": "s1",
                            "is_stale": False, "reset_reason": "suspended"}, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": False},
    state=store_state({K: AE}))
add("_apply_route_checks", {"session_key": K, "checks": {"session_id": "other", "canonical_id": None,
                            "is_stale": True, "reset_reason": "x"}, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": False,
     "transport_profile_of": "default"}, state=store_state({K: ent(sid="s1")}))
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": False,
                            "touch_activity": False, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": False},
    state=store_state({K: ent(sid="s1")}))
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": False,
                            "touch_activity": False, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": True},
    state=store_state({K: ent(sid="s1")}))
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": rs("SystemExit", "x")},
    state=store_state({K: ent(sid="s1")}))
add("_apply_route_checks", {"session_key": K, "checks": None, "force_new": False,
                            "touch_activity": True, "now": NOW, "source": src()},
    {"_ensure_loaded_locked": None, "_heal_compression_tip_locked": False,
     "transport_profile_of": rs("RuntimeError", "boom")},
    state=store_state({K: ent(sid="s1")}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    if short:
        print(f"WARNING funcs with <5 cases: {short}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()