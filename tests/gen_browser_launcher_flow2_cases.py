#!/usr/bin/env python3
"""Generator for tests/fixtures/browser_launcher_flow2.jsonl.

Emits >=5 cases per ported function of tools/browser_launcher.py (the UNCLAIMED
remainder ported as src/tools/browser_launcher_flow2.c). Each case is
{"func": <name>, "in": {...args..., "state": {browsers,procs,launching},
"io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam bl2_call and the oracle):
  absent key            -> None
  {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  any other JSON value  -> reused for every call

Domain (documented in the port header): POSIX-spelled paths (no . / .. / double
slash); _load_record texts are strict JSON or clearly-invalid JSON; pids integral
or null; browser dicts carry the full LaunchedBrowser field set; proc doubles are
{"pid": int}; state launching keys are str(pid). browser_ref aliases a state
browser node to model Python `is` identity.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "browser_launcher_flow2.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def br(**over):
    base = {
        "key": "k1", "pid": 100, "start_time": 5,
        "cdp_url": "ws://127.0.0.1:9222/devtools/browser/abc",
        "http_url": "http://127.0.0.1:9222", "user_data_dir": "/prof",
        "owned": True, "ephemeral": False, "home_key": "/home",
        "record_name": None, "foreign": False,
    }
    base.update(over)
    return base


def state(browsers=None, procs=None, launching=None):
    return {"browsers": browsers or {}, "procs": procs or {}, "launching": launching or {}}


PRIV = {"st_mode": 0o40700, "st_uid": 1000}
NOTPRIV = {"st_mode": 0o40777, "st_uid": 1000}
NOTDIR = {"st_mode": 0o100644, "st_uid": 1000}
TAG = "-nabcdef123456"
# common scripts for an ephemeral private record-path resolution
EPH_IO = io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG)


# ── _ephemeral_root ─────────────────────────────────────────────────────────
case("_ephemeral_root", home="/home")
case("_ephemeral_root", home="/h2")
case("_ephemeral_root", home="")
case("_ephemeral_root", home="/a/b")
case("_ephemeral_root", home="rel")

# ── _records_dir ────────────────────────────────────────────────────────────
case("_records_dir", home="/home")
case("_records_dir", home="/h2")
case("_records_dir", home="")
case("_records_dir", home="/a/b")
case("_records_dir", home="rel")

# ── _profile_digest ─────────────────────────────────────────────────────────
case("_profile_digest", user_data_dir="/prof", io=io(realpath="/real/prof"))
case("_profile_digest", user_data_dir="/prof/", io=io(realpath="/real/prof"))
case("_profile_digest", user_data_dir="/a", io=io(realpath=raise_("OSError", "no")))
case("_profile_digest", user_data_dir="/b", io=io(realpath="/b"))
case("_profile_digest", user_data_dir="/c", io=io(realpath="/real/c"))

# ── _same_profile ───────────────────────────────────────────────────────────
case("_same_profile", recorded="/prof", user_data_dir="/prof",
     io=io(realpath=seq("/real/prof", "/real/prof")))
case("_same_profile", recorded="/prof", user_data_dir="/other",
     io=io(realpath=seq("/real/prof", "/real/other")))
case("_same_profile", recorded=None, user_data_dir="/prof")
case("_same_profile", recorded=123, user_data_dir="/prof")
case("_same_profile", recorded="/prof", user_data_dir="/prof", io=io(realpath="/real/prof"))
case("_same_profile", recorded="/prof", user_data_dir="/prof",
     io=io(realpath=raise_("OSError", "x")))

# ── _devtools_id ────────────────────────────────────────────────────────────
case("_devtools_id", browser=br(cdp_url="ws://127.0.0.1:9222/devtools/browser/abc"))
case("_devtools_id", browser=br(cdp_url="ws://127.0.0.1:9222"))
case("_devtools_id", browser=br(cdp_url=""))
case("_devtools_id", browser=br(cdp_url="ws://x/devtools/browser/xyz"))
case("_devtools_id", browser=br(cdp_url="/devtools/browser/atstart"))

# ── _record_name ────────────────────────────────────────────────────────────
case("_record_name", browser=br(record_name="custom.json"))
case("_record_name", browser=br(record_name=None, pid=100),
     io=io(realpath="/real/prof", ns_tag=TAG))
case("_record_name", browser=br(record_name="", pid=200),
     io=io(realpath="/real/prof", ns_tag=""))
case("_record_name", browser=br(record_name=None, pid=None),
     io=io(realpath="/real/prof", ns_tag=TAG))
case("_record_name", browser=br(record_name=None), io=io(realpath=raise_("OSError", "x")))
case("_record_name", browser=br(record_name=None),
     io=io(realpath="/real/prof", ns_tag=raise_("RuntimeError", "tag")))

# ── _shared_profiles_root ───────────────────────────────────────────────────
case("_shared_profiles_root", io=io(_get_lock_dir="/lock"))
case("_shared_profiles_root", io=io(_get_lock_dir="/var/lock"))
case("_shared_profiles_root", io=io(_get_lock_dir=raise_("OSError", "x")))
case("_shared_profiles_root", io=io(_get_lock_dir="/a"))
case("_shared_profiles_root", io=io(_get_lock_dir="/b/c"))

# ── _is_private_dir ─────────────────────────────────────────────────────────
case("_is_private_dir", path="/d", env={"has_getuid": True}, io=io(lstat=PRIV, getuid=1000))
case("_is_private_dir", path="/d", env={"has_getuid": True}, io=io(lstat=NOTPRIV, getuid=1000))
case("_is_private_dir", path="/d", env={"has_getuid": True}, io=io(lstat=NOTDIR, getuid=1000))
case("_is_private_dir", path="/d", env={"has_getuid": True}, io=io(lstat=raise_("OSError", "x")))
case("_is_private_dir", path="/d", env={"has_getuid": False}, io=io(lstat=PRIV))
case("_is_private_dir", path="/d", env={"has_getuid": True},
     io=io(lstat={"st_mode": 0o40700, "st_uid": 999}, getuid=1000))
case("_is_private_dir", path="/d", env={"has_getuid": True}, io=io(lstat=raise_("ValueError", "x")))

# ── _private_dir ────────────────────────────────────────────────────────────
case("_private_dir", path="/d", env={"has_getuid": True},
     io=io(mkdir=seq(None, None), lstat=PRIV, getuid=1000))
case("_private_dir", path="/d", env={"has_getuid": True},
     io=io(mkdir=seq(None, raise_("FileExistsError", "exists")), lstat=PRIV, getuid=1000))
case("_private_dir", path="/d", env={"has_getuid": True},
     io=io(mkdir=seq(None, raise_("PermissionError", "no")), lstat=PRIV, getuid=1000))
case("_private_dir", path="/d", env={"has_getuid": True},
     io=io(mkdir=seq(None, None), lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))
case("_private_dir", path="/d", env={"has_getuid": True},
     io=io(mkdir=seq(raise_("OSError", "parent"), None)))
case("_private_dir", path="/d", env={"has_getuid": False},
     io=io(mkdir=seq(None, None), lstat=PRIV))

# ── _shared_state_dir ───────────────────────────────────────────────────────
case("_shared_state_dir", user_data_dir="/prof", env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof"))
case("_shared_state_dir", user_data_dir="/prof", env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))
case("_shared_state_dir", user_data_dir="/prof", io=io(_get_lock_dir=raise_("OSError", "x")))
case("_shared_state_dir", user_data_dir="/prof", env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=seq(PRIV, NOTPRIV), getuid=1000,
           realpath="/real/prof", **{"logger.debug": None}))
case("_shared_state_dir", user_data_dir="/prof", env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000,
           realpath=raise_("OSError", "x")))

# ── _state_dir ──────────────────────────────────────────────────────────────
case("_state_dir", browser=br(ephemeral=True, home_key="/home"), env={"has_getuid": True}, io=EPH_IO)
case("_state_dir", browser=br(ephemeral=False), env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof"))
case("_state_dir", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))
case("_state_dir", browser=br(ephemeral=False), io=io(_get_lock_dir=raise_("OSError", "x")))
case("_state_dir", browser=br(ephemeral=True, home_key="/h2"), env={"has_getuid": True}, io=EPH_IO)

# ── _record_path ────────────────────────────────────────────────────────────
case("_record_path", browser=br(ephemeral=True, record_name=None), env={"has_getuid": True}, io=EPH_IO)
case("_record_path", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))
case("_record_path", browser=br(ephemeral=True, record_name="custom.json"), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000))
case("_record_path", browser=br(ephemeral=False, record_name=None), env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG))
case("_record_path", browser=br(ephemeral=True, record_name=None), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath=raise_("OSError", "x")))

# ── _make_browser ───────────────────────────────────────────────────────────
case("_make_browser", key="k1", port=9222, path="/devtools/browser/abc", pid=100, start_time=5,
     user_data_dir="/prof", owned=True, ephemeral=False, home_key="/home")
case("_make_browser", key="k2", port=0, path="", pid=200, start_time=None,
     user_data_dir="/p2", owned=True, ephemeral=True, home_key="/h")
case("_make_browser", key="k3", port=1234, path="/devtools/browser/x", pid=None, start_time=None,
     user_data_dir="/p3", owned=False, ephemeral=False, home_key="/h")
case("_make_browser", key="k4", port=9222, path="/devtools/browser/abc", pid=100, start_time=5,
     user_data_dir="/prof", owned=False, ephemeral=True, home_key="/home")
case("_make_browser", key="k5", port=8080, path="/devtools/browser/y", pid=300, start_time=9,
     user_data_dir="/p5", owned=True, ephemeral=False, home_key="/h5")

# ── _record_is ──────────────────────────────────────────────────────────────
case("_record_is", record={"pid": 100, "user_data_dir": "/prof", "start_time": 5},
     browser=br(pid=100, start_time=5), io=io(realpath="/real/prof"))
case("_record_is", record={"pid": 999, "user_data_dir": "/prof", "start_time": 5}, browser=br())
case("_record_is", record={"pid": 100, "user_data_dir": "/other", "start_time": 5},
     browser=br(), io=io(realpath=seq("/real/other", "/real/prof")))
case("_record_is", record={"pid": 100, "user_data_dir": "/prof", "devtools": "/devtools/browser/abc"},
     browser=br(foreign=True), io=io(realpath="/real/prof", same_namespace=False))
case("_record_is", record={"pid": 100, "user_data_dir": "/prof", "devtools": "/devtools/browser/abc"},
     browser=br(foreign=True), io=io(realpath="/real/prof", same_namespace=True))
case("_record_is", record={"pid": 100, "user_data_dir": "/prof", "devtools": None},
     browser=br(foreign=True), io=io(realpath="/real/prof", same_namespace=False))
case("_record_is", record={"pid": 100, "user_data_dir": "/prof", "start_time": 999},
     browser=br(start_time=5), io=io(realpath="/real/prof"))

# ── _owner_alive ────────────────────────────────────────────────────────────
case("_owner_alive", record={"owner_pid": 100}, io=io(same_namespace=False))
case("_owner_alive", record={"owner_pid": 100, "owner_start_time": None},
     io=io(same_namespace=True, _pid_alive=True))
case("_owner_alive", record={"owner_pid": 100}, io=io(same_namespace=True, _pid_alive=False))
case("_owner_alive", record={"owner_pid": "x"}, io=io(same_namespace=True))
case("_owner_alive", record={"owner_pid": 100, "owner_start_time": 5},
     io=io(same_namespace=True, _pid_alive=True, _start_time=5))
case("_owner_alive", record={"owner_pid": 100, "owner_start_time": 5},
     io=io(same_namespace=True, _pid_alive=True, _start_time=999))
case("_owner_alive", record={"owner_pid": 100}, io=io(same_namespace=raise_("OSError", "x")))

# ── _load_record ────────────────────────────────────────────────────────────
case("_load_record", path="/r.json", io=io(read_text='{"pid":100}', unlink=None))
case("_load_record", path="/r.json", io=io(read_text="[1,2]", unlink=None))
case("_load_record", path="/r.json", io=io(read_text="{bad", unlink=None))
case("_load_record", path="/r.json", io=io(read_text=raise_("OSError", "x"), unlink=None))
case("_load_record", path="/r.json", io=io(read_text='\ufeff{"pid":1}', unlink=None))
case("_load_record", path="/r.json", io=io(read_text='{"pid":1}', unlink=raise_("OSError", "no")))

# ── _write_record ───────────────────────────────────────────────────────────
case("_write_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(getpid=seq(1000, 1000), _start_time=5, ns_identity="ns1", mkdir=None,
           lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG, atomic_json_write=None))
case("_write_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(getpid=seq(1000, 1000), _start_time=5, ns_identity="ns1",
           mkdir=seq(raise_("OSError", "no"), None), **{"logger.debug": None}))
case("_write_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(getpid=seq(1000, 1000), _start_time=5, ns_identity="ns1", mkdir=None,
           lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           atomic_json_write=raise_("OSError", "write"), **{"logger.debug": None}))
case("_write_record", browser=br(ephemeral=True), io=io(getpid=raise_("OSError", "x")))
case("_write_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(getpid=seq(1000, 1000), _start_time=5, ns_identity="ns1", mkdir=None,
           lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))

# ── _remove_record ──────────────────────────────────────────────────────────
case("_remove_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG, unlink=None))
case("_remove_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))
case("_remove_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=raise_("OSError", "no"), **{"logger.debug": None}))
case("_remove_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=seq(raise_("OSError", "no"), None), **{"logger.debug": None}))
case("_remove_record", browser=br(ephemeral=False), env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, unlink=None))

# ── _adopt_record ───────────────────────────────────────────────────────────
case("_adopt_record", browser=br(ephemeral=True, pid=100, start_time=5), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           exists=True, read_text='{"pid":100,"user_data_dir":"/prof","start_time":5,"owner_pid":9}',
           same_namespace=True, _pid_alive=False, getpid=1000, _start_time=5, atomic_json_write=None))
case("_adopt_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=NOTPRIV, getuid=1000, **{"logger.debug": None}))
case("_adopt_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG, exists=False))
case("_adopt_record", browser=br(ephemeral=True), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           exists=True, read_text="{bad", unlink=None))
case("_adopt_record", browser=br(ephemeral=True, pid=100), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           exists=True, read_text='{"pid":999,"user_data_dir":"/prof","start_time":5}'))
case("_adopt_record", browser=br(ephemeral=True, pid=100, start_time=5), env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           exists=True, read_text='{"pid":100,"user_data_dir":"/prof","start_time":5,"owner_pid":9}',
           same_namespace=True, _pid_alive=True, _start_time=5))

# ── _alive ──────────────────────────────────────────────────────────────────
case("_alive", browser=br(key="k1", owned=True), state=state(procs={"k1": {"pid": 100}}),
     io=io(**{"proc.poll": None}))
case("_alive", browser=br(key="k1", owned=True), state=state(procs={"k1": {"pid": 100}}),
     io=io(**{"proc.poll": 1}))
case("_alive", browser=br(pid=100, start_time=None, owned=False), io=io(_pid_alive=True))
case("_alive", browser=br(pid=None, owned=False))
case("_alive", browser=br(pid=100, owned=False), io=io(_pid_alive=False))
case("_alive", browser=br(pid=100, start_time=5, owned=False), io=io(_pid_alive=True, _start_time=5))
case("_alive", browser=br(pid=100, start_time=5, owned=False), io=io(_pid_alive=True, _start_time=999))

# ── _foreign_alive ──────────────────────────────────────────────────────────
case("_foreign_alive", browser=br(foreign=True),
     io=io(_read_devtools_file=[9222, "/devtools/browser/abc"], _devtools_endpoint_live=True))
case("_foreign_alive", browser=br(foreign=True), io=io(_read_devtools_file=None))
case("_foreign_alive", browser=br(foreign=True, cdp_url="ws://127.0.0.1:1/x"),
     io=io(_read_devtools_file=[9222, "/devtools/browser/abc"]))
case("_foreign_alive", browser=br(foreign=True),
     io=io(_read_devtools_file=[9222, "/devtools/browser/abc"], _devtools_endpoint_live=False))
case("_foreign_alive", browser=br(foreign=True), io=io(_read_devtools_file=raise_("OSError", "x")))

# ── _same_browser ───────────────────────────────────────────────────────────
case("_same_browser", a=br(), b=br(), io=io(realpath="/real/prof", ns_tag=TAG))
case("_same_browser", a=br(pid=100), b=br(pid=200))
case("_same_browser", a=br(foreign=False), b=br(foreign=True))
case("_same_browser", a=br(record_name="a.json"), b=br(record_name="b.json"),
     io=io(realpath="/real/prof"))
case("_same_browser", a=br(user_data_dir="/prof"), b=br(user_data_dir="/other"),
     io=io(realpath=seq("/real/prof", "/real/prof", "/real/prof", "/real/other"), ns_tag=TAG))
case("_same_browser", a=br(foreign=True, cdp_url="ws://x/devtools/browser/a"),
     b=br(foreign=True, cdp_url="ws://x/devtools/browser/b"),
     io=io(realpath="/real/prof", ns_tag=TAG))
case("_same_browser", a=br(foreign=True), b=br(foreign=True),
     io=io(realpath="/real/prof", ns_tag=TAG))

# ── _heir_key ───────────────────────────────────────────────────────────────
case("_heir_key", browser=br(ephemeral=True))
case("_heir_key", browser=br(key="k1"), browser_ref="k1",
     state=state(browsers={"k1": br(key="k1"), "k2": br(key="k2")}),
     io=io(realpath="/real/prof", ns_tag=TAG))
case("_heir_key", browser=br(key="k1"), browser_ref="k1",
     state=state(browsers={"k1": br(key="k1")}), io=io(realpath="/real/prof", ns_tag=TAG))
case("_heir_key", browser=br(key="k1"), browser_ref="k1",
     state=state(browsers={"k1": br(key="k1"), "k2": br(key="k2", ephemeral=True)}),
     io=io(realpath="/real/prof", ns_tag=TAG))
case("_heir_key", browser=br(key="k1"), state=state(browsers={}))
case("_heir_key", browser=br(key="k1", pid=100), browser_ref="k1",
     state=state(browsers={"k1": br(key="k1", pid=100), "k2": br(key="k2", pid=200)}),
     io=io(realpath="/real/prof", ns_tag=TAG))

# ── _tracked_browser ────────────────────────────────────────────────────────
case("_tracked_browser", record={"pid": 100, "user_data_dir": "/prof", "start_time": 5},
     state=state(browsers={"k1": br(pid=100, start_time=5)}), io=io(realpath="/real/prof"))
case("_tracked_browser", record={"pid": 100, "user_data_dir": "/prof", "start_time": 5},
     state=state(launching={"100": br(pid=100, start_time=5)}), io=io(realpath="/real/prof"))
case("_tracked_browser", record={"pid": 999, "user_data_dir": "/prof", "start_time": 5},
     state=state(browsers={"k1": br(pid=100)}), io=io(realpath="/real/prof"))
case("_tracked_browser", record={"pid": 100, "user_data_dir": "/prof", "start_time": 5},
     state=state(browsers={"k1": br(pid=100, foreign=True)}), io=io(realpath="/real/prof"))
case("_tracked_browser", record={"pid": 100, "user_data_dir": "/prof", "start_time": 5},
     state=state())

# ── _terminate ──────────────────────────────────────────────────────────────
case("_terminate", pid=100, start_time=5, proc=None,
     io=io(**{"ProcessRegistry._terminate_host_pid": None}))
case("_terminate", pid=0, start_time=None, proc=None)
case("_terminate", pid=None, start_time=None, proc=None)
case("_terminate", pid=100, start_time=5, proc=None,
     io=io(**{"ProcessRegistry._terminate_host_pid": raise_("OSError", "no"), "logger.debug": None}))
case("_terminate", pid=100, start_time=5, proc={"pid": 100},
     io=io(**{"ProcessRegistry._terminate_host_pid": None, "proc.wait": None}))
case("_terminate", pid=100, start_time=5, proc={"pid": 100},
     io=io(**{"ProcessRegistry._terminate_host_pid": None,
              "proc.wait": raise_("TimeoutExpired"), "proc.kill": None}))
case("_terminate", pid=100, start_time=5, proc=None,
     io=io(**{"ProcessRegistry._terminate_host_pid": raise_("ValueError", "x")}))

# ── _forget ─────────────────────────────────────────────────────────────────
case("_forget", key="k1", browser_ref="k1", drop_record=True,
     state=state(browsers={"k1": br(key="k1", ephemeral=True)}, procs={"k1": {"pid": 100}}),
     env={"has_getuid": True},
     io=io(**{"proc.wait": None, "proc.poll": None}, mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, unlink=None))
case("_forget", key="k1", browser_ref="k1", drop_record=True,
     state=state(browsers={"k1": br(key="k1"), "k2": br(key="k2")}, procs={"k1": {"pid": 100}}),
     env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", realpath="/real/prof", ns_tag=TAG, drop_lease=None, mkdir=None, lstat=PRIV,
           getuid=1000, unlink=None))
case("_forget", key="k1", browser_ref="k1", drop_record=True,
     state=state(browsers={"k1": br(key="k1", ephemeral=True)}, procs={"k1": {"pid": 100}}),
     env={"has_getuid": True},
     io=io(**{"proc.wait": raise_("TimeoutExpired")}, mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, unlink=None))
case("_forget", key="k1", browser_ref="k1", drop_record=True,
     state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=True)}),
     env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))
case("_forget", key="k1", browser_ref="k1", drop_record=False,
     state=state(browsers={"k1": br(key="k1", ephemeral=True)}),
     env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG))
case("_forget", key="k1", browser=br(key="k1", ephemeral=True), drop_record=True,
     state=state(browsers={"k1": br(key="k1", ephemeral=True)}),
     env={"has_getuid": True},
     io=io(mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))

# ── get_browser ─────────────────────────────────────────────────────────────
case("get_browser", key="k1", state=state())
case("get_browser", key="k1", state=state(browsers={"k1": br(key="k1", owned=True)},
                                          procs={"k1": {"pid": 100}}),
     io=io(**{"proc.poll": None}))
case("get_browser", key="k1", state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=True)}),
     env={"has_getuid": True},
     io=io(_pid_alive=False, **{"logger.info": None}, mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, unlink=None, rmtree=None))
case("get_browser", key="k1", state=state(browsers={"k1": br(key="k1", foreign=True)}),
     io=io(_read_devtools_file=[9222, "/devtools/browser/abc"], _devtools_endpoint_live=True))
case("get_browser", key="k1", state=state(browsers={"k1": br(key="k1", foreign=True)}),
     env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", _read_devtools_file=None, **{"logger.info": None}, same_namespace=True,
           _pid_alive=False, mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof",
           ns_tag=TAG, unlink=None))

# ── _last_holder_out ────────────────────────────────────────────────────────
case("_last_holder_out", record_path="/r.json", browser=br(foreign=True), io=io(drop_lease=None))
case("_last_holder_out", record_path="/r.json", browser=br(start_time=5),
     io=io(drop_lease=None, other_live_holders=[{"pid": 1}], exists=False, successor=None))
case("_last_holder_out", record_path="/r.json", browser=br(start_time=5),
     io=io(drop_lease=None, other_live_holders=[], exists=True,
           read_text='{"pid":100,"user_data_dir":"/prof","start_time":5,"owner_pid":9}',
           realpath="/real/prof", same_namespace=True, held_here=False, _pid_alive=True,
           _start_time=5, successor=None))
case("_last_holder_out", record_path="/r.json", browser=br(start_time=5, owned=True),
     io=io(drop_lease=None, other_live_holders=[], exists=False, successor=None))
case("_last_holder_out", record_path="/r.json", browser=br(start_time=5, owned=False),
     io=io(drop_lease=None, other_live_holders=[], exists=True,
           read_text='{"pid":100,"user_data_dir":"/prof","start_time":5,"owner_pid":9}',
           realpath="/real/prof", same_namespace=True, held_here=True, successor=None))
case("_last_holder_out", record_path="/r.json", browser=br(start_time=5, owned=True),
     io=io(drop_lease=None, other_live_holders=[{"pid": 7}], exists=True,
           read_text='{"pid":100,"user_data_dir":"/prof","start_time":5,"owner_pid":9}',
           realpath="/real/prof", same_namespace=True, held_here=False, _pid_alive=False,
           successor={"pid": 7}, hand_over=None))

# ── _stop_locked ────────────────────────────────────────────────────────────
case("_stop_locked", key="k1", state=state())
case("_stop_locked", key="k1",
     state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=True)}, procs={"k1": {"pid": 100}}),
     env={"has_getuid": True},
     io=io(**{"ProcessRegistry._terminate_host_pid": None, "proc.wait": None},
           mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))
case("_stop_locked", key="k1",
     state=state(browsers={"k1": br(key="k1", ephemeral=False, owned=True)}),
     env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=NOTPRIV, getuid=1000,
           **{"ProcessRegistry._terminate_host_pid": None, "logger.debug": None}))
case("_stop_locked", key="k1",
     state=state(browsers={"k1": br(key="k1", ephemeral=False, owned=True, start_time=5)},
                 procs={"k1": {"pid": 100}}),
     env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           records_lock=None, drop_lease=None, other_live_holders=[], exists=False,
           successor=None, **{"ProcessRegistry._terminate_host_pid": None, "proc.wait": None},
           unlink=None))
case("_stop_locked", key="k1",
     state=state(browsers={"k1": br(key="k1", ephemeral=False), "k2": br(key="k2", ephemeral=False)}),
     env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           records_lock=None))

# ── stop_browser ────────────────────────────────────────────────────────────
case("stop_browser", key="k1", state=state(), io=io(_key_lock=None))
case("stop_browser", key="k1",
     state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=True)}),
     env={"has_getuid": True},
     io=io(_key_lock=None, **{"ProcessRegistry._terminate_host_pid": None},
           mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))
case("stop_browser", key="k1", state=state(), io=io(_key_lock=raise_("OSError", "x")))
case("stop_browser", key="k1",
     state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=False)}),
     env={"has_getuid": True},
     io=io(_key_lock=None, mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof",
           ns_tag=TAG, unlink=None, rmtree=None))
case("stop_browser", key="k2", state=state(browsers={"k1": br(key="k1")}), io=io(_key_lock=None))

# ── stop_all_browsers ───────────────────────────────────────────────────────
case("stop_all_browsers", state=state())
case("stop_all_browsers", state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=True)}),
     env={"has_getuid": True},
     io=io(_key_lock=None, **{"ProcessRegistry._terminate_host_pid": None},
           mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))
case("stop_all_browsers",
     state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=True),
                           "k2": br(key="k2", ephemeral=True, owned=True)}),
     env={"has_getuid": True},
     io=io(_key_lock=None, **{"ProcessRegistry._terminate_host_pid": None},
           mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))
case("stop_all_browsers", state=state(browsers={"k1": br(key="k1", ephemeral=True, owned=False)}),
     env={"has_getuid": True},
     io=io(_key_lock=None, mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof",
           ns_tag=TAG, unlink=None, rmtree=None))
case("stop_all_browsers", state=state(browsers={"k1": br(key="k1", ephemeral=False, owned=True)}),
     env={"has_getuid": True},
     io=io(_key_lock=None, _get_lock_dir="/lock", mkdir=None, lstat=NOTPRIV, getuid=1000,
           **{"ProcessRegistry._terminate_host_pid": None, "logger.debug": None}))

# ── _abandon_launch ─────────────────────────────────────────────────────────
case("_abandon_launch", proc={"pid": 100}, starting=br(ephemeral=True, start_time=5),
     env={"has_getuid": True},
     io=io(**{"proc.poll": None, "ProcessRegistry._terminate_host_pid": None, "proc.wait": None},
           mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG,
           unlink=None, rmtree=None))
case("_abandon_launch", proc={"pid": 100}, starting=br(ephemeral=True, start_time=5),
     env={"has_getuid": True},
     io=io(**{"proc.poll": 1}, mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, unlink=None, rmtree=None))
case("_abandon_launch", proc={"pid": 100}, starting=br(ephemeral=False, start_time=5),
     env={"has_getuid": True},
     io=io(_get_lock_dir="/lock", **{"proc.poll": None, "ProcessRegistry._terminate_host_pid": None, "proc.wait": None},
           mkdir=None, lstat=PRIV, getuid=1000, realpath="/real/prof", ns_tag=TAG, unlink=None))
case("_abandon_launch", proc={"pid": 100}, starting=br(ephemeral=True),
     io=io(**{"proc.poll": raise_("OSError", "x")}))
case("_abandon_launch", proc={"pid": 100}, starting=br(ephemeral=True, start_time=5),
     env={"has_getuid": True},
     io=io(**{"proc.poll": 1}, mkdir=seq(raise_("OSError", "no"), None),
           **{"logger.debug": None}, rmtree=None))

# ── _launch_owned ───────────────────────────────────────────────────────────
LO_BASE = dict(key="k1", executable="/chromium", user_data_dir="/prof", ephemeral=True,
               packaged=True, headed=False, extra_args=[], browser_env={"DISPLAY": ":0"}, timeout=30,
               home_key="/home", sandbox_bypass=True)
case("_launch_owned", **LO_BASE, env={"has_getuid": True},
     io=io(_prepare_dir=None, _resolve_headed=False, build_chromium_argv=["/chromium", "about:blank"],
           _spawn={"pid": 100}, _start_time=5, monotonic=0, _launch_timeout=30,
           _wait_for_devtools=[[9222, "/devtools/browser/abc"], None],
           getpid=seq(1000, 1000), ns_identity="ns1", mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, atomic_json_write=None, **{"logger.info": None}))
case("_launch_owned", **{**LO_BASE, "browser_env": None}, env={"has_getuid": True},
     io=io(build_browser_env={"DISPLAY": ":0"}, _prepare_dir=None, _resolve_headed=False,
           build_chromium_argv=["/chromium"], _spawn={"pid": 100}, _start_time=5,
           monotonic=0, _launch_timeout=30, _wait_for_devtools=[[9222, "/devtools/browser/abc"], None],
           getpid=seq(1000, 1000), ns_identity="ns1", mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, atomic_json_write=None, **{"logger.info": None}))
case("_launch_owned", **LO_BASE,
     io=io(_prepare_dir=raise_("OSError", "mkdir failed")))
case("_launch_owned", **LO_BASE,
     io=io(_prepare_dir=None, _resolve_headed=False, build_chromium_argv=["/chromium"],
           _spawn=raise_("OSError", "spawn failed")))
case("_launch_owned", **LO_BASE, env={"has_getuid": True},
     io=io(_prepare_dir=None, _resolve_headed=False, build_chromium_argv=["/chromium"],
           _spawn={"pid": 100}, _start_time=5, monotonic=0, _launch_timeout=30,
           _wait_for_devtools=[None, "Chromium exited with code 1 during startup."],
           getpid=seq(1000, 1000), ns_identity="ns1", mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, atomic_json_write=None,
           **{"proc.poll": 1}, unlink=None, rmtree=None))
case("_launch_owned", **LO_BASE, env={"has_getuid": True},
     io=io(_prepare_dir=None, _resolve_headed=False, build_chromium_argv=["/chromium"],
           _spawn={"pid": 100}, _start_time=5, monotonic=0, _launch_timeout=30,
           _wait_for_devtools=raise_("KeyboardInterrupt"),
           getpid=seq(1000, 1000), ns_identity="ns1", mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, atomic_json_write=None,
           **{"proc.poll": None, "ProcessRegistry._terminate_host_pid": None, "proc.wait": None},
           unlink=None, rmtree=None))
case("_launch_owned", **LO_BASE, env={"has_getuid": True},
     io=io(_prepare_dir=None, _resolve_headed=False, build_chromium_argv=["/chromium"],
           _spawn={"pid": 100}, _start_time=5, monotonic=0, _launch_timeout=30,
           _wait_for_devtools=raise_("SystemExit", "bye"),
           getpid=seq(1000, 1000), ns_identity="ns1", mkdir=None, lstat=PRIV, getuid=1000,
           realpath="/real/prof", ns_tag=TAG, atomic_json_write=None,
           **{"proc.poll": 1}, unlink=None, rmtree=None))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function count check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"gen {NAME}: {len(CASES)} cases, {len(counts)} funcs")
    if short:
        print("SHORT (<5):", short)
        raise SystemExit(1)


NAME = "browser_launcher_flow2"
if __name__ == "__main__":
    main()