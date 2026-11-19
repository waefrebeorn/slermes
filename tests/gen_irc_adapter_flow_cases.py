#!/usr/bin/env python3
"""Generate tests/fixtures/irc_adapter_flow.jsonl for the IRC adapter FLOW
port. Deterministic; >=5 cases per ported function. Honours the corpus
restrictions documented in src/gateway/platforms/irc_adapter_flow.c (ASCII
scope, limit>=1 chunk splitter, clean int() port strings, str-or-null scoped
secrets, 2-array open_connection results, object send_error/seed results).

Run: python tests/gen_irc_adapter_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "irc_adapter_flow.jsonl")

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
LP = {"__loop__": True}
EV = {"__event__": True}
HANDLER = {"__handler__": True}
SSL = {"__ssl_ctx__": True}
ERR = {"success": False, "error": "x"}
TIMES = list(range(0, 60))


def wio(closing=False, write=None, drain=None):
    return {"writer.is_closing": closing, "writer.write": write, "writer.drain": drain}


# ── _parse_irc_message ──
for raw in [":nick!u@h PRIVMSG #c :hello world", "PING :tok", ":", "", ":x",
            "001 nick :Welcome", "PRIVMSG #c :", "NICK newnick",
            ":srv 433 * nick :in use", "  PING  :x  ", ":pfx CMD a b :trail :more",
            "CMD\t:a", ":onlyprefix"]:
    case("_parse_irc_message", {"raw": raw})

# ── _extract_nick ──
for p in ["nick!user@host", "nick", "", "!u@h", "nick!a!b", "NICK!x"]:
    case("_extract_nick", {"prefix": p})

# ── _ms_id ──
case("_ms_id", {}, io={"time.time": 1700000000.0})
case("_ms_id", {}, io={"time.time": 1700000000.123})
case("_ms_id", {}, io={"time.time": 0})
case("_ms_id", {}, io={"time.time": -5.5})
case("_ms_id", {}, io={"time.time": 1234567890})
case("_ms_id", {}, io={"time.time": rs("SystemExit", "bye")})
case("_ms_id", {}, io={})

# ── _env_or_extra ──
case("_env_or_extra", {"extra": {"server": "ex"}, "env": "IRC_SERVER", "key": "server", "default": ""},
     io={"_get_scoped_secret": "envval"})
case("_env_or_extra", {"extra": {"server": "ex"}, "env": "IRC_SERVER", "key": "server", "default": ""},
     io={"_get_scoped_secret": ""})
case("_env_or_extra", {"extra": {"server": "ex"}, "env": "IRC_SERVER", "key": "server", "default": ""},
     io={"_get_scoped_secret": None})
case("_env_or_extra", {"extra": {}, "env": "IRC_SERVER", "key": "server", "default": "dflt"},
     io={"_get_scoped_secret": None})
case("_env_or_extra", {"extra": {"server": "ex"}, "env": "IRC_SERVER", "key": "server", "default": ""},
     io={"_get_scoped_secret": rs("ValueError", "bad")})
case("_env_or_extra", {"extra": None, "env": "IRC_SERVER", "key": "server", "default": ""},
     io={"_get_scoped_secret": None})
case("_env_or_extra", {"extra": {"server": 0}, "env": "IRC_SERVER", "key": "server", "default": "d"},
     io={"_get_scoped_secret": None})

# ── _server_channel ──
case("_server_channel", {"config": {"extra": {"server": "s", "channel": "#c"}}},
     io={"_get_scoped_secret": [None, None]})
case("_server_channel", {"config": {"extra": {}}}, io={"_get_scoped_secret": ["envs", "envc"]})
case("_server_channel", {"config": None}, io={"_get_scoped_secret": ["s", "#c"]})
case("_server_channel", {"config": {}}, io={"_get_scoped_secret": [None, "#c"]})
case("_server_channel", {"config": {"extra": None}}, io={"_get_scoped_secret": ["s", None]})
case("_server_channel", {"config": {"extra": {}}}, io={"_get_scoped_secret": rs("OSError", "net")})

# ── _chunk_paragraph ──
for p, l in [("hello world foo", 8), ("abcdefghij", 3), ("a b c d e", 2), ("", 5),
             ("  leading", 4), ("x" * 20, 7), ("one two three four", 10), ("ab cd", 1),
             ("short", 100), ("trailing space   ", 6)]:
    case("_chunk_paragraph", {"paragraph": p, "limit": l})

# ── _privmsg_budget ──
for t in ["#c", "#channel", "nick", "", "#a-very-long-channel-name-here"]:
    case("_privmsg_budget", {"target": t})

# ── _split_lines ──
case("_split_lines", {"paragraphs": ["hello world", "foo"], "limit": 8})
case("_split_lines", {"paragraphs": [], "limit": 10})
case("_split_lines", {"paragraphs": ["abcdefghij"], "limit": 3})
case("_split_lines", {"paragraphs": ["", "x"], "limit": 5})
case("_split_lines", {"paragraphs": ["one two three"], "limit": 6})
case("_split_lines", {"paragraphs": ["a", "b", "c"], "limit": 1})

# ── _encode_line ──
for l in ["NICK bob", "", "PRIVMSG #c :hi", "QUIT :bye", "PONG :tok"]:
    case("_encode_line", {"line": l})

# ── _ssl_ctx ──
case("_ssl_ctx", {"use_tls": True}, io={"ssl.create_default_context": SSL})
case("_ssl_ctx", {"use_tls": False}, io={})
case("_ssl_ctx", {"use_tls": 1}, io={"ssl.create_default_context": SSL})
case("_ssl_ctx", {"use_tls": 0}, io={})
case("_ssl_ctx", {"use_tls": None}, io={})
case("_ssl_ctx", {"use_tls": "x"}, io={"ssl.create_default_context": SSL})
case("_ssl_ctx", {"use_tls": True}, io={"ssl.create_default_context": rs("OSError", "ssl")})

# ── check_requirements ──
case("check_requirements", {}, io={"_get_scoped_secret": ["s", "#c"]})
case("check_requirements", {}, io={"_get_scoped_secret": ["", "#c"]})
case("check_requirements", {}, io={"_get_scoped_secret": ["s", ""]})
case("check_requirements", {}, io={"_get_scoped_secret": [None, None]})
case("check_requirements", {}, io={"_get_scoped_secret": ["s", rs("SystemExit", "x")]})
case("check_requirements", {}, io={})

# ── validate_config ──
case("validate_config", {"config": {"extra": {}}}, io={"_get_scoped_secret": ["s", "#c"]})
case("validate_config", {"config": {"extra": {}}}, io={"_get_scoped_secret": ["", "#c"]})
case("validate_config", {"config": None}, io={"_get_scoped_secret": ["s", ""]})
case("validate_config", {"config": {"extra": {"server": "s", "channel": "#c"}}},
     io={"_get_scoped_secret": [None, None]})
case("validate_config", {"config": {}}, io={"_get_scoped_secret": [None, None]})

# ── is_connected ──
case("is_connected", {"config": {"extra": {}}}, io={"_get_scoped_secret": ["s", "#c"]})
case("is_connected", {"config": {"extra": {}}}, io={"_get_scoped_secret": [None, "#c"]})
case("is_connected", {"config": None}, io={"_get_scoped_secret": ["s", "#c"]})
case("is_connected", {"config": {}}, io={"_get_scoped_secret": ["", ""]})
case("is_connected", {"config": {"extra": {}}}, io={"_get_scoped_secret": rs("KeyboardInterrupt", "")})

# ── _env_enablement ──
case("_env_enablement", {}, io={"_get_scoped_secret": [" s ", " #c "],
                                "_seed_extra_from_env": {"port": 6697}})
case("_env_enablement", {}, io={"_get_scoped_secret": ["", "#c"]})
case("_env_enablement", {}, io={"_get_scoped_secret": ["s", ""]})
case("_env_enablement", {}, io={"_get_scoped_secret": [None, None]})
case("_env_enablement", {}, io={"_get_scoped_secret": ["s", "#c"], "_seed_extra_from_env": {}})
case("_env_enablement", {}, io={"_get_scoped_secret": ["s", "#c"], "_seed_extra_from_env": None})
case("_env_enablement", {}, io={"_get_scoped_secret": [5, "#c"]})
case("_env_enablement", {}, io={"_get_scoped_secret": ["s", "#c"],
                                "_seed_extra_from_env": rs("ValueError", "bad")})

# ── _strip_irc_control_chars ──
for t in ["hello\r\nworld", "a\x00b", "plain", "\r\n\x00", "line1\nline2\rline3", ""]:
    case("_strip_irc_control_chars", {"text": t})

# ── _is_irc_channel ──
for t in ["#c", "&c", "+c", "!c", "nick", "", None, 5, "#"]:
    case("_is_irc_channel", {"target": t})

# ── _sa_error ──
case("_sa_error", {"detail": "boom"}, io={"send_error": ERR})
case("_sa_error", {"detail": ""}, io={"send_error": ERR})
case("_sa_error", {"detail": "x"}, io={"send_error": {"success": False, "error": "custom"}})
case("_sa_error", {"detail": "y"}, io={"send_error": rs("RuntimeError", "se")})
case("_sa_error", {"detail": "z"}, io={"send_error": rs("SystemExit", "bye")})
case("_sa_error", {"detail": "w"}, io={})

# ── name ──
for _ in range(5):
    case("name", {})

# ── _fail ──
case("_fail", {"code": "config_missing", "message": "m", "retryable": False}, io={"_set_fatal_error": None})
case("_fail", {"code": "connect_failed", "message": "m", "retryable": True}, io={"_set_fatal_error": None})
case("_fail", {"code": "c", "message": "", "retryable": False}, io={"_set_fatal_error": None})
case("_fail", {"code": "c", "message": "m", "retryable": True}, io={"_set_fatal_error": rs("ValueError", "x")})
case("_fail", {"code": "c", "message": "m", "retryable": False}, io={"_set_fatal_error": rs("CancelledError", "")})
case("_fail", {"code": "c", "message": "m", "retryable": False}, io={})

# ── _split_message ──
case("_split_message", {"content": "hello", "target": "#c"}, state={"max_message_length": 450})
case("_split_message", {"content": "**bold** text", "target": "#c"}, state={"max_message_length": 450})
case("_split_message", {"content": "a\n\nb\n  \nc", "target": "#c"}, state={"max_message_length": 450})
case("_split_message", {"content": "x" * 600, "target": "#c"}, state={"max_message_length": 450})
case("_split_message", {"content": "", "target": "#c"}, state={"max_message_length": 450})
case("_split_message", {"content": "   ", "target": "#c"}, state={"max_message_length": 450})
case("_split_message", {"content": "word " * 30, "target": "#longchannel"}, state={"max_message_length": 20})
case("_split_message", {"content": "hi", "target": "#c"}, state={})

# ── _strip_markdown ──
for t in ["**bold** and *it*", "__u__ _v_ x_y_w", "`code` ```py\ncode\n```",
          "![img](http://i) [link](http://l)", "**a\nb**", "_a_b", "a * b * c",
          "```\nplain\n```", "***x***", "`a`b`c`", "plain text", "![alt](url)",
          "[text](http://x)", "****", "````", "_under_ word_"]:
    case("_strip_markdown", {"text": t})

# ── _send_raw ──
case("_send_raw", {"line": "NICK bob"}, state={"_writer": WR}, io=wio())
case("_send_raw", {"line": "QUIT :bye"}, state={"_writer": None}, io={})
case("_send_raw", {"line": "X"}, state={"_writer": WR}, io=wio(closing=True))
case("_send_raw", {"line": "X"}, state={}, io={})
case("_send_raw", {"line": "X"}, state={"_writer": WR}, io=wio(write=rs("OSError", "broken")))
case("_send_raw", {"line": "X"}, state={"_writer": WR}, io=wio(drain=rs("CancelledError", "")))
case("_send_raw", {"line": "X"}, state={"_writer": WR}, io={})

# ── _handle_line ──
HL_BASE = {"_current_nick": "hermes-bot", "nickname": "hermes-bot", "_registered": False,
           "_registration_event": EV, "_writer": WR, "_allowed_users_lower": [],
           "max_message_length": 450, "_message_handler": None}
case("_handle_line", {"raw": "PING :tok"}, state=dict(HL_BASE), io=wio())
case("_handle_line", {"raw": "PING"}, state=dict(HL_BASE), io=wio())
case("_handle_line", {"raw": ":srv 001 hermes-bot :Welcome"}, state=dict(HL_BASE),
     io={"registration_event.set": None})
case("_handle_line", {"raw": "001 newnick :Welcome"}, state=dict(HL_BASE),
     io={"registration_event.set": None})
case("_handle_line", {"raw": ":srv 433 * hermes-bot :in use"}, state=dict(HL_BASE), io=wio())
case("_handle_line", {"raw": ":srv 433 * hermes-bot_ :in use"},
     state=dict(HL_BASE, _current_nick="hermes-bot_"), io=wio())
case("_handle_line", {"raw": ":srv 433 * hermes-bot_3 :in use"},
     state=dict(HL_BASE, _current_nick="hermes-bot_3"), io=wio())
case("_handle_line", {"raw": ":hermes-bot!u@h NICK newnick"}, state=dict(HL_BASE), io={})
case("_handle_line", {"raw": ":other!u@h NICK newnick"}, state=dict(HL_BASE), io={})
case("_handle_line", {"raw": ":srv NOTICE * :hi"}, state=dict(HL_BASE), io={})
case("_handle_line", {"raw": ":nick!u@h PRIVMSG #c :hermes-bot: hello"},
     state=dict(HL_BASE, _message_handler=HANDLER),
     io={"build_source": {"chat_id": "#c"}, "uuid.uuid4": {"hex": "abc123"},
         "datetime.now": "TS", "handle_message": None})
case("_handle_line", {"raw": "PING :tok"}, state=dict(HL_BASE, _writer=None), io={})

# ── _handle_privmsg ──
HP_BASE = {"_current_nick": "hermes-bot", "_allowed_users_lower": [], "_message_handler": HANDLER}
DISP_IO = {"build_source": {"chat_id": "x"}, "uuid.uuid4": {"hex": "abc"},
           "datetime.now": "TS", "handle_message": None}
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "hermes-bot: hi"},
     state=dict(HP_BASE), io=DISP_IO)
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "hermes-bot, hi"},
     state=dict(HP_BASE), io=DISP_IO)
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "hermes-bot hi"},
     state=dict(HP_BASE), io=DISP_IO)
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "not addressed"},
     state=dict(HP_BASE), io={})
case("_handle_privmsg", {"sender_nick": "alice", "target": "hermes-bot", "text": "dm hi"},
     state=dict(HP_BASE), io=DISP_IO)
case("_handle_privmsg", {"sender_nick": "hermes-bot", "target": "#c", "text": "hermes-bot: hi"},
     state=dict(HP_BASE), io={})
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "\x01ACTION waves\x01"},
     state=dict(HP_BASE), io=DISP_IO)
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "\x01VERSION\x01"},
     state=dict(HP_BASE), io={})
case("_handle_privmsg", {"sender_nick": "bob", "target": "#c", "text": "hermes-bot: hi"},
     state=dict(HP_BASE, _allowed_users_lower=["alice"]), io={"logger.debug": None})
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "hermes-bot: hi"},
     state=dict(HP_BASE, _allowed_users_lower=["alice"]), io=DISP_IO)
case("_handle_privmsg", {"sender_nick": "alice", "target": "#c", "text": "HERMES-BOT: hi"},
     state=dict(HP_BASE), io=DISP_IO)

# ── _dispatch_message ──
DM_ARGS = {"text": "hi", "chat_id": "#c", "chat_type": "group", "user_id": "alice", "user_name": "alice"}
case("_dispatch_message", dict(DM_ARGS), state={"_message_handler": HANDLER}, io=DISP_IO)
case("_dispatch_message", dict(DM_ARGS), state={"_message_handler": None}, io={})
case("_dispatch_message", dict(DM_ARGS), state={}, io={})
case("_dispatch_message", dict(DM_ARGS), state={"_message_handler": HANDLER},
     io={"build_source": {"chat_id": "#c"}, "uuid.uuid4": None})
case("_dispatch_message", dict(DM_ARGS), state={"_message_handler": HANDLER},
     io={"build_source": rs("ValueError", "bs")})
case("_dispatch_message", dict(DM_ARGS), state={"_message_handler": HANDLER},
     io={"build_source": {"chat_id": "#c"}, "uuid.uuid4": {"hex": "h"},
         "datetime.now": rs("CancelledError", "")})
case("_dispatch_message", dict(DM_ARGS), state={"_message_handler": HANDLER},
     io={"build_source": {"chat_id": "#c"}, "uuid.uuid4": {"hex": "h"},
         "datetime.now": "TS", "handle_message": rs("SystemExit", "bye")})

# ── send ──
SEND_BASE = {"_writer": WR, "max_message_length": 450}
case("send", {"chat_id": "#c", "content": "hello"}, state=dict(SEND_BASE),
     io={**wio(), "asyncio.sleep": None, "time.time": 1700000000.0})
case("send", {"chat_id": "#c", "content": "hello"}, state={"_writer": None}, io={})
case("send", {"chat_id": "#c", "content": "hello"}, state={"_writer": WR}, io=wio(closing=True))
case("send", {"chat_id": "#c", "content": "a\nb"}, state=dict(SEND_BASE),
     io={**wio(), "asyncio.sleep": None, "time.time": 1700000000.0})
case("send", {"chat_id": "#c", "content": "hello"}, state=dict(SEND_BASE),
     io={**wio(write=rs("OSError", "broken pipe")), "asyncio.sleep": None})
case("send", {"chat_id": "#c", "content": "hello"}, state=dict(SEND_BASE),
     io={**wio(), "asyncio.sleep": rs("CancelledError", "")})
case("send", {"chat_id": "#c", "content": "hello"}, state={}, io={})
case("send", {"chat_id": "#c", "content": ""}, state=dict(SEND_BASE),
     io={**wio(), "asyncio.sleep": None, "time.time": 1700000000.0})

# ── send_typing ──
for _ in range(5):
    case("send_typing", {"chat_id": "#c", "metadata": None})

# ── get_chat_info ──
for cid in ["#c", "&c", "nick", "+c", ""]:
    case("get_chat_info", {"chat_id": cid})
case("get_chat_info", {"chat_id": None})

# ── _receive_loop ──
RL_BASE = {"_reader": RD, "is_connected": True, "_current_nick": "hermes-bot",
           "nickname": "hermes-bot", "_registered": False, "_registration_event": EV,
           "_writer": WR, "_allowed_users_lower": [], "max_message_length": 450,
           "_message_handler": None}
RL_FINAL = {"logger.warning": None, "_set_fatal_error": None, "_notify_fatal_error": None}
case("_receive_loop", {}, state=dict(RL_BASE),
     io={"reader.at_eof": [False, True], "reader.read": ["PING :tok\r\n"], **wio(), **RL_FINAL})
case("_receive_loop", {}, state=dict(RL_BASE),
     io={"reader.at_eof": [False, False, True], "reader.read": ["001 nick :Welcome\r\n", ""],
         "registration_event.set": None, **RL_FINAL})
case("_receive_loop", {}, state=dict(RL_BASE),
     io={"reader.at_eof": [False, True], "reader.read": ["partial"], **RL_FINAL})
case("_receive_loop", {}, state=dict(RL_BASE, is_connected=False),
     io={"reader.at_eof": [True]})
case("_receive_loop", {}, state=dict(RL_BASE),
     io={"reader.at_eof": [False, True], "reader.read": [rs("OSError", "reset")],
         "logger.error": None, **RL_FINAL})
case("_receive_loop", {}, state=dict(RL_BASE),
     io={"reader.at_eof": [False, True], "reader.read": [rs("CancelledError", "")], **RL_FINAL})
case("_receive_loop", {}, state={"_reader": None, "is_connected": False}, io={})
case("_receive_loop", {}, state=dict(RL_BASE),
     io={"reader.at_eof": [False, True],
         "reader.read": [":nick!u@h PRIVMSG #c :hermes-bot: hi\r\n"],
         "build_source": {"chat_id": "#c"}, "uuid.uuid4": {"hex": "h"},
         "datetime.now": "TS", "handle_message": rs("ValueError", "dispatch"),
         **RL_FINAL})

# ── raw ──
case("raw", {"line": "NICK x"}, state={"writer": WR}, io={"writer.write": None, "writer.drain": None})
case("raw", {"line": "QUIT :bye"}, state={"writer": None}, io={})
case("raw", {"line": "X"}, state={}, io={})
case("raw", {"line": "X"}, state={"writer": WR}, io={"writer.write": rs("OSError", "b"), "writer.drain": None})
case("raw", {"line": "X"}, state={"writer": WR}, io={"writer.write": None, "writer.drain": rs("CancelledError", "")})
case("raw", {"line": "X"}, state={"writer": WR}, io={})

# ── pump ──
PUMP_STATE = {"reader": RD, "writer": WR, "loop": LP}
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": TIMES, "reader.readuntil": ["001 nick :Welcome\r\n"], "on_msg": [True]})
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": TIMES, "reader.readuntil": ["PING :tok\r\n", "001 n :W\r\n", "001 n :W\r\n"],
         "writer.write": None, "writer.drain": None, "on_msg": [None, True]})
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": TIMES, "reader.readuntil": [rs("TimeoutError", "t")]})
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": TIMES, "reader.readuntil": [rs("IncompleteReadError", "eof")]})
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": [0, 20], "reader.readuntil": []})
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": TIMES, "reader.readuntil": ["CMD x\r\n"], "on_msg": [False]})
case("pump", {"timeout": 15.0}, state=dict(PUMP_STATE),
     io={"loop.time": TIMES, "reader.readuntil": ["CMD x\r\n"], "on_msg": [0]})
case("pump", {"timeout": 15.0}, state={"reader": RD, "loop": LP},
     io={"loop.time": TIMES, "reader.readuntil": [rs("ValueError", "other")]})
case("pump", {"timeout": 15.0}, state={"writer": WR, "loop": LP}, io={"loop.time": TIMES})

# ── close ──
case("close", {}, state={"writer": WR}, io={"writer.close": None, "writer.wait_closed": None})
case("close", {}, state={"writer": None}, io={})
case("close", {}, state={}, io={})
case("close", {}, state={"writer": WR}, io={"writer.close": rs("OSError", "x"), "writer.wait_closed": None})
case("close", {}, state={"writer": WR}, io={"writer.close": None, "writer.wait_closed": rs("TimeoutError", "t")})
case("close", {}, state={"writer": WR}, io={"writer.close": rs("CancelledError", "")})
case("close", {}, state={"writer": WR}, io={"writer.close": None, "writer.wait_closed": rs("SystemExit", "bye")})

# ── _sa_register ──
SA_STATE = {"reader": RD, "writer": WR, "loop": LP}
SA_WIO = {"writer.write": None, "writer.drain": None}
case("_sa_register", {"nick_base": "hermes-bot", "server_password": None}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["001 nick :Welcome\r\n"]})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": "secret"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["001 nick :Welcome\r\n"]})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": None}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["433 * nick :in use\r\n", "001 nick :W\r\n"]})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": None}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES,
         "reader.readuntil": ["433 * n :u\r\n"] * 6, "send_error": ERR})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": None}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": [rs("TimeoutError", "t")], "send_error": ERR})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": None}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": [rs("IncompleteReadError", "eof")], "send_error": ERR})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": None}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["464 * :rejected\r\n"], "send_error": ERR})
case("_sa_register", {"nick_base": "hermes-bot", "server_password": "p\r\nass"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["001 nick :W\r\n"]})

# ── _sa_join ──
case("_sa_join", {"target": "#c"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["366 nick #c :End\r\n"]})
case("_sa_join", {"target": "#c"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["JOIN #c\r\n"]})
case("_sa_join", {"target": "#c"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["473 * #c :invite\r\n"], "send_error": ERR})
case("_sa_join", {"target": "#c"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": [rs("TimeoutError", "t")]})
case("_sa_join", {"target": "#c"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": [rs("IncompleteReadError", "eof")]})
case("_sa_join", {"target": "#c"}, state=dict(SA_STATE),
     io={**SA_WIO, "loop.time": TIMES, "reader.readuntil": ["403 * #c :no\r\n"], "send_error": ERR})

# ── _standalone_send ──
def sa_io(server="srv", channel="#c", port=6697, use_tls=None, nickname="hermes-bot",
          server_password=None, nickserv=None, readuntil=None, **over):
    io = {
        "_get_scoped_secret": [server, channel, port, use_tls, nickname, server_password, nickserv],
        "ssl.create_default_context": SSL,
        "asyncio.open_connection": [[RD, WR]],
        "asyncio.get_running_loop": LP,
        "loop.time": TIMES,
        "reader.readuntil": readuntil if readuntil is not None else ["001 nick :Welcome\r\n", "366 nick #c :End\r\n"],
        "writer.write": None, "writer.drain": None, "writer.close": None, "writer.wait_closed": None,
        "asyncio.sleep": None, "t": "bye", "reader.read": "", "time.time": 1700000000.0,
        "send_error": ERR,
    }
    io.update(over)
    return io


SS_ARGS = {"pconfig": {"extra": {}}, "chat_id": "#c", "message": "hello"}
case("_standalone_send", dict(SS_ARGS), io=sa_io())
case("_standalone_send", dict(SS_ARGS), io=sa_io(server="", channel="#c"))
case("_standalone_send", dict(SS_ARGS), io=sa_io(port="notaport"))
case("_standalone_send", dict(SS_ARGS), io=sa_io(port=None))
case("_standalone_send", {"pconfig": {"extra": {}}, "chat_id": "#c\r\n", "message": "hi"}, io=sa_io())
case("_standalone_send", dict(SS_ARGS), io=sa_io(**{"asyncio.open_connection": rs("OSError", "refused")}))
case("_standalone_send", dict(SS_ARGS), io=sa_io(**{"asyncio.open_connection": None}))
case("_standalone_send", dict(SS_ARGS), io=sa_io(**{"asyncio.open_connection": [[RD]]}))
case("_standalone_send", {"pconfig": {"extra": {}}, "chat_id": "#c", "message": "  \n  "}, io=sa_io())
case("_standalone_send", dict(SS_ARGS), io=sa_io(nickserv="nssecret"))
case("_standalone_send", {"pconfig": {"extra": {}}, "chat_id": "nick", "message": "dm"}, io=sa_io())
case("_standalone_send", dict(SS_ARGS), io=sa_io(use_tls="false"))
case("_standalone_send", dict(SS_ARGS),
     io=sa_io(readuntil=["433 * n :u\r\n", "001 nick :W\r\n", "366 nick #c :End\r\n"]))
case("_standalone_send", dict(SS_ARGS),
     io=sa_io(**{"asyncio.open_connection": rs("CancelledError", "")}))
case("_standalone_send", dict(SS_ARGS), io=sa_io(nickname="bot_123-"))
case("_standalone_send", {"pconfig": {"extra": {"use_tls": False}}, "chat_id": "#c", "message": "hi"},
     io=sa_io())

# ── write ──
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {OUT}")
low = [f for f, n in counts.items() if n < 5]
print(f"funcs with <5 cases: {low}")
print(f"distinct funcs: {len(counts)}")
