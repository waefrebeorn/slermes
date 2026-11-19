#!/usr/bin/env python3
"""Generator for tests/fixtures/whatsapp_adapter_flow.jsonl.

Emits >=5 cases per ported function of the WhatsApp adapter bridge-lifecycle
FLOW port (src/gateway/platforms/whatsapp_adapter_flow.c). Cases carry ONLY
inputs + scripted seam results (in.io); expected values are computed by the
oracle from the LIVE quarry defs. Script semantics (shared with the C
harness): a bare JSON array is a per-call QUEUE (one result per call, last
value NOT repeated — exhaustion is a fail-closed RuntimeError); any other
value is returned verbatim on every call; {"__script__": true, "ret": v}
forces a single result; {"__script__": true, "raise": {type, message}}
raises. List-valued seam results are wrapped one level deeper ([["a"]] or
__script__) so the queue does not eat the list elements.

Cases marked "fail_closed": true intentionally leave a seam unscripted to
exercise the fail-closed RuntimeError parity (the oracle self-test allows
the script gap only on those).

Corpus restrictions honoured here (documented in the C port header):
ASCII str()-positions, int/float tokens printable-ASCII when invalid,
|values| < 2^53, POSIX lexical paths, cmdline elements str, no integral
floats in str()-computed positions.
"""
import json
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "whatsapp_adapter_flow.jsonl")

CASES = []


def add(func, args, io=None, fail_closed=False):
    rec = {"func": func, "in": {"args": args, "io": io or {}}}
    if fail_closed:
        rec["fail_closed"] = True
    CASES.append(rec)


def script(ret):
    return {"__script__": True, "ret": ret}


def raise_(kind, message=""):
    return {"__script__": True, "raise": {"type": kind, "message": message}}


RUN_OK = {"stdout": "", "returncode": 0}

# ── _safe_ints ──
add("_safe_ints", {"tokens": ["1", "2", "3"]})
add("_safe_ints", {"tokens": [" 42 ", "+7", "-3", "0"]})
add("_safe_ints", {"tokens": ["abc", "", "12x", "5"]})
add("_safe_ints", {"tokens": ["1_0", "_1", "1__0", "1_"]})
add("_safe_ints", {"tokens": [None]})
add("_safe_ints", {"tokens": [3.9, -3.9, True, False]})
add("_safe_ints", {"tokens": ["\xa012\xa0", "1\x1c"]})
add("_safe_ints", {"tokens": []})
add("_safe_ints", {"tokens": ["1\x85", "1\u2000", " + 5", "0x10"]})
_rng = random.Random(11)
add("_safe_ints", {"tokens": [str(_rng.randint(-10**6, 10**6)) for _ in range(12)]
                   + ["x", "", "9_9", "_", "1_2_3"]})

# ── _fmt_uptime ──
for secs, in [(0,), (59,), (60,), (3599,), (3600,), (3661,), (86400,), (90061,), (-5,)]:
    add("_fmt_uptime", {"seconds": secs})
add("_fmt_uptime", {"seconds": "90"})
add("_fmt_uptime", {"seconds": " 12.9 "})
add("_fmt_uptime", {"seconds": None})
add("_fmt_uptime", {"seconds": "abc"})
add("_fmt_uptime", {"seconds": "nan"})
add("_fmt_uptime", {"seconds": "inf"})
add("_fmt_uptime", {"seconds": "-Infinity"})
add("_fmt_uptime", {"seconds": True})
add("_fmt_uptime", {"seconds": 3.9})
add("_fmt_uptime", {"seconds": "1_0e2"})
add("_fmt_uptime", {"seconds": [1]})

# ── _bridge_auth_headers ──
add("_bridge_auth_headers", {"token": "abc"})
add("_bridge_auth_headers", {"token": ""})
add("_bridge_auth_headers", {"token": None})
add("_bridge_auth_headers", {"token": "tokén"})
add("_bridge_auth_headers", {"token": 123})

# ── _bridge_media_type ──
add("_bridge_media_type", {"file_path": "a.jpg", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "a.MP4", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "a.ogg", "is_voice": True, "force_document": False})
add("_bridge_media_type", {"file_path": "a.png", "is_voice": False, "force_document": True})
add("_bridge_media_type", {"file_path": "a.xyz", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "a", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "/x/.hidden", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "C:\\x\\y.JPG", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": None, "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "a.jpg", "is_voice": True, "force_document": True})
add("_bridge_media_type", {"file_path": "x/.jpg", "is_voice": False, "force_document": False})
add("_bridge_media_type", {"file_path": "a.3gp", "is_voice": False, "force_document": False})

# ── _bridge_auth_proof ──
add("_bridge_auth_proof", {"token": "token", "challenge": "challenge"})
add("_bridge_auth_proof", {"token": "", "challenge": ""})
add("_bridge_auth_proof", {"token": "kéy", "challenge": "challengé"})
add("_bridge_auth_proof", {"token": "a" * 100, "challenge": "x"})
add("_bridge_auth_proof", {"token": "t", "challenge": ""})
add("_bridge_auth_proof", {"token": None, "challenge": "c"})
add("_bridge_auth_proof", {"token": "t", "challenge": None})

# ── _bridge_config_fingerprint ──
add("_bridge_config_fingerprint", {"bridge_env": {}, "package_hash": ""})
add("_bridge_config_fingerprint", {"bridge_env": {"WHATSAPP_MODE": "self-chat"}, "package_hash": "abc"})
add("_bridge_config_fingerprint", {"bridge_env": {
    "WHATSAPP_ALLOWED_USERS": "1,2", "WHATSAPP_ALLOW_FROM": "", "WHATSAPP_DM_POLICY": "pairing",
    "WHATSAPP_GROUP_POLICY": "open", "WHATSAPP_GROUP_ALLOWED_USERS": None,
    "WHATSAPP_GROUP_ALLOW_FROM": "3", "WHATSAPP_REQUIRE_MENTION": "true",
    "WHATSAPP_MENTION_PATTERNS": "[]", "WHATSAPP_FREE_RESPONSE_CHATS": "",
    "WHATSAPP_DEBUG": True, "WHATSAPP_FORWARD_OWNER_MESSAGES": False,
    "WHATSAPP_MAX_MESSAGE_LENGTH": 4096, "WHATSAPP_CHUNK_DELAY_MS": 0,
    "WHATSAPP_SEND_TIMEOUT_MS": 30000, "WHATSAPP_MODE": "self-chat",
    "WHATSAPP_REPLY_PREFIX": "", "WHATSAPP_SEND_READ_RECEIPTS": "false",
    "HERMES_IMAGE_CACHE_DIR": "/i", "HERMES_AUDIO_CACHE_DIR": "/a",
    "HERMES_DOCUMENT_CACHE_DIR": "/d"}, "package_hash": "ph"})
add("_bridge_config_fingerprint", {"bridge_env": {"WHATSAPP_REPLY_PREFIX": ""}})
add("_bridge_config_fingerprint", {"bridge_env": {"WHATSAPP_REPLY_PREFIX": None}})
add("_bridge_config_fingerprint", {"bridge_env": {"WHATSAPP_DEBUG": True, "WHATSAPP_MAX_MESSAGE_LENGTH": 4096}})
add("_bridge_config_fingerprint", {"bridge_env": None})
add("_bridge_config_fingerprint", {"bridge_env": {}, "package_hash": None})
add("_bridge_config_fingerprint", {"bridge_env": {"UNRELATED_KEY": "x", "WHATSAPP_MODE": "group"}})

# ── _classify_bridge_message ──
add("_classify_bridge_message", {"data": {"mediaType": "location"}})
add("_classify_bridge_message", {"data": {"mediaType": "live_location"}})
add("_classify_bridge_message", {"data": {"mediaType": "sticker"}})
add("_classify_bridge_message", {"data": {}})
add("_classify_bridge_message", {"data": {"hasMedia": False}})
add("_classify_bridge_message", {"data": {"mediaType": "", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": "image/jpeg", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": "video/mp4", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": "audio/ogg", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": "ptt", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": "audio/ptt", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": "application/pdf", "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": None, "hasMedia": True}})
add("_classify_bridge_message", {"data": {"mediaType": 5, "hasMedia": True}})

# ── _wenv ──
add("_wenv", {"name": "WHATSAPP_MODE"}, {"get_scoped_secret": "self-chat"})
add("_wenv", {"name": "WHATSAPP_REPLY_PREFIX", "default": "d"}, {"get_scoped_secret": None})
add("_wenv", {"name": "X"}, {"get_scoped_secret": ""})
add("_wenv", {"name": "X"}, {"get_scoped_secret": raise_("RuntimeError", "scoped")})
add("_wenv", {"name": "X"}, {}, fail_closed=True)
add("_wenv", {"name": "X"}, {"get_scoped_secret": raise_("SystemExit", "bye")})

# ── _is_connected ──
add("_is_connected", {"config": {"enabled": True, "extra": {"x": 1}}})
add("_is_connected", {"config": {"enabled": True, "extra": {}}}, {"get_env_value": "true"})
add("_is_connected", {"config": {"enabled": False}}, {"get_env_value": "false"})
add("_is_connected", {"config": None}, {"get_env_value": " YES "})
add("_is_connected", {"config": {}}, {"get_env_value": None})
add("_is_connected", {"config": {"enabled": True, "extra": None}}, {"get_env_value": "1"})
add("_is_connected", {"config": "str"}, {"get_env_value": "0"})
add("_is_connected", {"config": None}, {"get_env_value": raise_("SystemExit", "bye")})
add("_is_connected", {"config": None}, {"get_env_value": 5})
add("_is_connected", {"config": None}, {}, fail_closed=True)

# ── _cache_dirs ──
add("_cache_dirs", {}, {"get_image_cache_dir": "/i", "get_audio_cache_dir": "/a",
                        "get_video_cache_dir": "/v", "get_document_cache_dir": "/d"})
add("_cache_dirs", {}, {"get_image_cache_dir": "/i", "get_audio_cache_dir": None,
                        "get_video_cache_dir": "/v", "get_document_cache_dir": "/d"})
add("_cache_dirs", {}, {"get_image_cache_dir": "/i"}, fail_closed=True)
add("_cache_dirs", {}, {"get_image_cache_dir": "/i", "get_audio_cache_dir": "/a",
                        "get_video_cache_dir": raise_("ValueError", "bad")})
add("_cache_dirs", {}, {"get_image_cache_dir": raise_("SystemExit", "bye")})
add("_cache_dirs", {}, {"get_image_cache_dir": "", "get_audio_cache_dir": "",
                        "get_video_cache_dir": "", "get_document_cache_dir": ""})

# ── _unlink_quietly ──
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {"unlink": None})
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {"unlink": raise_("OSError", "io")})
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {"unlink": raise_("FileNotFoundError", "gone")})
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {"unlink": raise_("PermissionError", "no")})
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {"unlink": raise_("ValueError", "bad")})
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {"unlink": raise_("SystemExit", "bye")})
add("_unlink_quietly", {"path": "/s/bridge.pid"}, {}, fail_closed=True)

# ── _tighten_bridge_log ──
add("_tighten_bridge_log", {"path": "/s/bridge.log", "private": True}, {"chmod": None})
add("_tighten_bridge_log", {"path": "/s/bridge.log", "private": False})
add("_tighten_bridge_log", {"path": "/s/bridge.log", "private": True}, {"chmod": raise_("OSError", "io")})
add("_tighten_bridge_log", {"path": "/s/bridge.log", "private": True}, {"chmod": raise_("ValueError", "bad")})
add("_tighten_bridge_log", {"path": "/s/bridge.log", "private": True}, {"chmod": raise_("SystemExit", "bye")})
add("_tighten_bridge_log", {"path": "/s/bridge.log", "private": True}, {}, fail_closed=True)

# ── _read_bridge_token ──
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": "tok"})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": "  tok  "})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": "\ufefftok"})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": ""})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": raise_("OSError", "io")})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": raise_("ValueError", "bad")})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": None})
add("_read_bridge_token", {"session_path": "/s"}, {"read_text": raise_("SystemExit", "bye")})
add("_read_bridge_token", {"session_path": "/s"}, {}, fail_closed=True)

# ── _write_bridge_fingerprint ──
add("_write_bridge_fingerprint", {"session_path": "/s", "fingerprint": "fp"}, {"write_text": None})
add("_write_bridge_fingerprint", {"session_path": "/s", "fingerprint": ""}, {"write_text": raise_("OSError", "io")})
add("_write_bridge_fingerprint", {"session_path": "/s", "fingerprint": "fp"}, {"write_text": raise_("ValueError", "bad")})
add("_write_bridge_fingerprint", {"session_path": "/s", "fingerprint": "fp"}, {"write_text": raise_("SystemExit", "bye")})
add("_write_bridge_fingerprint", {"session_path": "/s", "fingerprint": "fp"}, {}, fail_closed=True)

# ── _write_bridge_pidfile ──
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123},
    {"get_process_start_time": None, "write_text": None})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123},
    {"get_process_start_time": 999, "write_text": None})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123, "port": 3000},
    {"get_process_start_time": 999, "write_text": None})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123, "port": 3000},
    {"get_process_start_time": None, "write_text": None})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123},
    {"get_process_start_time": raise_("OSError", "io")})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123},
    {"get_process_start_time": 999, "write_text": raise_("OSError", "io")})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123},
    {"get_process_start_time": 999, "write_text": raise_("ValueError", "bad")})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123},
    {"get_process_start_time": raise_("SystemExit", "bye")})
add("_write_bridge_pidfile", {"session_path": "/s", "pid": 123}, {}, fail_closed=True)

# ── _read_bridge_pidfile ──
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": False})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": "123"})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": "123\n999\n3000"})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": "123\n\n3000"})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": " 123 \n 999 "})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": "abc"})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": "\ufeff123\n999"})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": None})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": raise_("OSError", "io")})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": raise_("ValueError", "bad")})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": raise_("SystemExit", "bye")})
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": raise_("RuntimeError", "x")})
add("_read_bridge_pidfile", {"session_path": "/s"}, {}, fail_closed=True)
add("_read_bridge_pidfile", {"session_path": "/s"}, {"exists": True, "read_text": "123\n999\nbad"})

# ── _bridge_pid_is_ours ──
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999}, {"_pid_exists": False})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": None}, {"_pid_exists": True})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999},
    {"_pid_exists": True, "get_process_start_time": 999})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999},
    {"_pid_exists": True, "get_process_start_time": 998})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999},
    {"_pid_exists": True, "get_process_start_time": None})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999},
    {"_pid_exists": True, "get_process_start_time": "999"})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999},
    {"_pid_exists": raise_("SystemExit", "bye")})
add("_bridge_pid_is_ours", {"pid": 101, "expected_start": 999}, {}, fail_closed=True)

# ── _pids_alive ──
add("_pids_alive", {"pids": [1, 2, 3]}, {"_pid_exists": [True, False, True]})
add("_pids_alive", {"pids": [1, 2]}, {"_pid_exists": False})
add("_pids_alive", {"pids": []})
add("_pids_alive", {"pids": [1]}, {"_pid_exists": None})
add("_pids_alive", {"pids": [1]}, {"_pid_exists": raise_("ValueError", "bad")})
add("_pids_alive", {"pids": [1]}, {"_pid_exists": raise_("SystemExit", "bye")})
add("_pids_alive", {"pids": [1]}, {}, fail_closed=True)

# ── _wait_pids_exit ──
add("_wait_pids_exit", {"pids": [1]},
    {"time.monotonic": [0], "_pids_alive": script([])})
add("_wait_pids_exit", {"pids": [1]},
    {"time.monotonic": [0, 0.05], "_pids_alive": [[1], []], "time.sleep": None})
add("_wait_pids_exit", {"pids": [1]},
    {"time.monotonic": [0, 0.05, 5.1], "_pids_alive": [[1], [1]], "time.sleep": None})
add("_wait_pids_exit", {"pids": [1], "timeout": 2},
    {"time.monotonic": [0, 3], "_pids_alive": script([1])})
add("_wait_pids_exit", {"pids": [1]}, {"time.monotonic": None})
add("_wait_pids_exit", {"pids": [1]},
    {"time.monotonic": [0, "x"], "_pids_alive": script([1])})
add("_wait_pids_exit", {"pids": [1]},
    {"time.monotonic": [0], "_pids_alive": raise_("SystemExit", "bye")})
add("_wait_pids_exit", {"pids": [1]}, {}, fail_closed=True)
add("_wait_pids_exit", {"pids": [1], "timeout": 0},
    {"time.monotonic": [0, 0], "_pids_alive": script([1])})

# ── _pid_looks_like_node_bridge ──
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "node.exe", "cmdline": []}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "bash", "cmdline": ["node", "bridge.js"]}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "bash", "cmdline": ["/usr/bin/nodejs", "x"]}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "bash", "cmdline": ["python", "node"]}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": None, "cmdline": ["node"]}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "Node", "cmdline": []}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "bash"}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": 5, "cmdline": ["node"]}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": None})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": raise_("NoSuchProcess", "gone")})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": raise_("SystemExit", "bye")})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "bash", "cmdline": ["node", 5]}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {"psutil.Process": {"name": "bash", "cmdline": None}})
add("_pid_looks_like_node_bridge", {"pid": 101}, {}, fail_closed=True)

# ── _windows_listener_pids ──
NETSTAT = ("  TCP    0.0.0.0:3000    0.0.0.0:0    LISTENING    1234\n"
           "  TCP    0.0.0.0:3001    0.0.0.0:0    LISTENING    5678\n"
           "  TCP    0.0.0.0:3000    0.0.0.0:0    ESTABLISHED  9999\n"
           "  TCP    0.0.0.0:3000    0.0.0.0:0    LISTENING\n")
add("_windows_listener_pids", {"port": 3000},
    {"windows_hide_flags": 8, "subprocess.run": {"stdout": NETSTAT, "returncode": 0}})
add("_windows_listener_pids", {"port": 300},
    {"windows_hide_flags": 8, "subprocess.run": {"stdout": NETSTAT, "returncode": 0}})
add("_windows_listener_pids", {"port": 3000},
    {"windows_hide_flags": 8, "subprocess.run": {"stdout": "", "returncode": 0}})
add("_windows_listener_pids", {"port": 3000},
    {"windows_hide_flags": 8, "subprocess.run": {"stdout": "  TCP    0.0.0.0:3000    0.0.0.0:0    LISTENING    abc\n", "returncode": 0}})
add("_windows_listener_pids", {"port": 3000},
    {"windows_hide_flags": raise_("RuntimeError", "flags")})
add("_windows_listener_pids", {"port": 3000},
    {"windows_hide_flags": 8, "subprocess.run": raise_("FileNotFoundError", "netstat")})
add("_windows_listener_pids", {"port": 3000}, {"windows_hide_flags": 8, "subprocess.run": None})
add("_windows_listener_pids", {"port": 3000}, {"windows_hide_flags": 8, "subprocess.run": {"returncode": 0}})
add("_windows_listener_pids", {"port": 3000}, {}, fail_closed=True)
add("_windows_listener_pids", {"port": 3000},
    {"windows_hide_flags": 8, "subprocess.run": {"stdout": "TCP 0.0.0.0:3000 x LISTENING 42\r\nTCP [::]:3000 x LISTENING 43\n", "returncode": 0}})

# ── _listener_pids_on_port ──
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [{"stdout": "111\n222\n", "returncode": 0}]})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), {"stdout": "pid=333 pid=444", "returncode": 0}]})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), raise_("FileNotFoundError", "ss")],
     "psutil.net_connections": script([])})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), raise_("FileNotFoundError", "ss")],
     "psutil.net_connections": script([
         {"status": "LISTEN", "laddr": {"port": 3000}, "pid": 555},
         {"status": "ESTABLISHED", "laddr": {"port": 3000}, "pid": 666},
         {"status": "LISTEN", "laddr": {"port": 3001}, "pid": 777},
         {"status": "LISTEN", "laddr": None, "pid": 888},
         {"status": "LISTEN", "laddr": {"port": 3000}, "pid": 0},
         {"status": "LISTEN", "laddr": {}, "pid": 999}])})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [{"stdout": "", "returncode": 0}, {"stdout": "nope pid=100", "returncode": 0}]})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [{"stdout": "abc\n", "returncode": 0}, {"stdout": "pid=7", "returncode": 0}]})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("ValueError", "bad")]})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), raise_("FileNotFoundError", "ss")],
     "psutil.net_connections": raise_("RuntimeError", "psutil")})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), raise_("FileNotFoundError", "ss")],
     "psutil.net_connections": raise_("SystemExit", "bye")})
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), raise_("FileNotFoundError", "ss")],
     "psutil.net_connections": script([{"laddr": {"port": 3000}, "pid": 1}])})
add("_listener_pids_on_port", {"port": 3000}, {"subprocess.run": [None]})
add("_listener_pids_on_port", {"port": 3000}, {}, fail_closed=True)
add("_listener_pids_on_port", {"port": 3000},
    {"subprocess.run": [raise_("FileNotFoundError", "lsof"), {"stdout": "pid=pid=5 pid=6x", "returncode": 0}]})

# ── _port_listener_pids ──
add("_port_listener_pids", {"port": 3000, "is_windows": True}, {"_windows_listener_pids": script([1, 2])})
add("_port_listener_pids", {"port": 3000, "is_windows": False}, {"_listener_pids_on_port": script([3])})
add("_port_listener_pids", {"port": 3000, "is_windows": True}, {"_windows_listener_pids": raise_("ValueError", "bad")})
add("_port_listener_pids", {"port": 3000, "is_windows": False}, {"_listener_pids_on_port": raise_("SystemExit", "bye")})
add("_port_listener_pids", {"port": 3000, "is_windows": True}, {}, fail_closed=True)
add("_port_listener_pids", {"port": 3000, "is_windows": False}, {"_listener_pids_on_port": script([])})

# ── _kill_port_process ──
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101, 102]), "_pid_looks_like_node_bridge": [True, True],
     "os.kill": None, "_wait_pids_exit": [[], []]})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([0, 101]), "_pid_looks_like_node_bridge": [True],
     "logger.warning": None, "os.kill": None, "_wait_pids_exit": [[], []]})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [False],
     "logger.warning": None})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True],
     "os.kill": raise_("OSError", "perm")})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True, True],
     "os.kill": [None, None], "logger.warning": None, "_wait_pids_exit": [[101], []]})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True, False],
     "os.kill": None, "logger.warning": None, "_wait_pids_exit": [[101], []]})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101, 102]), "_pid_looks_like_node_bridge": [True],
     "os.kill": raise_("ValueError", "bad")})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": raise_("RuntimeError", "scan")})
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True],
     "os.kill": raise_("SystemExit", "bye")})
add("_kill_port_process", {"port": 3000, "is_windows": True},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True],
     "windows_hide_flags": 8, "subprocess.run": RUN_OK})
add("_kill_port_process", {"port": 3000, "is_windows": True},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True],
     "windows_hide_flags": 8, "subprocess.run": raise_("SubprocessError", "taskkill")})
add("_kill_port_process", {"port": 3000, "is_windows": True},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True],
     "windows_hide_flags": 8, "subprocess.run": raise_("OSError", "missing")})
add("_kill_port_process", {"port": 3000, "is_windows": False}, {}, fail_closed=True)
add("_kill_port_process", {"port": 3000, "is_windows": False},
    {"_port_listener_pids": script([101]), "_pid_looks_like_node_bridge": [True],
     "os.kill": None, "_wait_pids_exit": [raise_("SystemExit", "bye")]})

# ── _kill_stale_bridge_by_pidfile ──
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False}, {"exists": False})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([None, None, None]), "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [True, True], "os.kill": [None, None], "logger.info": None,
     "logger.warning": None, "_wait_pids_exit": [[101], []], "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [True], "os.kill": None, "logger.info": None,
     "_wait_pids_exit": script([]), "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": True},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [True], "os.kill": None, "logger.info": None, "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, None, None]),
     "_bridge_pid_is_ours": [False], "_pid_exists": True, "logger.warning": None,
     "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [False], "_pid_exists": True, "logger.warning": None,
     "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [False], "_pid_exists": False, "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": True},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [True], "os.kill": raise_("OSError", "perm"), "_unlink_quietly": None})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [True], "os.kill": raise_("SystemExit", "bye")})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": raise_("ValueError", "bad")})
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False}, {}, fail_closed=True)
add("_kill_stale_bridge_by_pidfile", {"session_path": "/s", "is_windows": False},
    {"exists": True, "_read_bridge_pidfile": script([101, 999, None]),
     "_bridge_pid_is_ours": [True], "os.kill": None, "logger.info": raise_("OSError", "log"),
     "_wait_pids_exit": script([]), "_unlink_quietly": None})

# ── check_whatsapp_requirements ──
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/usr/bin/node", "with_hermes_node_path": {},
     "subprocess.run": {"returncode": 0}})
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/usr/bin/node", "with_hermes_node_path": {},
     "subprocess.run": {"returncode": 1}})
add("check_whatsapp_requirements", {}, {"find_node_executable": None, "lazy_installs_allowed": True})
add("check_whatsapp_requirements", {}, {"find_node_executable": None, "lazy_installs_allowed": False})
add("check_whatsapp_requirements", {}, {"find_node_executable": "", "lazy_installs_allowed": True})
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/n", "with_hermes_node_path": {},
     "subprocess.run": raise_("RuntimeError", "run")})
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/n", "with_hermes_node_path": {},
     "subprocess.run": raise_("SystemExit", "bye")})
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/n", "with_hermes_node_path": raise_("ValueError", "env")})
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/n", "with_hermes_node_path": {}, "subprocess.run": {}})
add("check_whatsapp_requirements", {},
    {"find_node_executable": "/n", "with_hermes_node_path": {}, "subprocess.run": None})
add("check_whatsapp_requirements", {}, {}, fail_closed=True)

# ── _file_content_hash ──
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": "hello"})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": ""})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": "héllo"})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": raise_("OSError", "io")})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": raise_("FileNotFoundError", "gone")})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": raise_("ValueError", "bad")})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": None})
add("_file_content_hash", {"path": "/x/package.json"}, {"read_bytes": raise_("SystemExit", "bye")})
add("_file_content_hash", {"path": "/x/package.json"}, {}, fail_closed=True)

# ── _bridge_source_hash ──
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {"read_bytes": ["a", "b"]})
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {"read_bytes": ["a", raise_("OSError", "io")]})
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {"read_bytes": [raise_("OSError", "io")]})
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {"read_bytes": ["a", None]})
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {"read_bytes": [raise_("SystemExit", "bye")]})
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {}, fail_closed=True)
add("_bridge_source_hash", {"bridge_path": "/x/bridge.js"}, {"read_bytes": ["", ""]})

# ── _spawn_bridge_process ──
add("_spawn_bridge_process", {"argv": ["/n", "/x/bridge.js", "--port", "3000"], "log_fh": "fh", "env": {"A": "1"}},
    {"windows_detach_flags": 0, "spawn_owned": {"pid": 42}})
add("_spawn_bridge_process", {"argv": ["/n"], "log_fh": None, "env": {}},
    {"windows_detach_flags": 256, "spawn_owned": {"pid": 7}})
add("_spawn_bridge_process", {"argv": ["/n"], "log_fh": "fh", "env": {}, "umask": 63},
    {"windows_detach_flags": 0, "spawn_owned": {"pid": 7}})
add("_spawn_bridge_process", {"argv": ["/n"], "log_fh": "fh", "env": {}},
    {"windows_detach_flags": 0, "spawn_owned": raise_("RuntimeError", "spawn")})
add("_spawn_bridge_process", {"argv": ["/n"], "log_fh": "fh", "env": {}},
    {"windows_detach_flags": raise_("SystemExit", "bye")})
add("_spawn_bridge_process", {"argv": ["/n"], "log_fh": "fh", "env": {}}, {}, fail_closed=True)
add("_spawn_bridge_process", {"argv": [], "log_fh": "fh", "env": {}, "umask": -1},
    {"windows_detach_flags": 0, "spawn_owned": None})

# ── _normalize_outbound_mentions ──
add("_normalize_outbound_mentions", {"mentions": ["a", "b", "a"]},
    {"normalize_whatsapp_mention_jid": ["1@a", "2@a", "1@a"]})
add("_normalize_outbound_mentions", {"mentions": ["x", "y", "z"]},
    {"normalize_whatsapp_mention_jid": ["1@a", None, ""]})
add("_normalize_outbound_mentions", {"mentions": None})
add("_normalize_outbound_mentions", {"mentions": []})
add("_normalize_outbound_mentions", {"mentions": ["x"]},
    {"normalize_whatsapp_mention_jid": raise_("ValueError", "bad")})
add("_normalize_outbound_mentions", {"mentions": ["x"]},
    {"normalize_whatsapp_mention_jid": raise_("SystemExit", "bye")})
add("_normalize_outbound_mentions", {"mentions": ["x"]}, {}, fail_closed=True)
add("_normalize_outbound_mentions", {"mentions": ["x", "y"]},
    {"normalize_whatsapp_mention_jid": ["", None]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, c in counts.items() if c < 5]
    if thin:
        raise SystemExit(f"THIN (<5 cases): {thin}")


if __name__ == "__main__":
    main()
