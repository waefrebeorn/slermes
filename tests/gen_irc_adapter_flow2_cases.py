#!/usr/bin/env python3
"""Generate tests/fixtures/irc_adapter_flow2.jsonl for the IRC adapter FLOW
remainder port. Deterministic; >=5 cases per ported function. Honours the corpus
restrictions documented in src/gateway/platforms/irc_adapter_flow2.c (ASCII
inputs, str/int/bool/null f-string positions, clean int() strings, 2-array /
wrong-arity / non-iterable open_connection results, object registry results).

Run: python tests/gen_irc_adapter_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "irc_adapter_flow2.jsonl")
CASES = []


def case(func, args, state=None, io=None):
    inp = {"args": args}
    if state is not None:
        inp["state"] = state
    if io is not None:
        inp["io"] = io
    CASES.append({"func": func, "in": inp})


def rs(kind, msg=""):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def sc(ret):
    return {"__script__": True, "ret": ret}


WR = {"__writer__": True}
RD = {"__reader__": True}
EV = {"__event__": True}
LP = {"__loop__": True}
REG = {"__registry__": True}
CTX = {"__ctx__": True}
TASK = {"__task__": True}
SSL = {"__ssl_ctx__": True}
ERR = {"success": False, "error": "x"}

# ── __init__ (IRCAdapter) ──
# _env_or_extra call order: server, port, nickname, channel, server_password, nickserv_password
INIT_IO = {
    "Platform": "irc", "base.__init__": None,
    "_env_or_extra": [None, None, None, None, None, None],
    "coerce_port": 6697, "_get_scoped_secret": None, "asyncio.Event": EV,
}
case("__init__", {"config": {"extra": {"server": "s", "channel": "#c", "port": 6697,
                                       "nickname": "bot", "use_tls": False,
                                       "server_password": "sp", "nickserv_password": "np",
                                       "allowed_users": ["Alice", "BOB", "alice", 5],
                                       "max_message_length": 450}}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {}}}, state={},
     io=dict(INIT_IO, _env_or_extra=["envs", "6698", "envnick", "envc", "envsp", "envnp"],
             coerce_port=6698, _get_scoped_secret="TRUE",
             import_platform_registry=REG, **{"platform_registry.get": {"max_message_length": 432}}))
case("__init__", {"config": {"extra": {}}}, state={},
     io=dict(INIT_IO, _get_scoped_secret="", import_platform_registry=rs("ImportError", "no")))
case("__init__", {"config": {"extra": {"max_message_length": 0}}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {"max_message_length": "abc"}}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {"max_message_length": " 12 "}}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {"allowed_users": None}}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {"allowed_users": 5}}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {}}}, state={}, io=dict(INIT_IO, _get_scoped_secret=5))
case("__init__", {"config": {"extra": {}}}, state={},
     io=dict(INIT_IO, import_platform_registry=rs("SystemExit", "bye")))
case("__init__", {"config": {"extra": {}}}, state={},
     io=dict(INIT_IO, import_platform_registry=REG, **{"platform_registry.get": None}))
case("__init__", {"config": {"extra": {}}}, state={},
     io=dict(INIT_IO, import_platform_registry=REG, **{"platform_registry.get": {}}))
case("__init__", {"config": {"extra": 5}}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {}}}, state={}, io=dict(INIT_IO, Platform=rs("RuntimeError", "p")))
case("__init__", {"config": {"extra": {}}}, state={}, io=dict(INIT_IO, **{"base.__init__": rs("OSError", "b")}))
case("__init__", {"config": {"extra": {}}}, state={}, io=dict(INIT_IO, **{"asyncio.Event": rs("RuntimeError", "e")}))
case("__init__", {"config": None}, state={}, io=dict(INIT_IO))
case("__init__", {"config": {"extra": {"allowed_users": ["x", "X", "y"], "use_tls": True}}},
     state={}, io=dict(INIT_IO))

# ── connect ──
CS = {"server": "s", "channel": "#c", "nickname": "bot", "port": 6697, "use_tls": True,
      "server_password": "", "nickserv_password": "", "_current_nick": "bot",
      "_reader": None, "_writer": None, "_recv_task": None, "_registered": False,
      "_registration_event": EV}
CONN_OK = {"_acquire_platform_lock": True, "_ssl_ctx": SSL,
           "asyncio.open_connection": [[RD, WR]], "_send_raw": None,
           "asyncio.create_task": TASK, "registration_event.wait": None,
           "_mark_connected": None, "logger.info": None, "_wire_plugin_handlers": None}
DISC_IO = {"_release_platform_lock": None, "_mark_disconnected": None,
           "writer.is_closing": False, "t": "bye", "_send_raw": None,
           "asyncio.sleep": None, "writer.close": None, "writer.wait_closed": None,
           "cancel_task": None, "registration_event.clear": None}
case("connect", {}, state=dict(CS), io=dict(CONN_OK))
case("connect", {}, state=dict(CS, server=""), io={"logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS, channel=""), io={"logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS), io={"_acquire_platform_lock": False})
case("connect", {}, state=dict(CS),
     io={"_acquire_platform_lock": True, "_ssl_ctx": SSL,
         "asyncio.open_connection": rs("OSError", "net"), "logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS),
     io={"_acquire_platform_lock": True, "_ssl_ctx": SSL,
         "asyncio.open_connection": [[RD]], "logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS),
     io={"_acquire_platform_lock": True, "_ssl_ctx": SSL,
         "asyncio.open_connection": None, "logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS), io=dict(CONN_OK, **{"asyncio.open_connection": "ab"}))
case("connect", {}, state=dict(CS),
     io={**CONN_OK, **DISC_IO, "registration_event.wait": rs("TimeoutError", ""),
         "logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS, _registration_event=None),
     io={"_acquire_platform_lock": True, "_ssl_ctx": SSL,
         "asyncio.open_connection": [[RD, WR]], "_send_raw": None, "asyncio.create_task": TASK})
case("connect", {}, state=dict(CS, server_password="pw"), io=dict(CONN_OK))
case("connect", {}, state=dict(CS, nickserv_password="np"), io=dict(CONN_OK, **{"asyncio.sleep": None}))
case("connect", {}, state=dict(CS),
     io={"_acquire_platform_lock": True, "_ssl_ctx": rs("OSError", "ssl"),
         "logger.error": None, "_fail": False})
case("connect", {}, state=dict(CS),
     io={"_acquire_platform_lock": True, "_ssl_ctx": SSL,
         "asyncio.open_connection": rs("CancelledError", "")})
case("connect", {}, state={}, io={})
case("connect", {}, state=dict(CS), io=dict(CONN_OK, **{"logger.info": rs("RuntimeError", "log")}))
case("connect", {}, state=dict(CS, nickname=5), io=dict(CONN_OK))

# ── disconnect ──
DS = {"_writer": WR, "_reader": RD, "_recv_task": TASK, "_registered": True,
      "_registration_event": EV}
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO))
case("disconnect", {}, state=dict(DS, _writer=None),
     io={"_release_platform_lock": None, "_mark_disconnected": None,
         "cancel_task": None, "registration_event.clear": None})
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, **{"writer.is_closing": True}))
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, **{"_release_platform_lock": rs("OSError", "x")}))
case("disconnect", {}, state=dict(DS), io={"_release_platform_lock": rs("SystemExit", "")})
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, t=5))
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, _send_raw=rs("OSError", "x")))
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, **{"writer.close": rs("OSError", "x")}))
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, cancel_task=rs("RuntimeError", "x")))
case("disconnect", {}, state=dict(DS, _registration_event=None),
     io={"_release_platform_lock": None, "_mark_disconnected": None,
         "writer.is_closing": False, "t": "bye", "_send_raw": None, "asyncio.sleep": None,
         "writer.close": None, "writer.wait_closed": None, "cancel_task": None})
case("disconnect", {}, state={}, io={})
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, **{"registration_event.clear": rs("RuntimeError", "c")}))
case("disconnect", {}, state=dict(DS), io=dict(DISC_IO, **{"writer.wait_closed": rs("CancelledError", "")}))

# ── interactive_setup ──
SETUP_BASE = {"print_header": None, "print_info": None, "print": None,
              "print_warning": None, "print_success": None, "save_env_value": None,
              "declines_reconfigure": False}
case("interactive_setup", {}, io=dict(SETUP_BASE, get_env_value="s", declines_reconfigure=True))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", ""],
             prompt=["irc.example.com", "", "bot", "#c"],
             prompt_yes_no=[True, False, False, True]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=[""], prompt=[""], prompt_yes_no=[True]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", ""],
             prompt=["irc.example.com", "abc", "bot", "#c"],
             prompt_yes_no=[True, False, False, True]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", ""],
             prompt=["irc.example.com", " 6697 ", "bot", "#c"],
             prompt_yes_no=[True, False, False, True]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", "", ""],
             prompt=["irc.example.com", "", "bot", "#c", "a, b"],
             prompt_yes_no=[True, False, False, False]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", "", ""],
             prompt=["irc.example.com", "", "bot", "#c", ""],
             prompt_yes_no=[True, False, False, False]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", ""],
             prompt=["irc.example.com", "", "bot", "#c", "s"],
             prompt_yes_no=[True, True, False, True]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", ""],
             prompt=["irc.example.com", 5, "bot", "#c"],
             prompt_yes_no=[True, False, False, True]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", ""],
             prompt=["irc.example.com", [1], "bot", "#c"],
             prompt_yes_no=[True, False, False, True]))
case("interactive_setup", {}, io=dict(SETUP_BASE, get_env_value="s",
                                      declines_reconfigure=rs("SystemExit", "")))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "", "", "", "", ""],
             prompt=["irc.example.com", "", "bot", "#c", 5],
             prompt_yes_no=[True, False, False, False]))
case("interactive_setup", {},
     io=dict(SETUP_BASE, get_env_value=["", "6667", "6667", "", ""],
             prompt=["irc.example.com", "", "bot", "#c"],
             prompt_yes_no=[False, False, False, True]))

# ── info ──
case("info", {"lines": ["a", "b"]}, io={"print_info": None})
case("info", {"lines": []}, io={})
case("info", {"lines": ["x"]}, io={"print_info": None})
case("info", {"lines": ["one", "two", "three"]}, io={"print_info": None})
case("info", {"lines": [""]}, io={"print_info": None})
case("info", {"lines": ["a"]}, io={"print_info": rs("RuntimeError", "x")})
case("info", {"lines": ["a", "b"]}, io={"print_info": rs("SystemExit", "")})

# ── _required ──
REQ_ARGS = {"label": "L", "env": "IRC_SERVER", "default": "", "what": "Server"}
case("_required", dict(REQ_ARGS), io={"prompt": " val ", "save_env_value": None})
case("_required", dict(REQ_ARGS), io={"prompt": "", "print_warning": None})
case("_required", dict(REQ_ARGS), io={"prompt": None, "print_warning": None})
case("_required", dict(REQ_ARGS), io={"prompt": 5})
case("_required", dict(REQ_ARGS), io={"prompt": "v", "save_env_value": rs("OSError", "x")})
case("_required", dict(REQ_ARGS), io={"prompt": rs("SystemExit", "")})
case("_required", dict(REQ_ARGS, what=5), io={"prompt": "", "print_warning": None})
case("_required", dict(REQ_ARGS, default="dflt"), io={"prompt": "v", "save_env_value": None})

# ── _StandaloneConn.__init__ ──
case("_StandaloneConn.__init__", {"reader": RD, "writer": WR}, state={},
     io={"asyncio.get_running_loop": LP})
case("_StandaloneConn.__init__", {"reader": None, "writer": None}, state={},
     io={"asyncio.get_running_loop": None})
case("_StandaloneConn.__init__", {"reader": RD, "writer": WR}, state={},
     io={"asyncio.get_running_loop": rs("RuntimeError", "no loop")})
case("_StandaloneConn.__init__", {}, state={}, io={"asyncio.get_running_loop": LP})
case("_StandaloneConn.__init__", {"reader": RD, "writer": WR}, state={},
     io={"asyncio.get_running_loop": rs("SystemExit", "")})
case("_StandaloneConn.__init__", {"reader": RD, "writer": None}, state={},
     io={"asyncio.get_running_loop": LP})

# ── _on_registration ──
REG_ST = {"nick_base": "hermes-bot", "nick_attempts": 0, "standalone_nick": "hermes-bot-cron"}
case("_on_registration", {"cmd": "433", "conn": CTX}, state=dict(REG_ST), io={"conn.raw": None})
case("_on_registration", {"cmd": "432", "conn": CTX}, state=dict(REG_ST, nick_attempts=5),
     io={"_sa_error": ERR})
case("_on_registration", {"cmd": "464", "conn": CTX}, state=dict(REG_ST), io={"_sa_error": ERR})
case("_on_registration", {"cmd": "465", "conn": CTX}, state=dict(REG_ST), io={"_sa_error": ERR})
case("_on_registration", {"cmd": "001", "conn": CTX}, state=dict(REG_ST), io={})
case("_on_registration", {"cmd": "PING", "conn": CTX}, state=dict(REG_ST), io={})
case("_on_registration", {"cmd": "433", "conn": None}, state=dict(REG_ST), io={})
case("_on_registration", {"cmd": "433", "conn": CTX}, state=dict(REG_ST),
     io={"conn.raw": rs("OSError", "x")})
case("_on_registration", {"cmd": "433", "conn": CTX}, state=dict(REG_ST, nick_base="a" * 40),
     io={"conn.raw": None})
case("_on_registration", {"cmd": "464", "conn": CTX}, state=dict(REG_ST),
     io={"_sa_error": rs("SystemExit", "")})
case("_on_registration", {"cmd": "433", "conn": CTX}, state=dict(REG_ST, nick_attempts=4),
     io={"conn.raw": None})

# ── _on_join ──
JOIN_ST = {"target": "#c"}
case("_on_join", {"cmd": "473"}, state=dict(JOIN_ST), io={"_sa_error": ERR})
case("_on_join", {"cmd": "403"}, state=dict(JOIN_ST), io={"_sa_error": ERR})
case("_on_join", {"cmd": "366"}, state=dict(JOIN_ST), io={})
case("_on_join", {"cmd": "JOIN"}, state=dict(JOIN_ST), io={})
case("_on_join", {"cmd": "PING"}, state=dict(JOIN_ST), io={})
case("_on_join", {"cmd": "475"}, state=dict(JOIN_ST), io={"_sa_error": ERR})
case("_on_join", {"cmd": "471"}, state=dict(JOIN_ST), io={"_sa_error": rs("RuntimeError", "x")})
case("_on_join", {"cmd": "405"}, state=dict(JOIN_ST, target=5), io={"_sa_error": ERR})

# ── register ──
case("register", {"ctx": CTX}, io={"ctx.register_platform": None})
case("register", {"ctx": None}, io={})
case("register", {"ctx": CTX}, io={"ctx.register_platform": rs("RuntimeError", "x")})
case("register", {"ctx": CTX}, io={"ctx.register_platform": rs("SystemExit", "")})
case("register", {"ctx": CTX}, io={"ctx.register_platform": {"ok": True}})
case("register", {}, io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    short = [fn for fn, n in counts.items() if n < 5]
    if short:
        raise SystemExit(f"FUNCS WITH <5 CASES: {short}")


if __name__ == "__main__":
    main()