#!/usr/bin/env python3
"""Generator for tests/fixtures/whatsapp_adapter_flow2.jsonl.

Emits >=5 cases per ported function of the WhatsApp adapter session/token/
auth/env FLOW remainder port (src/gateway/platforms/whatsapp_adapter_flow2.c).
Cases carry ONLY inputs + scripted seam results (in.io); expected values are
computed by the oracle from the LIVE quarry defs. Script semantics (shared
with the C harness): a bare JSON array is a per-call QUEUE (one result per
call; exhaustion is a fail-closed RuntimeError); any other value is returned
verbatim on every call; {"__script__": true, "ret": v} forces a single
result; {"__script__": true, "raise": {type, message}} raises. List-valued
seam results are wrapped one level deeper ([["a"]] or __script__).

Cases marked "fail_closed": true intentionally leave a seam unscripted to
exercise the fail-closed RuntimeError parity (the oracle self-test allows
the script gap only on those).

Corpus restrictions honoured here (documented in the C port header):
ASCII str()-positions, |int values| < 2^53, POSIX lexical paths, ASCII
proof/challenge/token strings, proc doubles carry pid only, psutil.Process
result pid == input proc.pid, self.name always present, extra/config/data
carriers are objects|null, mkstemp results [fd, tmp], _cache_dirs 4-element
path lists, resp.exit null on body-raise cases.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "whatsapp_adapter_flow2.jsonl")
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


def self_base(**over):
    d = {
        "name": "whatsapp", "_bridge_port": 3000,
        "_bridge_process": {"pid": 4242}, "_bridge_log": "/h/bridge.log",
        "_bridge_log_fh": None, "_session_path": "/h/sess",
        "_bridge_script": "/h/bridge/bridge.js", "_reply_prefix": None,
        "_send_read_receipts": True, "_bridge_token": "tok",
        "_dm_policy": "pairing", "_group_policy": "open",
        "_allow_from": ["2@s.whatsapp.net", "1@s.whatsapp.net"],
        "_group_allow_from": [], "has_fatal_error": False,
        "fatal_error_message": None, "_shutting_down": False,
        "gateway_runner": None, "config": {"extra": {}},
        "_http_session": {"__session__": True},
    }
    d.update(over)
    return d


# -- _session_dir --
add("_session_dir", {"extra": None},
    {"get_hermes_dir": script("/h/d"), "expanduser": script("/h/d"), "abspath": script("/h/d")})
add("_session_dir", {"extra": {"session_path": "~/x"}},
    {"get_hermes_dir": script("/h/d"), "expanduser": script("/home/u/x"), "abspath": script("/home/u/x")})
add("_session_dir", {"extra": {"session_path": "/a//b/"}},
    {"get_hermes_dir": script("/h/d"), "expanduser": script("/a//b/"), "abspath": script("/a//b/")})
add("_session_dir", {"extra": {"session_path": None}},
    {"get_hermes_dir": script("/h/d"), "expanduser": script("None"), "abspath": script("/cwd/None")})
add("_session_dir", {"extra": {}},
    {"get_hermes_dir": script("/h/d"), "expanduser": script("/h/d"), "abspath": script("/h/d")})
add("_session_dir", {"extra": {"session_path": "rel/dir"}},
    {"get_hermes_dir": script("/h/d"), "expanduser": script("rel/dir"), "abspath": script("/cwd/rel/dir")})
add("_session_dir", {"extra": None},
    {"get_hermes_dir": script("/h/d"), "expanduser": raise_("OSError", "no home")})
add("_session_dir", {"extra": None}, {}, fail_closed=True)

# -- _owner_only_opener --
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": True},
    {"os.open": script(7), "os.fchmod": script(None)})
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": False},
    {"os.open": script(7)})
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": True},
    {"os.open": script(7), "os.fchmod": raise_("OSError", "fchmod denied")})
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": True},
    {"os.open": script(7), "os.fchmod": raise_("PermissionError", "nope")})
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": True},
    {"os.open": script(7), "os.fchmod": raise_("SystemExit", "1")})
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": True},
    {"os.open": raise_("FileNotFoundError", "missing")})
add("_owner_only_opener", {"path": "/h/bridge.log", "flags": 1089, "has_fchmod": True},
    {}, fail_closed=True)

# -- _open_bridge_log --
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": script({"st_size": 10}), "open": script({"fd": 7})})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": script({"st_size": 6000000}), "os.replace": script(None), "open": script({"fd": 7})})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": False},
    {"stat": script({"st_size": 10}), "open": script({"fd": 7})})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": raise_("FileNotFoundError", "gone"), "open": script({"fd": 7})})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": script({"st_size": 6000000}), "os.replace": raise_("OSError", "held"),
     "open": script({"fd": 7})})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": script({})})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": script({"st_size": 10}), "open": raise_("OSError", "cannot open")})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True},
    {"stat": raise_("SystemExit", "1")})
add("_open_bridge_log", {"path": "/h/bridge.log", "private": True}, {}, fail_closed=True)

# -- _terminate_bridge_process --
RUN_OK = {"returncode": 0, "stderr": "", "stdout": ""}
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {"subprocess.run": script(RUN_OK)})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": True, "is_windows": True},
    {"subprocess.run": script({"returncode": 1, "stderr": " boom ", "stdout": ""})})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {"subprocess.run": script({"returncode": 1, "stderr": "", "stdout": " out "})})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {"subprocess.run": script({"returncode": 1, "stderr": "", "stdout": ""})})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {"subprocess.run": raise_("FileNotFoundError", "taskkill missing"),
     "proc.terminate": script("done")})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": True, "is_windows": True},
    {"subprocess.run": raise_("FileNotFoundError", "taskkill missing"),
     "proc.kill": script(None)})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {"subprocess.run": raise_("OSError", "other")})
add("_terminate_bridge_process", {"proc": {}, "force": False, "is_windows": True}, {})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {"subprocess.run": script({"stderr": "", "stdout": ""})})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": False},
    {"psutil.Process": script({"pid": 100}), "psutil.children": script([{"pid": 101}, {"pid": 102}]),
     "child.terminate": script(None), "proc.terminate": script(None)})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": True, "is_windows": False},
    {"psutil.Process": script({"pid": 100}), "psutil.children": script([{"pid": 101}]),
     "child.kill": raise_("NoSuchProcess", "gone"), "proc.kill": script(None)})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": False},
    {"psutil.Process": raise_("NoSuchProcess", "gone")})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": False},
    {"psutil.Process": script({"pid": 100}), "psutil.children": raise_("NoSuchProcess", "gone")})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": True, "is_windows": False},
    {"psutil.Process": script({"pid": 100}), "psutil.children": script([[]]),
     "proc.kill": raise_("NoSuchProcess", "gone")})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": False},
    {"psutil.Process": script({"pid": 100}), "psutil.children": script([[]]),
     "proc.terminate": raise_("PermissionError", "denied")})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": False},
    {"psutil.Process": raise_("SystemExit", "1")})
add("_terminate_bridge_process", {"proc": {"pid": 100}, "force": False, "is_windows": True},
    {}, fail_closed=True)

# -- _is_allowed_bridge_path --
CD4 = script(["/h/img", "/h/aud", "/h/vid", "/h/doc"])
add("_is_allowed_bridge_path", {"url": "/h/img/a.jpg"},
    {"resolve": ["/h/img/a.jpg", "/h/img"], "_cache_dirs": CD4,
     "is_relative_to": script(True)})
add("_is_allowed_bridge_path", {"url": "/h/aud/b.ogg"},
    {"resolve": ["/h/aud/b.ogg", "/h/img", "/h/aud"], "_cache_dirs": CD4,
     "is_relative_to": [False, True]})
add("_is_allowed_bridge_path", {"url": "/etc/passwd"},
    {"resolve": ["/etc/passwd", "/h/img", "/h/aud", "/h/vid", "/h/doc"], "_cache_dirs": CD4,
     "is_relative_to": script(False)})
add("_is_allowed_bridge_path", {"url": "bad"},
    {"resolve": raise_("ValueError", "bad path")})
add("_is_allowed_bridge_path", {"url": "/x"},
    {"resolve": raise_("OSError", "loop")})
add("_is_allowed_bridge_path", {"url": "/h/img/a.jpg"},
    {"resolve": ["/h/img/a.jpg", raise_("OSError", "root gone"), "/h/aud"],
     "_cache_dirs": CD4, "is_relative_to": script(True)})
add("_is_allowed_bridge_path", {"url": "/h/img/a.jpg"},
    {"resolve": ["/h/img/a.jpg", "/h/img", "/h/aud", "/h/vid", "/h/doc"], "_cache_dirs": CD4,
     "is_relative_to": [raise_("ValueError", "cmp"), False, False, False]})
add("_is_allowed_bridge_path", {"url": "/h/img/a.jpg"},
    {"resolve": script("/h/img/a.jpg"), "_cache_dirs": raise_("RuntimeError", "no dirs")})
add("_is_allowed_bridge_path", {"url": "/x"},
    {"resolve": raise_("SystemExit", "1")})
add("_is_allowed_bridge_path", {"url": "/h/img/a.jpg"}, {}, fail_closed=True)

# -- _bridge_token_lock --
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt"},
    {"open": script({}), "handle.seek": [0, 0, 0], "handle.write": script(None),
     "handle.flush": script(None), "handle.fileno": script(5),
     "msvcrt.locking": [None, None], "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt"},
    {"open": script({}), "handle.seek": [5, 0, 0], "handle.fileno": script(5),
     "msvcrt.locking": [None, None], "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "posix"},
    {"open": script({}), "handle.fileno": script(5), "fcntl.flock": [None, None],
     "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt",
                           "body_raise": {"type": "ValueError", "message": "body boom"}},
    {"open": script({}), "handle.seek": [0, 0, 0], "handle.write": script(None),
     "handle.flush": script(None), "handle.fileno": script(5),
     "msvcrt.locking": [None, None], "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt"},
    {"open": script({}), "handle.seek": script(0), "handle.write": script(None),
     "handle.flush": script(None), "handle.fileno": script(5),
     "msvcrt.locking": raise_("OSError", "lock busy"), "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt"},
    {"open": raise_("OSError", "cannot open")})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt",
                           "body_raise": {"type": "ValueError", "message": "body boom"}},
    {"open": script({}), "handle.seek": [0, 0, 0], "handle.write": script(None),
     "handle.flush": script(None), "handle.fileno": script(5),
     "msvcrt.locking": [None, None], "handle.close": raise_("RuntimeError", "close boom")})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "posix"},
    {"open": script({}), "handle.fileno": script(5),
     "fcntl.flock": raise_("OSError", "flock busy"), "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "posix",
                           "body_raise": {"type": "SystemExit", "message": "1"}},
    {"open": script({}), "handle.fileno": script(5), "fcntl.flock": [None, None],
     "handle.close": script(None)})
add("_bridge_token_lock", {"lock_path": "/h/sess/.bridge-token.lock", "os_name": "nt"},
    {}, fail_closed=True)

# -- _load_or_create_bridge_token --
TOK32 = "x" * 32
MINT_IO = {
    "mkdir": script(None), "_bridge_token_lock": script(None),
    "token_lock.enter": script(None), "secrets.token_urlsafe": script("NEWTOKEN"),
    "tempfile.mkstemp": script([9, "/h/sess/.bridge-token.tmp"]),
    "os.fdopen": script({}), "handle.write": script(None), "handle.flush": script(None),
    "handle.fileno": script(9), "os.fsync": script(None), "handle.close": script(None),
    "os.replace": script(None), "token_lock.exit": script(None),
}
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    {"mkdir": script(None), "_bridge_token_lock": script(None),
     "token_lock.enter": script(None), "read_text": script(TOK32),
     "token_lock.exit": script(None)})
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    dict(MINT_IO, read_text=script("short")))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": True}, dict(MINT_IO))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    dict(MINT_IO, read_text=raise_("FileNotFoundError", "absent")))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    {"mkdir": script(None), "_bridge_token_lock": script(None),
     "token_lock.enter": script(None), "read_text": raise_("SystemExit", "1"),
     "token_lock.exit": script(None)})
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    dict(MINT_IO, read_text=script("short"), **{
        "handle.write": raise_("OSError", "disk full"),
        "os.unlink": script(None),
        "token_lock.exit": script(None)}))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    dict(MINT_IO, read_text=script("short"), **{
        "os.replace": raise_("OSError", "rename failed"),
        "os.unlink": script(None),
        "token_lock.exit": script(None)}))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    dict(MINT_IO, read_text=script("short"), **{
        "handle.write": raise_("OSError", "disk full"),
        "os.unlink": raise_("FileNotFoundError", "tmp gone"),
        "token_lock.exit": script(None)}))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    dict(MINT_IO, read_text=script("short"), **{
        "tempfile.mkstemp": raise_("OSError", "no tmp"),
        "token_lock.exit": script(None)}))
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    {"mkdir": script(None), "_bridge_token_lock": script(None),
     "token_lock.enter": raise_("OSError", "lock failed")})
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    {"mkdir": script(None), "_bridge_token_lock": script(None),
     "token_lock.enter": script(None), "read_text": script("\ufeff" + "y" * 32),
     "token_lock.exit": script(None)})
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    {"mkdir": script(None), "_bridge_token_lock": script(None),
     "token_lock.enter": script(None), "read_text": script("short"),
     "secrets.token_urlsafe": raise_("SystemExit", "1"),
     "token_lock.exit": script(None)})
add("_load_or_create_bridge_token", {"session_path": "/h/sess", "rotate": False},
    {}, fail_closed=True)

# -- _import_aiohttp --
add("_import_aiohttp", {"aiohttp": {"client_session": True}})
add("_import_aiohttp", {"aiohttp": None})
add("_import_aiohttp", {"aiohttp": {"client_session": False, "file": "/x/aiohttp.py"}})
add("_import_aiohttp", {"aiohttp": {"client_session": False, "file": None, "path": ["/x/aiohttp"]}})
add("_import_aiohttp", {"aiohttp": {"client_session": False}})
add("_import_aiohttp", {"aiohttp": {"client_session": False, "file": "", "path": []}})
add("_import_aiohttp", {"aiohttp": {"client_session": False, "file": "/x/a.py", "path": ["/p1", "/p2"]}})
add("_import_aiohttp", {"aiohttp": {"client_session": False, "file": None, "path": ["/x/it's", '/y/q"uote']}})

# -- ensure_aiohttp --
PM_IO = {"repo_root": script("/r"), "lazy_installs_allowed": script(True),
         "running_from_selected_environment": script(True),
         "ensure_import": script(None)}
add("ensure_aiohttp", {}, {"_import_aiohttp": script(None)})
add("ensure_aiohttp", {}, {"_import_aiohttp": raise_("ImportError", "broken install")})
add("ensure_aiohttp", {}, dict(PM_IO, _import_aiohttp=[
    raise_("ModuleNotFoundError", "import of aiohttp halted; None in sys.modules"),
    script(None)]))
add("ensure_aiohttp", {}, dict(PM_IO, lazy_installs_allowed=script(False),
                               ensure_import=raise_("RuntimeError", "busy lock"),
                               _import_aiohttp=raise_("ModuleNotFoundError", "no aiohttp")))
add("ensure_aiohttp", {}, dict(PM_IO, running_from_selected_environment=script(False),
                               restart_needed=script(None),
                               ensure_import=raise_("OSError", ""),
                               _import_aiohttp=raise_("ModuleNotFoundError", "no aiohttp")))
add("ensure_aiohttp", {}, dict(PM_IO, running_from_selected_environment=script(False),
                               restart_needed=script("gen2"),
                               ensure_import=raise_("ValueError", "bad extra"),
                               _import_aiohttp=raise_("ModuleNotFoundError", "no aiohttp")))
add("ensure_aiohttp", {}, dict(PM_IO, _import_aiohttp=[
    raise_("ModuleNotFoundError", "no aiohttp"),
    raise_("ImportError", "still broken")]))
add("ensure_aiohttp", {}, dict(PM_IO, ensure_import=raise_("SystemExit", "1"),
                               _import_aiohttp=raise_("ModuleNotFoundError", "no aiohttp")))
add("ensure_aiohttp", {}, {"_import_aiohttp": raise_("TypeError", "weird")})
add("ensure_aiohttp", {}, dict(PM_IO, repo_root=raise_("RuntimeError", "no root"),
                               _import_aiohttp=raise_("ModuleNotFoundError", "no aiohttp")))
add("ensure_aiohttp", {}, {}, fail_closed=True)

# -- _needs_bridge --
add("_needs_bridge", {"call_args": ["c1"], "call_kwargs": {"k": 1}},
    {"_bridge_unavailable": script(None), "method": script({"ok": True})})
add("_needs_bridge", {"call_args": [], "call_kwargs": {}},
    {"_bridge_unavailable": script("Not connected")})
add("_needs_bridge", {"call_args": [], "call_kwargs": {}},
    {"_bridge_unavailable": raise_("RuntimeError", "probe boom")})
add("_needs_bridge", {"call_args": ["c1"], "call_kwargs": {}},
    {"_bridge_unavailable": script(None), "method": raise_("ValueError", "send boom")})
add("_needs_bridge", {"call_args": [], "call_kwargs": {}},
    {"_bridge_unavailable": raise_("SystemExit", "1")})
add("_needs_bridge", {"call_args": [], "call_kwargs": {}},
    {"_bridge_unavailable": script(None), "method": script(None)})
add("_needs_bridge", {"call_args": [], "call_kwargs": {}},
    {"_bridge_unavailable": script(""), "method": script(None)})
add("_needs_bridge", {"call_args": [], "call_kwargs": {}}, {}, fail_closed=True)

# -- _bridge_req --
REQ_SELF = {"_bridge_port": 3000, "_http_session": {"__session__": True}}
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["get", "post"],
                    "method": "get", "path": "health", "timeout": 2, "kwargs": {}},
    {"ClientTimeout": script(None), "session.get": script({"status": 200})})
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["get", "post"],
                    "method": "post", "path": "send", "timeout": 30,
                    "kwargs": {"json": {"a": 1}}},
    {"ClientTimeout": script({"total": 30}), "session.post": script({"status": 200})})
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["get"],
                    "method": "delete", "path": "x", "timeout": 1, "kwargs": {}}, {})
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["get"],
                    "method": "get", "path": "health", "timeout": 2, "kwargs": {}},
    {"ClientTimeout": raise_("RuntimeError", "timeout boom")})
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["get"],
                    "method": "get", "path": "health", "timeout": 2, "kwargs": {}},
    {"ClientTimeout": script(None), "session.get": raise_("ConnectionRefusedError", "refused")})
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["post"],
                    "method": "post", "path": "send", "timeout": None, "kwargs": {}},
    {"ClientTimeout": script(None), "session.post": script(None)})
add("_bridge_req", {"self": REQ_SELF, "session_methods": ["get"],
                    "method": "get", "path": "health", "timeout": 2, "kwargs": {}},
    {}, fail_closed=True)

# -- _authenticated_bridge_health --
def health_io(proof="PROOF", status1=200, status2=200, json1=None, json2=None,
              proof_result="PROOF", exit1=None, exit2=None):
    if json1 is None:
        json1 = {"authProof": proof}
    if json2 is None:
        json2 = {"status": "connected"}
    return {
        "secrets.token_urlsafe": script("CHAL"),
        "session.get": [{"status": status1}, {"status": status2}],
        "resp.enter": [None, None],
        "resp.json": [json1, json2],
        "resp.exit": [exit1, exit2],
        "_bridge_auth_proof": script(proof_result),
        "_bridge_auth_headers": script({"Authorization": "Bearer tok"}),
    }


add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io())
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(proof="WRONG"))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(status1=500))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json1=raise_("ValueError", "non-JSON")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json1=raise_("TimeoutError", "body timeout")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json1=raise_("ClientError", "client boom")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json1=None))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(proof=123))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(status2=503))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json2=raise_("ValueError", "bad health body")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json2=raise_("TimeoutError", "health timeout")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    dict(health_io(), _bridge_auth_proof=raise_("RuntimeError", "proof boom")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    dict(health_io(), **{"session.get": raise_("ConnectionRefusedError", "refused")}))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    dict(health_io(), **{"resp.enter": raise_("RuntimeError", "enter boom")}))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    health_io(json1=raise_("ValueError", "non-JSON"), exit1=raise_("RuntimeError", "exit boom")))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    dict(health_io(), **{"secrets.token_urlsafe": raise_("SystemExit", "1")}))
add("_authenticated_bridge_health", {"base_url": "http://127.0.0.1:3000", "token": "tok", "timeout": 2},
    {}, fail_closed=True)

# -- _bridge_package_hash --
add("_bridge_package_hash", {"self": self_base()},
    {"_file_content_hash": script("abc123")})
add("_bridge_package_hash", {"self": self_base(_bridge_script="/h/bridge.js")},
    {"_file_content_hash": script("abc123")})
add("_bridge_package_hash", {"self": self_base(_bridge_script="bridge.js")},
    {"_file_content_hash": script("")})
add("_bridge_package_hash", {"self": self_base()},
    {"_file_content_hash": raise_("OSError", "unreadable")})
add("_bridge_package_hash", {"self": {"name": "whatsapp"}}, {})
add("_bridge_package_hash", {"self": self_base()}, {}, fail_closed=True)

# -- _bridge_env --
PASSTHROUGH = [
    "WHATSAPP_ALLOWED_USERS", "WHATSAPP_ALLOW_FROM", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY",
    "WHATSAPP_GROUP_ALLOWED_USERS", "WHATSAPP_GROUP_ALLOW_FROM", "WHATSAPP_REQUIRE_MENTION",
    "WHATSAPP_MENTION_PATTERNS", "WHATSAPP_FREE_RESPONSE_CHATS", "WHATSAPP_DEBUG",
    "WHATSAPP_FORWARD_OWNER_MESSAGES", "WHATSAPP_MAX_MESSAGE_LENGTH",
    "WHATSAPP_CHUNK_DELAY_MS", "WHATSAPP_SEND_TIMEOUT_MS",
]
BASE_ENV = {"PATH": "/bin", "WHATSAPP_REPLY_PREFIX": "old", "WHATSAPP_MODE": "inherit",
            "WHATSAPP_DEBUG": "stale"}


def env_io(reply="", mode="self-chat", pass_vals=None, base=None):
    if pass_vals is None:
        pass_vals = [""] * 14
    if base is None:
        base = BASE_ENV
    return {
        "hermes_subprocess_env": script(dict(base)),
        "with_hermes_node_path": script(dict(base)),
        "_wenv": [reply, mode] + list(pass_vals),
        "_cache_dirs": script(["/i", "/a", "/v", "/d"]),
    }


add("_bridge_env", {"self": self_base()}, env_io(reply="new"))
add("_bridge_env", {"self": self_base(_reply_prefix="p")}, env_io(reply=""))
add("_bridge_env", {"self": self_base(_reply_prefix=None)}, env_io(reply=""))
pv = [""] * 14
pv[0] = "9,8"
pv[9] = "true"
add("_bridge_env", {"self": self_base()}, env_io(pass_vals=pv))
add("_bridge_env", {"self": self_base(_allow_from=[], _group_allow_from=["g2", "g1"])},
    env_io())
add("_bridge_env", {"self": self_base()},
    dict(env_io(), **{"_wenv": ["", raise_("RuntimeError", "wenv boom")]}))
add("_bridge_env", {"self": self_base()},
    {"hermes_subprocess_env": script(dict(BASE_ENV)),
     "with_hermes_node_path": raise_("RuntimeError", "node path boom")})
add("_bridge_env", {"self": self_base()},
    dict(env_io(), _cache_dirs=raise_("OSError", "no dirs")))
add("_bridge_env", {"self": self_base()}, {}, fail_closed=True)

# -- _adopted_config_matches --
ADOPT_IO = {"read_text": script("FP"), "_bridge_env": script({"E": 1}),
            "_bridge_package_hash": script("PH"),
            "_bridge_config_fingerprint": script("FP")}
add("_adopted_config_matches", {"self": self_base()}, dict(ADOPT_IO))
add("_adopted_config_matches", {"self": self_base()},
    dict(ADOPT_IO, _bridge_config_fingerprint=script("OTHER")))
add("_adopted_config_matches", {"self": self_base()}, {"read_text": script("")})
add("_adopted_config_matches", {"self": self_base()},
    {"read_text": raise_("FileNotFoundError", "absent")})
add("_adopted_config_matches", {"self": self_base()},
    {"read_text": raise_("SystemExit", "1")})
add("_adopted_config_matches", {"self": self_base()},
    dict(ADOPT_IO, read_text=script("\ufeff FP ")))
add("_adopted_config_matches", {"self": self_base()},
    dict(ADOPT_IO, _bridge_env=raise_("RuntimeError", "env boom")))
add("_adopted_config_matches", {"self": self_base()}, {}, fail_closed=True)

# -- _bridge_died --
DIED_IO = {"proc.poll": script(None), "print": script(None),
           "_close_bridge_log": script(None), "_set_fatal_error": script(None)}
add("_bridge_died", {"self": self_base(), "detail": "bridge exited"}, dict(DIED_IO))
add("_bridge_died", {"self": self_base(), "detail": "bridge exited"},
    dict(DIED_IO, **{"proc.poll": script(78)}))
add("_bridge_died", {"self": self_base(_bridge_process=None), "detail": "external died"},
    {k: v for k, v in DIED_IO.items() if k != "proc.poll"})
add("_bridge_died", {"self": self_base(_bridge_log=None), "detail": "d"}, dict(DIED_IO))
add("_bridge_died", {"self": self_base(), "detail": "d"},
    dict(DIED_IO, print=raise_("RuntimeError", "print boom")))
add("_bridge_died", {"self": self_base(), "detail": "d"},
    dict(DIED_IO, _set_fatal_error=raise_("SystemExit", "1")))
add("_bridge_died", {"self": self_base(), "detail": "d"},
    dict(DIED_IO, **{"proc.poll": raise_("OSError", "poll boom")}))
add("_bridge_died", {"self": self_base(), "detail": "d", "code": "custom_code"}, dict(DIED_IO))
add("_bridge_died", {"self": self_base(), "detail": "d"},
    {k: v for k, v in DIED_IO.items() if k != "proc.poll"}, fail_closed=True)

# -- _close_bridge_log --
add("_close_bridge_log", {"self": self_base(_bridge_log_fh={"__fh__": True})},
    {"fh.close": script(None)})
add("_close_bridge_log", {"self": self_base(_bridge_log_fh=None)}, {})
add("_close_bridge_log", {"self": self_base(_bridge_log_fh={"__fh__": True})},
    {"fh.close": raise_("RuntimeError", "close boom")})
add("_close_bridge_log", {"self": self_base(_bridge_log_fh={"__fh__": True})},
    {"fh.close": raise_("SystemExit", "1")})
add("_close_bridge_log", {"self": self_base(_bridge_log_fh={"__fh__": True})},
    {"fh.close": raise_("KeyboardInterrupt", "")})
add("_close_bridge_log", {"self": self_base(_bridge_log_fh={"__fh__": True})},
    {}, fail_closed=True)

# -- _terminate_bridge --
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": False},
    {"_terminate_bridge_process": script(None)})
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": False},
    {"_terminate_bridge_process": raise_("ProcessLookupError", "gone"),
     "proc.terminate": script(None)})
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": True},
    {"_terminate_bridge_process": raise_("PermissionError", "denied"),
     "proc.kill": script(None)})
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": False},
    {"_terminate_bridge_process": raise_("OSError", "other")})
add("_terminate_bridge", {"self": self_base(_bridge_process=None), "force": False},
    {"_terminate_bridge_process": raise_("ProcessLookupError", "gone")})
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": True},
    {"_terminate_bridge_process": raise_("ProcessLookupError", "gone"),
     "proc.kill": raise_("RuntimeError", "kill boom")})
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": False},
    {"_terminate_bridge_process": raise_("SystemExit", "1")})
add("_terminate_bridge", {"self": self_base(_bridge_process={"pid": 7}), "force": False},
    {}, fail_closed=True)

# -- _check_managed_bridge_exit --
EXIT_IO = {"proc.poll": script(None)}
add("_check_managed_bridge_exit", {"self": self_base()}, dict(EXIT_IO))
add("_check_managed_bridge_exit", {"self": self_base(_shutting_down=True)},
    dict(EXIT_IO, **{"proc.poll": script(0), "logger.info": script(None)}))
add("_check_managed_bridge_exit",
    {"self": self_base(gateway_runner={"_stop_requested_by_signal": True})},
    dict(EXIT_IO, **{"proc.poll": script(-15), "logger.info": script(None)}))
add("_check_managed_bridge_exit", {"self": self_base()},
    dict(EXIT_IO, **{"proc.poll": script(-2), "logger.error": script(None),
                     "_bridge_died": script(False), "_notify_fatal_error": script(None)}))
add("_check_managed_bridge_exit", {"self": self_base()},
    dict(EXIT_IO, **{"proc.poll": script(1), "logger.error": script(None),
                     "_bridge_died": script(False), "_notify_fatal_error": script(None)}))
add("_check_managed_bridge_exit",
    {"self": self_base(has_fatal_error=True, fatal_error_message="prior fatal")},
    dict(EXIT_IO, **{"proc.poll": script(1)}))
add("_check_managed_bridge_exit", {"self": self_base()},
    dict(EXIT_IO, **{"proc.poll": script(0), "logger.error": script(None),
                     "_bridge_died": script(False), "_notify_fatal_error": script(None)}))
add("_check_managed_bridge_exit", {"self": self_base(_bridge_process=None)}, {})
add("_check_managed_bridge_exit", {"self": self_base()},
    dict(EXIT_IO, **{"proc.poll": script(1), "logger.error": script(None),
                     "_bridge_died": script(False),
                     "_notify_fatal_error": raise_("SystemExit", "1")}))
add("_check_managed_bridge_exit", {"self": self_base(_shutting_down=True)},
    dict(EXIT_IO, **{"proc.poll": script(-15), "logger.info": raise_("RuntimeError", "log boom")}))
add("_check_managed_bridge_exit", {"self": self_base()},
    {k: v for k, v in EXIT_IO.items() if k != "proc.poll"}, fail_closed=True)

# -- _inject_document_text --
INJ_SELF = {"name": "whatsapp"}
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_report.txt"], "body": "hi"},
    {"stat": script({"st_size": 10}), "read_text": script("hello"), "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/a.bin"], "body": "hi"}, {})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_big.txt"], "body": "hi"},
    {"stat": script({"st_size": 200000}), "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": "hi"},
    {"stat": script({"st_size": 10}), "read_text": raise_("OSError", "unreadable"),
     "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": "hi"},
    {"stat": raise_("FileNotFoundError", "gone"), "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": ""},
    {"stat": script({"st_size": 10}), "read_text": script("hello"), "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc.txt"], "body": "b"},
    {"stat": script({"st_size": 10}), "read_text": script("t"), "print": script(None)})
add("_inject_document_text",
    {"self": INJ_SELF, "cached_urls": ["/c/a.bin", "/c/doc_abc_x.txt", "/c/doc_abc_y.md"], "body": "b"},
    {"stat": [{"st_size": 10}, {"st_size": 20}], "read_text": ["one", "two"],
     "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": "b"},
    {"stat": raise_("SystemExit", "1")})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": "b"},
    {"stat": script({"st_size": 10}), "read_text": script("\ufeffbom"), "print": script(None)})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": "b"},
    {"stat": script({"st_size": 10}), "read_text": script("ok"),
     "print": raise_("RuntimeError", "print boom")})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": [], "body": "b"}, {})
add("_inject_document_text", {"self": INJ_SELF, "cached_urls": ["/c/doc_abc_x.txt"], "body": "b"},
    {}, fail_closed=True)

# -- _quoted_media --
QM_SELF = {"name": "whatsapp"}
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": ["/c/a.jpg"], "chatId": "c1"}, "raw_reply_id": None},
    {"_is_allowed_bridge_path": script(True), "print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": ["/c/a.jpg"], "chatId": "c1"}, "raw_reply_id": None},
    {"_is_allowed_bridge_path": script(False), "print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": ["a.jpg"], "chatId": "c1"}, "raw_reply_id": None},
    {"print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": [5], "chatId": "c1"}, "raw_reply_id": None},
    {"print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": [], "chatId": "c1"}, "raw_reply_id": "r1"},
    {"lookup_media": script([["/c/q.jpg", "image/png"]]), "print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "sticker",
                      "quotedMediaUrls": ["/c/s.webp"], "chatId": "c1"}, "raw_reply_id": None},
    {"_is_allowed_bridge_path": script(True), "print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": None,
                      "quotedMediaUrls": ["/c/x.bin"], "chatId": "c1"}, "raw_reply_id": None},
    {"_is_allowed_bridge_path": script(True), "print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": ["/c/a.jpg"], "chatId": "c1"}, "raw_reply_id": None},
    {"_is_allowed_bridge_path": raise_("RuntimeError", "allow boom")})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaUrls": [], "chatId": "c1"},
                      "raw_reply_id": "r1"},
    {"lookup_media": raise_("SystemExit", "1")})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "ptt",
                      "quotedMediaUrls": ["/c/v.ogg"], "chatId": "c1"}, "raw_reply_id": 77},
    {"_is_allowed_bridge_path": script(True), "print": script(None)})
add("_quoted_media", {"self": QM_SELF, "data": {"quotedMediaType": "image",
                      "quotedMediaUrls": ["/c/a.jpg"], "chatId": "c1"}, "raw_reply_id": None},
    {}, fail_closed=True)

# -- _post_bridge_message --
def post_io(status=200, json_ret=None, text_ret="err", req=None, enter=None, exit_=None):
    if json_ret is None:
        json_ret = {"messageId": "m1"}
    if req is None:
        req = {"status": status}
    return {"_bridge_req": script(req), "resp.enter": script(enter),
            "resp.json": script(json_ret), "resp.text": script(text_ret),
            "resp.exit": script(exit_)}


add("_post_bridge_message", {"path": "send", "payload": {"chatId": "c1", "message": "hi"}, "timeout": 30},
    post_io())
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    post_io(status=500))
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    {"_bridge_req": raise_("RuntimeError", "req boom")})
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    post_io(json_ret=raise_("ValueError", "bad json")))
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    post_io(json_ret=None))
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    post_io(exit_=raise_("RuntimeError", "exit boom")))
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    {"_bridge_req": raise_("SystemExit", "1")})
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    post_io(req={}))
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    post_io(enter=raise_("RuntimeError", "enter boom")))
add("_post_bridge_message", {"path": "send", "payload": {}, "timeout": 30},
    {}, fail_closed=True)

# -- _apply_yaml_config --
add("_apply_yaml_config", {"yaml_cfg": {}, "whatsapp_cfg": {"dm_policy": "open"}},
    {"_apply_yaml_bridge": script({"env": {"WHATSAPP_DM_POLICY": "open"}})})
add("_apply_yaml_config", {"yaml_cfg": None, "whatsapp_cfg": {}},
    {"_apply_yaml_bridge": script(None)})
add("_apply_yaml_config", {"yaml_cfg": {}, "whatsapp_cfg": {"allow_from": ["1", "2"]}},
    {"_apply_yaml_bridge": raise_("RuntimeError", "yaml boom")})
add("_apply_yaml_config", {"yaml_cfg": {"x": 1}, "whatsapp_cfg": {"require_mention": True}},
    {"_apply_yaml_bridge": script({"extra": {"require_mention": True}})})
add("_apply_yaml_config", {"yaml_cfg": {}, "whatsapp_cfg": {}},
    {"_apply_yaml_bridge": script({})})
add("_apply_yaml_config", {"yaml_cfg": {}, "whatsapp_cfg": {}}, {}, fail_closed=True)

# -- _may_transcribe_at_gate --
MT_SELF = {"name": "whatsapp", "config": {"extra": {"voice_mention_patterns": ["hey"]}}}
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": True, "mediaType": "ptt"}},
    {"voice_mention_gate_enabled": script(True)})
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": False, "mediaType": "ptt"}}, {})
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": True, "mediaType": "audio"}}, {})
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": True, "mediaType": None}}, {})
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": True, "mediaType": "ptt"}},
    {"voice_mention_gate_enabled": script(False)})
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": True, "mediaType": "ptt"}},
    {"voice_mention_gate_enabled": raise_("RuntimeError", "gate boom")})
add("_may_transcribe_at_gate", {"self": {"name": "whatsapp"},
                                "data": {"isGroup": True, "mediaType": "ptt"}}, {})
add("_may_transcribe_at_gate", {"self": MT_SELF, "data": {"isGroup": True, "mediaType": "ptt"}},
    {}, fail_closed=True)

# -- _log_delivery_status --
LD_SELF = {"name": "whatsapp"}
add("_log_delivery_status", {"self": LD_SELF, "data": {
    "messageId": "m1", "chatId": "c1", "status": "delivered", "timestamp": 123}},
    {"logger.info": script(None)})
add("_log_delivery_status", {"self": LD_SELF, "data": {
    "messageId": "m1", "chatId": "c1", "status": "read", "timestamp": 123, "recipientId": "r1"}},
    {"logger.info": script(None)})
add("_log_delivery_status", {"self": LD_SELF, "data": {
    "messageId": "m1", "chatId": "c1", "status": "failed", "timestamp": 123, "error": "oops"}},
    {"logger.info": script(None)})
add("_log_delivery_status", {"self": LD_SELF, "data": {
    "messageId": "m1", "chatId": "c1", "status": "failed", "timestamp": 123,
    "recipientId": "r1", "error": "oops"}},
    {"logger.info": script(None)})
add("_log_delivery_status", {"self": LD_SELF, "data": {}},
    {"logger.info": script(None)})
add("_log_delivery_status", {"self": LD_SELF, "data": {"messageId": "m1"}},
    {"logger.info": raise_("RuntimeError", "log boom")})
add("_log_delivery_status", {"self": LD_SELF, "data": {}}, {}, fail_closed=True)


# =====================================================================
# extension wave: send-family + lifecycle leaves
# =====================================================================

def srun(**over):
    """self double for send-family method targets."""
    d = dict(self_base())
    d["_running"] = True
    d["_http_session"] = {"__session__": True}
    d.update(over)
    return d


# -- _bridge_unavailable --
BU_SELF = {"_running": True, "_http_session": {"__session__": True}, "name": "whatsapp"}
add("_bridge_unavailable", {"self": dict(BU_SELF, _running=False)}, {})
add("_bridge_unavailable", {"self": BU_SELF},
    {"_check_managed_bridge_exit": script("still running")})
add("_bridge_unavailable", {"self": BU_SELF},
    {"_check_managed_bridge_exit": script(None)})
add("_bridge_unavailable", {"self": BU_SELF},
    {"_check_managed_bridge_exit": script("")})
add("_bridge_unavailable", {"self": BU_SELF},
    {"_check_managed_bridge_exit": raise_("RuntimeError", "check boom")})
add("_bridge_unavailable", {"self": BU_SELF}, {}, fail_closed=True)

# -- _report_bridge_exit --
RE_SELF = {"name": "whatsapp"}
add("_report_bridge_exit", {"self": RE_SELF},
    {"_check_managed_bridge_exit": script("bridge gone"), "print": script(None)})
add("_report_bridge_exit", {"self": RE_SELF},
    {"_check_managed_bridge_exit": script(None)})
add("_report_bridge_exit", {"self": RE_SELF},
    {"_check_managed_bridge_exit": script(""), "print": script(None)})
add("_report_bridge_exit", {"self": RE_SELF},
    {"_check_managed_bridge_exit": script(5), "print": script(None)})
add("_report_bridge_exit", {"self": RE_SELF},
    {"_check_managed_bridge_exit": raise_("SystemExit", "1")})
add("_report_bridge_exit", {"self": RE_SELF}, {}, fail_closed=True)

# -- send_image_file / send_video (pure delegates) --
for _fn, _pathk in (("send_image_file", "image_path"), ("send_video", "video_path")):
    add(_fn, {"self": srun(), "chat_id": "c1", _pathk: "/c/pic.png", "caption": "cap"},
        {"_send_media_to_bridge": script({"success": True, "message_id": "m1"})})
    add(_fn, {"self": srun(), "chat_id": "c2", _pathk: "/c/v.mp4"},
        {"_send_media_to_bridge": script({"success": False, "error": "no"})})
    add(_fn, {"self": srun(), "chat_id": "c3", _pathk: "/c/p.png", "caption": None},
        {"_send_media_to_bridge": script({"success": True, "message_id": "m2"})})
    add(_fn, {"self": srun(), "chat_id": "c4", _pathk: "/c/p.png"},
        {"_send_media_to_bridge": raise_("RuntimeError", "bridge boom")})
    add(_fn, {"self": srun(), "chat_id": "c5", _pathk: "/c/p.png"},
        {"_send_media_to_bridge": raise_("SystemExit", "1")})
    add(_fn, {"self": srun(), "chat_id": "c6", _pathk: "/c/p.png"}, {}, fail_closed=True)

# -- send_voice --
add("send_voice", {"self": srun(), "chat_id": "c1", "audio_path": "/c/a.ogg", "caption": None},
    {"_send_media_to_bridge": script({"success": True, "message_id": "m1"})})
add("send_voice", {"self": srun(), "chat_id": "c2", "audio_path": "/c/a.ogg",
                   "is_voice": True},
    {"_send_media_to_bridge": script({"success": True, "message_id": "m2"})})
add("send_voice", {"self": srun(), "chat_id": "c3", "audio_path": "/c/m.mp3",
                   "caption": "song", "reply_to": "r1", "is_voice": False},
    {"send_document": script({"success": True, "message_id": "m3"})})
add("send_voice", {"self": srun(), "chat_id": "c4", "audio_path": "/c/a.ogg",
                   "is_voice": False},
    {"send_document": raise_("RuntimeError", "doc boom")})
add("send_voice", {"self": srun(), "chat_id": "c5", "audio_path": "/c/a.ogg"},
    {"_send_media_to_bridge": raise_("SystemExit", "1")})
add("send_voice", {"self": srun(), "chat_id": "c6", "audio_path": "/c/a.ogg"}, {}, fail_closed=True)

# -- send_document --
add("send_document", {"self": srun(), "chat_id": "c1", "file_path": "/c/rep.pdf",
                      "caption": "rep", "file_name": "custom.pdf"},
    {"_send_media_to_bridge": script({"success": True, "message_id": "m1"})})
add("send_document", {"self": srun(), "chat_id": "c2", "file_path": "/c/rep.pdf"},
    {"os.path.basename": script("rep.pdf"),
     "_send_media_to_bridge": script({"success": True, "message_id": "m2"})})
add("send_document", {"self": srun(), "chat_id": "c3", "file_path": "/c/x.bin",
                      "file_name": "y.bin"},
    {"_send_media_to_bridge": script({"success": False, "error": "nope"})})
add("send_document", {"self": srun(), "chat_id": "c4", "file_path": "/c/x.bin"},
    {"os.path.basename": raise_("RuntimeError", "base boom")})
add("send_document", {"self": srun(), "chat_id": "c5", "file_path": "/c/x.bin"},
    {"os.path.basename": script("x.bin"),
     "_send_media_to_bridge": raise_("SystemExit", "1")})
add("send_document", {"self": srun(), "chat_id": "c6", "file_path": "/c/x.bin",
                      "file_name": "z.bin"}, {}, fail_closed=True)

# -- send_poll --
add("send_poll", {"self": srun(), "chat_id": "c1", "question": "pick?",
                  "options": ["a", "b"], "selectable_count": 1},
    {"to_whatsapp_jid": script("1@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m1"})})
add("send_poll", {"self": srun(), "chat_id": "c2", "question": "q2", "options": []},
    {"to_whatsapp_jid": script("2@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m2"})})
add("send_poll", {"self": srun(), "chat_id": "c3", "question": "q3", "options": None,
                  "selectable_count": 2},
    {"to_whatsapp_jid": script("3@s.whatsapp.net"),
     "_post_bridge_message": script({"success": False, "error": "bad"})})
add("send_poll", {"self": srun(), "chat_id": "c4", "question": "q4", "options": ["x"]},
    {"to_whatsapp_jid": raise_("RuntimeError", "jid boom")})
add("send_poll", {"self": srun(), "chat_id": "c5", "question": "q5", "options": ["x"]},
    {"to_whatsapp_jid": script("5@s.whatsapp.net"),
     "_post_bridge_message": raise_("SystemExit", "1")})
add("send_poll", {"self": srun(), "chat_id": "c6", "question": "q6", "options": ["x"]},
    {}, fail_closed=True)

# -- send_location --
add("send_location", {"self": srun(), "chat_id": "c1", "latitude": 12.5, "longitude": 7.25,
                      "name": "home", "address": "street"},
    {"to_whatsapp_jid": script("1@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m1"})})
add("send_location", {"self": srun(), "chat_id": "c2", "latitude": 0, "longitude": 0},
    {"to_whatsapp_jid": script("2@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m2"})})
add("send_location", {"self": srun(), "chat_id": "c3", "latitude": -3, "longitude": 4,
                      "name": "", "address": None},
    {"to_whatsapp_jid": script("3@s.whatsapp.net"),
     "_post_bridge_message": script({"success": False, "error": "nope"})})
add("send_location", {"self": srun(), "chat_id": "c4", "latitude": 1, "longitude": 2},
    {"to_whatsapp_jid": raise_("ValueError", "jid boom")})
add("send_location", {"self": srun(), "chat_id": "c5", "latitude": 1, "longitude": 2},
    {"to_whatsapp_jid": script("5@s.whatsapp.net"),
     "_post_bridge_message": raise_("SystemExit", "1")})
add("send_location", {"self": srun(), "chat_id": "c6", "latitude": 1, "longitude": 2},
    {}, fail_closed=True)

# -- edit_message --
add("edit_message", {"self": srun(), "chat_id": "c1", "message_id": "mid1", "content": "new"},
    {"to_whatsapp_jid": script("1@s.whatsapp.net"),
     "_bridge_req": script({"status": 200}), "resp.enter": script(None),
     "resp.exit": script(None)})
add("edit_message", {"self": srun(), "chat_id": "c2", "message_id": "mid2", "content": "new"},
    {"to_whatsapp_jid": script("2@s.whatsapp.net"),
     "_bridge_req": script({"status": 500}), "resp.enter": script(None),
     "resp.text": script("edit failed"), "resp.exit": script(None)})
add("edit_message", {"self": srun(), "chat_id": "c3", "message_id": "mid3", "content": "new"},
    {"to_whatsapp_jid": script("3@s.whatsapp.net"),
     "_bridge_req": raise_("RuntimeError", "req boom")})
add("edit_message", {"self": srun(), "chat_id": "c4", "message_id": "mid4", "content": "new"},
    {"to_whatsapp_jid": script("4@s.whatsapp.net"),
     "_bridge_req": script({"status": 200}), "resp.enter": script(None),
     "resp.exit": raise_("RuntimeError", "exit boom")})
add("edit_message", {"self": srun(), "chat_id": "c5", "message_id": "mid5", "content": "new"},
    {"to_whatsapp_jid": raise_("SystemExit", "1")})
add("edit_message", {"self": srun(), "chat_id": "c6", "message_id": "mid6", "content": "new"},
    {}, fail_closed=True)

# -- _send_media_to_bridge --
SM_SELF = srun()
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c1", "file_path": "/c/a.jpg",
                              "media_type": "image", "caption": "cap"},
    {"os.path.exists": script(True), "to_whatsapp_jid": script("1@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m1"}),
     "mimetypes.guess_type": script(["image/png", None]),
     "record_media_async": script(None)})
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c2", "file_path": "/c/nope.jpg",
                              "media_type": "image"},
    {"os.path.exists": script(False)})
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c3", "file_path": "/c/v.mp4",
                              "media_type": "video", "file_name": "v.mp4"},
    {"os.path.exists": script(True), "to_whatsapp_jid": script("3@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m3"}),
     "mimetypes.guess_type": script([None, None]),
     "record_media_async": script(None)})
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c4", "file_path": "/c/d.bin",
                              "media_type": "document"},
    {"os.path.exists": script(True), "to_whatsapp_jid": script("4@s.whatsapp.net"),
     "_post_bridge_message": script({"success": False, "error": "no"})})
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c5", "file_path": "/c/a.jpg",
                              "media_type": "image"},
    {"os.path.exists": raise_("RuntimeError", "exists boom")})
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c6", "file_path": "/c/a.jpg",
                              "media_type": "image"},
    {"os.path.exists": script(True), "to_whatsapp_jid": script("6@s.whatsapp.net"),
     "_post_bridge_message": script({"success": True, "message_id": "m6"}),
     "mimetypes.guess_type": script(["image/jpeg", None]),
     "record_media_async": raise_("RuntimeError", "record boom")})
add("_send_media_to_bridge", {"self": SM_SELF, "chat_id": "c7", "file_path": "/c/a.jpg",
                              "media_type": "image"}, {}, fail_closed=True)

# -- _send_read_receipt --
SRR_SELF = srun(_send_read_receipts=True)
add("_send_read_receipt", {"self": SRR_SELF, "data": {"readReceiptKey": {"id": "k1"}}},
    {"_bridge_req": script({"status": 200}), "resp.enter": script(None),
     "resp.exit": script(None)})
add("_send_read_receipt", {"self": SRR_SELF, "data": {"readReceiptKey": {"id": "k1"}}},
    {"_bridge_req": script({"status": 500}), "resp.enter": script(None),
     "resp.exit": script(None), "logger.warning": script(None)})
add("_send_read_receipt", {"self": srun(_send_read_receipts=False),
                           "data": {"readReceiptKey": {"id": "k1"}}}, {})
add("_send_read_receipt", {"self": SRR_SELF, "data": {}}, {})
add("_send_read_receipt", {"self": SRR_SELF, "data": {"readReceiptKey": "not-a-dict"}}, {})
add("_send_read_receipt", {"self": SRR_SELF, "data": {"readReceiptKey": {"id": "k1"}}},
    {"_bridge_req": raise_("RuntimeError", "req boom"),
     "logger.warning": script(None)})
add("_send_read_receipt", {"self": SRR_SELF, "data": {"readReceiptKey": {"id": "k1"}}},
    {"_bridge_req": raise_("SystemExit", "1")})
add("_send_read_receipt", {"self": SRR_SELF, "data": {"readReceiptKey": {"id": "k1"}}},
    {}, fail_closed=True)

# -- get_chat_info --
GCI_SELF = srun()
add("get_chat_info", {"self": srun(_running=False), "chat_id": "c1"}, {})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c2"},
    {"_check_managed_bridge_exit": script("dead")})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c3"},
    {"_check_managed_bridge_exit": script(None),
     "to_whatsapp_jid": script("3@s.whatsapp.net"),
     "_bridge_req": script({"status": 200}), "resp.enter": script(None),
     "resp.json": script({"name": "Group", "isGroup": True, "participants": ["a", "b"]}),
     "resp.exit": script(None)})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c4"},
    {"_check_managed_bridge_exit": script(None),
     "to_whatsapp_jid": script("4@s.whatsapp.net"),
     "_bridge_req": script({"status": 404}), "resp.enter": script(None),
     "resp.exit": script(None)})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c5"},
    {"_check_managed_bridge_exit": script(None),
     "to_whatsapp_jid": script("5@s.whatsapp.net"),
     "_bridge_req": raise_("RuntimeError", "req boom"),
     "logger.debug": script(None)})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c6"},
    {"_check_managed_bridge_exit": script(None),
     "to_whatsapp_jid": script("6@s.whatsapp.net"),
     "_bridge_req": script({"status": 200}), "resp.enter": script(None),
     "resp.json": script({"isGroup": False}), "resp.exit": script(None)})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c7"},
    {"_check_managed_bridge_exit": raise_("SystemExit", "1")})
add("get_chat_info", {"self": GCI_SELF, "chat_id": "c8"}, {}, fail_closed=True)

# -- _bridge_url (extension wave 2) --
add("_bridge_url", {"self": self_base(_bridge_port=3000), "path": "typing"})
add("_bridge_url", {"self": self_base(_bridge_port=45678), "path": "messages"})
add("_bridge_url", {"self": self_base(_bridge_port=80), "path": "chat/1@s.whatsapp.net"})
add("_bridge_url", {"self": self_base(_bridge_port=0), "path": ""})
add("_bridge_url", {"self": self_base(_bridge_port=-1), "path": "send"})
add("_bridge_url", {"self": self_base(_bridge_port=3001), "path": "health"})

# -- send_clarify (extension wave 2b) --
SC_SELF = self_base()
add("send_clarify", {"self": SC_SELF, "chat_id": "c1", "question": " Pick one ", "choices": [" a ", "b", "", "  "],
                     "clarify_id": "q1", "session_key": "s1", "metadata": None},
    {"send_poll": script({"success": True, "message_id": "m1"})})
add("send_clarify", {"self": SC_SELF, "chat_id": "c2", "question": "Q", "choices": ["a", "b"],
                     "clarify_id": "q2", "session_key": "s2", "metadata": None},
    {"send_poll": script({"success": False, "error": "poll failed"}),
     "logger.warning": script(None),
     "_base_send_clarify": script({"success": True, "message_id": "base1"})})
add("send_clarify", {"self": SC_SELF, "chat_id": "c3", "question": "Q", "choices": ["a"],
                     "clarify_id": "q3", "session_key": "s3", "metadata": {"x": 1}},
    {"_base_send_clarify": script({"success": True, "message_id": "base2"})})
add("send_clarify", {"self": SC_SELF, "chat_id": "c4", "question": None, "choices": [str(i) for i in range(13)],
                     "clarify_id": "q4", "session_key": "s4", "metadata": None},
    {"_base_send_clarify": script({"success": False, "error": "base no"})})
add("send_clarify", {"self": SC_SELF, "chat_id": "c5", "question": 42, "choices": [1, 2, None],
                     "clarify_id": "q5", "session_key": "s5", "metadata": None},
    {"send_poll": script({"success": True, "message_id": "m5"})})
add("send_clarify", {"self": SC_SELF, "chat_id": "c6", "question": "Q", "choices": ["a", "b"],
                     "clarify_id": "q6", "session_key": "s6", "metadata": None},
    {"send_poll": raise_("RuntimeError", "poll boom")})
add("send_clarify", {"self": SC_SELF, "chat_id": "c7", "question": "Q", "choices": ["a", "b"],
                     "clarify_id": "q7", "session_key": "s7", "metadata": None}, {}, fail_closed=True)

# -- send_image (extension wave 2b) --
SI_SELF = self_base()
add("send_image", {"self": SI_SELF, "chat_id": "c1", "image_url": "http://x/a.jpg", "caption": "cap",
                   "reply_to": None, "metadata": None},
    {"cache_image_from_url": script("/cache/a.jpg"),
     "_send_media_to_bridge": script({"success": True, "message_id": "im1"})})
add("send_image", {"self": SI_SELF, "chat_id": "c2", "image_url": "bad", "caption": None,
                   "reply_to": "r1", "metadata": {"m": 2}},
    {"cache_image_from_url": raise_("ValueError", "bad url"),
     "_base_send_image": script({"success": False, "error": "fallback"})})
add("send_image", {"self": SI_SELF, "chat_id": "c3", "image_url": "http://x/b.jpg", "caption": "c",
                   "reply_to": None, "metadata": None},
    {"cache_image_from_url": script("/cache/b.jpg"),
     "_send_media_to_bridge": raise_("OSError", "media boom"),
     "_base_send_image": script({"success": True, "message_id": "ib3"})})
add("send_image", {"self": SI_SELF, "chat_id": "c4", "image_url": "http://x/c.jpg", "caption": None,
                   "reply_to": None, "metadata": None},
    {"cache_image_from_url": raise_("SystemExit", "1")})
add("send_image", {"self": SI_SELF, "chat_id": "c5", "image_url": "http://x/d.jpg", "caption": None,
                   "reply_to": None, "metadata": None}, {}, fail_closed=True)

# -- _wait_for_bridge (extension wave 2b) --
WF_SELF = self_base()
add("_wait_for_bridge", {"self": WF_SELF},
    {"_poll_bridge_health": script([True, True, {"status": "connected"}])})
add("_wait_for_bridge", {"self": WF_SELF},
    {"_poll_bridge_health": script([False, False, {}])})
add("_wait_for_bridge", {"self": WF_SELF},
    {"_poll_bridge_health": script([None, False, {}]),
     "_bridge_died": script("Bridge HTTP server did not start in 15s")})
add("_wait_for_bridge", {"self": WF_SELF},
    {"_poll_bridge_health": [script([True, True, {"status": "connecting"}]),
                            script([True, True, {"status": "connected"}])],
     "print": script(None)})
add("_wait_for_bridge", {"self": WF_SELF},
    {"_poll_bridge_health": [script([True, True, {"status": "connecting"}]),
                            script([False, True, {"status": "connecting"}])],
     "print": script(None)})
add("_wait_for_bridge", {"self": WF_SELF},
    {"_poll_bridge_health": raise_("SystemExit", "1")})
add("_wait_for_bridge", {"self": WF_SELF}, {}, fail_closed=True)

# -- _attach_to_bridge (extension wave 2c) --
AT_SELF = self_base()
AT_MP = {"pid": 5555}
add("_attach_to_bridge", {"self": AT_SELF, "managed_process": AT_MP},
    {"_bridge_auth_headers": script({"Authorization": "Bearer tok"}),
     "aiohttp.ClientSession": script({"__session__": True, "headers": {"Authorization": "Bearer tok"}}),
     "_poll_messages": script("pollbody"),
     "asyncio.create_task": script({"handle": "t1"})})
add("_attach_to_bridge", {"self": AT_SELF, "managed_process": AT_MP},
    {"_bridge_auth_headers": raise_("RuntimeError", "hdr boom")})
add("_attach_to_bridge", {"self": AT_SELF, "managed_process": AT_MP},
    {"_bridge_auth_headers": script({"Authorization": "Bearer tok"}),
     "aiohttp.ClientSession": raise_("OSError", "session boom")})
add("_attach_to_bridge", {"self": AT_SELF, "managed_process": AT_MP},
    {"_bridge_auth_headers": script({"Authorization": "Bearer tok"}),
     "aiohttp.ClientSession": script({"__session__": True}),
     "_poll_messages": raise_("RuntimeError", "poll boom")})
add("_attach_to_bridge", {"self": AT_SELF, "managed_process": AT_MP},
    {"_bridge_auth_headers": script({"Authorization": "Bearer tok"}),
     "aiohttp.ClientSession": script({"__session__": True}),
     "_poll_messages": script("pollbody"),
     "asyncio.create_task": raise_("SystemExit", "1")})
add("_attach_to_bridge", {"self": AT_SELF, "managed_process": AT_MP}, {}, fail_closed=True)

# -- send_typing (extension wave 2c) --
ST_SELF = self_base()
ST_IO = {"_bridge_unavailable": script(False),
         "to_whatsapp_jid": script("1@s.whatsapp.net"),
         "ClientTimeout": script({"total": 5}),
         "session.post": script({"status": 200}),
         "resp.enter": script(None), "resp.exit": script(None)}
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c1", "metadata": None}, ST_IO)
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c2", "metadata": {"m": 1}},
    {"_bridge_unavailable": script(True)})
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c3", "metadata": None},
    {"_bridge_unavailable": script(False), "to_whatsapp_jid": raise_("ValueError", "bad jid")})
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c4", "metadata": None},
    {"_bridge_unavailable": script(False), "to_whatsapp_jid": script("j"),
     "ClientTimeout": raise_("OSError", "timeout boom")})
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c5", "metadata": None},
    {"_bridge_unavailable": script(False), "to_whatsapp_jid": script("j"),
     "ClientTimeout": script({"total": 5}), "session.post": raise_("OSError", "post boom")})
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c6", "metadata": None},
    {"_bridge_unavailable": script(False), "to_whatsapp_jid": script("j"),
     "ClientTimeout": script({"total": 5}), "session.post": script({"status": 200}),
     "resp.enter": raise_("RuntimeError", "enter boom"), "resp.exit": script(None)})
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c7", "metadata": None},
    {"_bridge_unavailable": script(False), "to_whatsapp_jid": script("j"),
     "ClientTimeout": script({"total": 5}), "session.post": raise_("SystemExit", "1")})
add("send_typing", {"self": ST_SELF, "session_methods": ["post"], "chat_id": "c8", "metadata": None}, {}, fail_closed=True)

# -- _probe_bridge_health (extension wave 2c) --
PB_SELF = self_base()
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": script(None), "ClientTimeout": script({"total": 2}),
     "_authenticated_bridge_health": script([True, {"status": "connected"}]),
     "session.exit": script(None)})
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": script(None), "ClientTimeout": script({"total": 2}),
     "_authenticated_bridge_health": script([False, None]),
     "session.exit": script(None)})
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": script(None), "ClientTimeout": script({"total": 2}),
     "_authenticated_bridge_health": raise_("ClientError", "health boom"),
     "session.exit": script(None)})
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": script(None), "ClientTimeout": raise_("ValueError", "timeout boom"),
     "session.exit": script(None)})
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": raise_("OSError", "enter boom")})
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": script(None), "ClientTimeout": script({"total": 2}),
     "_authenticated_bridge_health": raise_("ClientError", "health boom"),
     "session.exit": script(True)})
add("_probe_bridge_health", {"self": PB_SELF},
    {"aiohttp.ClientSession": script({"__session__": True}),
     "session.enter": script(None), "ClientTimeout": script({"total": 2}),
     "_authenticated_bridge_health": script([True, {}]),
     "session.exit": raise_("SystemExit", "1")})
add("_probe_bridge_health", {"self": PB_SELF}, {}, fail_closed=True)

# -- send (extension wave 2d) --
SEND_SELF = self_base()
add("send", {"self": SEND_SELF, "chat_id": "c1", "content": "  ", "reply_to": None, "metadata": None}, {})
add("send", {"self": SEND_SELF, "chat_id": "c2", "content": "hello", "reply_to": None, "metadata": None},
    {"to_whatsapp_jid": script("2@s.whatsapp.net"), "format_message": script("hello"),
     "_outgoing_chunk_limit": script(4096), "truncate_message": script(["hello"]),
     "_post_bridge_message": script({"success": True, "message_id": 100})})
add("send", {"self": SEND_SELF, "chat_id": "c3", "content": "a\nb", "reply_to": "r1", "metadata": {"m": 1}},
    {"to_whatsapp_jid": script("3@s.whatsapp.net"), "format_message": script("a\nb"),
     "_outgoing_chunk_limit": script(5), "truncate_message": script(["a", "b"]),
     "_post_bridge_message": [script({"success": True, "message_id": "m1"}),
                              script({"success": True, "message_id": "m2"})],
     "asyncio.sleep": script(None)})
add("send", {"self": SEND_SELF, "chat_id": "c4", "content": "hello", "reply_to": None, "metadata": None},
    {"to_whatsapp_jid": script("4@s.whatsapp.net"), "format_message": script("hello"),
     "_outgoing_chunk_limit": script(4096), "truncate_message": script(["hello"]),
     "_post_bridge_message": script({"success": False, "error": "post failed"})})
add("send", {"self": SEND_SELF, "chat_id": "c5", "content": "hello", "reply_to": None, "metadata": None},
    {"to_whatsapp_jid": script("5@s.whatsapp.net"), "format_message": raise_("ValueError", "fmt boom")})
add("send", {"self": SEND_SELF, "chat_id": "c6", "content": "hello", "reply_to": None, "metadata": None},
    {"to_whatsapp_jid": raise_("SystemExit", "1")})
add("send", {"self": SEND_SELF, "chat_id": "c7", "content": "hello", "reply_to": "r1", "metadata": None},
    {"to_whatsapp_jid": script("7@s.whatsapp.net"), "format_message": script("hello"),
     "_outgoing_chunk_limit": script(2), "truncate_message": script(["a", "b"]),
     "_post_bridge_message": [script({"success": True, "message_id": "x1"}),
                              raise_("RuntimeError", "second post boom")],
     "asyncio.sleep": script(None)})

# -- interactive_setup (extension wave 2d) --
add("interactive_setup", {},
    {"print_header": script(None), "print_info": script(None), "get_env_value": script(None),
     "prompt_yes_no": script(True), "print_success": script(None),
     "prompt": [script("11 @x.com 22"), script(" c1 ")],
     "save_env_value": script(None), "remove_env_value": script(False)})
add("interactive_setup", {},
    {"print_header": script(None), "print_info": script(None), "get_env_value": script("true"),
     "prompt_yes_no": [script(True), script(False)]})
add("interactive_setup", {},
    {"print_header": script(None), "print_info": script(None), "get_env_value": script("YES"),
     "prompt_yes_no": [script(True), script(True)], "print_success": script(None),
     "prompt": [script(""), script("")], "save_env_value": script(None),
     "remove_env_value": script(True)})
add("interactive_setup", {},
    {"print_header": script(None), "print_info": script(None), "get_env_value": script("false"),
     "prompt_yes_no": script(False), "save_env_value": script(None)})
add("interactive_setup", {},
    {"print_header": script(None), "print_info": script(None), "get_env_value": raise_("OSError", "env boom")})
add("interactive_setup", {},
    {"print_header": script(None), "print_info": script(None), "get_env_value": script(None),
     "prompt_yes_no": script(True), "print_success": script(None),
     "prompt": [script("u1"), script("home1")], "save_env_value": script(None)})
add("interactive_setup", {}, {}, fail_closed=True)

# -- _preflight (extension wave 2e) --
PF_SELF = self_base()
add("_preflight", {"self": PF_SELF},
    {"check_whatsapp_requirements": script(True), "exists": [script(True), script(True)],
     "ensure_aiohttp": script(None), "logger.info": script(None)})
add("_preflight", {"self": PF_SELF},
    {"check_whatsapp_requirements": script(False), "logger.warning": script(None),
     "_set_fatal_error": script(None)})
add("_preflight", {"self": PF_SELF},
    {"check_whatsapp_requirements": script(True), "exists": [script(False)],
     "logger.warning": script(None), "_set_fatal_error": script(None)})
add("_preflight", {"self": PF_SELF},
    {"check_whatsapp_requirements": script(True), "exists": [script(True), script(False)],
     "logger.warning": script(None), "_set_fatal_error": script(None)})
add("_preflight", {"self": PF_SELF},
    {"check_whatsapp_requirements": script(True), "exists": [script(True), script(True)],
     "ensure_aiohttp": script(["no aiohttp", True]), "logger.warning": script(None),
     "_set_fatal_error": script(None)})
add("_preflight", {"self": PF_SELF},
    {"check_whatsapp_requirements": raise_("SystemExit", "1")})
add("_preflight", {"self": PF_SELF}, {}, fail_closed=True)

# -- _unidentified_holder_remains (extension wave 2e) --
UH_SELF = self_base(_bridge_probe_timed_out=False)
add("_unidentified_holder_remains", {"self": UH_SELF},
    {"asyncio.sleep": script(None), "_port_listener_pids": script([])})
add("_unidentified_holder_remains", {"self": self_base(_bridge_probe_timed_out=False)},
    {"asyncio.sleep": script(None), "_port_listener_pids": script([4242]),
     "_set_fatal_error": script(None)})
add("_unidentified_holder_remains", {"self": self_base(_bridge_probe_timed_out=True)},
    {"asyncio.sleep": script(None), "_port_listener_pids": script([9]),
     "_set_fatal_error": script(None)})
add("_unidentified_holder_remains", {"self": UH_SELF},
    {"asyncio.sleep": script(None), "_port_listener_pids": raise_("OSError", "no netstat"),
     "_set_fatal_error": script(None)})
add("_unidentified_holder_remains", {"self": UH_SELF},
    {"asyncio.sleep": script(None), "_port_listener_pids": raise_("SystemExit", "1")})
add("_unidentified_holder_remains", {"self": UH_SELF}, {}, fail_closed=True)

# -- _wait_bridge_port_free (extension wave 2e) --
WP_SELF = self_base()
add("_wait_bridge_port_free", {"self": WP_SELF},
    {"time.monotonic": script(100.0), "port_is_free": script(True)})
add("_wait_bridge_port_free", {"self": WP_SELF},
    {"time.monotonic": [script(100.0), script(100.5)],
     "port_is_free": [script(False), script(True)], "asyncio.sleep": script(None)})
add("_wait_bridge_port_free", {"self": WP_SELF},
    {"time.monotonic": [script(100.0), script(115.0)],
     "port_is_free": script(False), "_port_listener_pids": script([77, 88]),
     "_set_fatal_error": script(None)})
add("_wait_bridge_port_free", {"self": WP_SELF},
    {"time.monotonic": [script(100.0), script(115.0)],
     "port_is_free": script(False), "_port_listener_pids": raise_("OSError", "no lsof"),
     "_set_fatal_error": script(None)})
add("_wait_bridge_port_free", {"self": WP_SELF},
    {"time.monotonic": raise_("SystemExit", "1")})
add("_wait_bridge_port_free", {"self": WP_SELF}, {}, fail_closed=True)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            # ensure_ascii keeps the fixture file pure ASCII (the BOM-test
            # strings ride \ufeff escapes; libjson parses \uXXXX).
            f.write(json.dumps(rec, ensure_ascii=True) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for func in sorted(counts):
        print(f"  {func}: {counts[func]}")
    thin = [f for f, c in counts.items() if c < 5]
    if thin:
        raise SystemExit(f"THIN FUNCS (<5 cases): {thin}")


if __name__ == "__main__":
    main()