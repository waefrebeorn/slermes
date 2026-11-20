#!/usr/bin/env python3
"""Generate tests/fixtures/process_identity_flow2.jsonl for the process_identity_flow2
FLOW oracle (hermes_cli/process_identity.py UNCLAIMED remainder). >=5 cases per ported
function (17 funcs), exercising decision branches, error paths, BaseException
propagation, real composition, lru_cache hits and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None); single
value = constant; scripted null = None; {"__raise__": cls, "msg": ...} = exception.
ASCII / POSIX-lexical domain; integral numbers read as int; proc doubles are dicts or
null; create_time values are numbers or null; project_root is a POSIX string or null.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "process_identity_flow2.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


PROC = {"_proc": {}}          # a Linux platform-proc double
LINUX = {"LINUX": True}
NOLINUX = {"LINUX": False}

# ── boot_start ──
add("boot_start", {"proc": PROC, "psutil": NOLINUX})                       # None, no seam
add("boot_start", {"proc": PROC, "psutil": {}}, {})                        # None (flag absent)
add("boot_start", {"proc": PROC, "psutil": LINUX}, {"proc._proc.create_time": 123.5})
add("boot_start", {"proc": PROC, "psutil": LINUX}, {"proc._proc.create_time": "100.25"})
add("boot_start", {"proc": None, "psutil": LINUX})                         # AttributeError NoneType
add("boot_start", {"proc": {}, "psutil": LINUX})                           # AttributeError no _proc
add("boot_start", {"proc": {"_proc": None}, "psutil": LINUX})              # AttributeError create_time
add("boot_start", {"proc": PROC, "psutil": LINUX}, {"proc._proc.create_time": R("NoSuchProcess")})
add("boot_start", {"proc": PROC, "psutil": LINUX}, {"proc._proc.create_time": "abc"})  # ValueError
add("boot_start", {"proc": PROC, "psutil": LINUX}, {})                     # fail-closed

# ── process_create_time ──
add("process_create_time", {"pid": None},
    {"os.getpid": 42, "psutil.Process": PROC, "process_birth": 100.0})
add("process_create_time", {"pid": 5}, {"psutil.Process": PROC, "process_birth": 100.0})
add("process_create_time", {"pid": 5}, {"psutil.Process": R("NoSuchProcess")})   # -> None
add("process_create_time", {"pid": 5}, {"psutil.Process": PROC, "process_birth": R("Error")})
add("process_create_time", {"pid": None}, {"os.getpid": R("OSError")})           # -> None
add("process_create_time", {"pid": 5}, {"psutil.Process": R("SystemExit", "bye")})  # propagate
add("process_create_time", {"pid": 5}, {})                                       # fail-closed -> None
add("process_create_time", {"pid": 5}, {"psutil.Process": PROC, "process_birth": 0})

# ── live_boot_id (lru_cache memo state) ──
add("live_boot_id", {"calls": 1}, {"path.read_text": "  abc-def  "})
add("live_boot_id", {"calls": 1}, {"path.read_text": ""})
add("live_boot_id", {"calls": 1}, {"path.read_text": "   "})
add("live_boot_id", {"calls": 1}, {"path.read_text": "boot-1\n"})
add("live_boot_id", {"calls": 1}, {"path.read_text": R("FileNotFoundError")})   # OSError -> None
add("live_boot_id", {"calls": 1}, {"path.read_text": R("SystemExit", "bye")})   # propagate
add("live_boot_id", {"calls": 2}, {"path.read_text": "boot-2"})                 # cache hit
add("live_boot_id", {"calls": 2}, {"path.read_text": R("PermissionError")})     # None cached
add("live_boot_id", {"calls": 1}, {})                                           # fail-closed propagate

# ── entry_boot ──
add("entry_boot", {"entry": {"boot_id": "b1", "boot_start": 50}, "key": "boot_start"},
    {"live_boot_id": "b1"})
add("entry_boot", {"entry": {"boot_id": "b1", "boot_start": 50}, "key": "boot_start"},
    {"live_boot_id": "b2"})                                                     # -1.0
add("entry_boot", {"entry": {"boot_id": None, "boot_start": 50}, "key": "boot_start"},
    {"live_boot_id": "b1"})                                                     # None
add("entry_boot", {"entry": {}, "key": "boot_start"}, {"live_boot_id": "b1"})   # None
add("entry_boot", {"entry": {"boot_id": "b1"}, "key": "boot_start"}, {"live_boot_id": None})
add("entry_boot", {"entry": {"boot_id": "b1", "spawner_boot": 7}, "key": "spawner_boot"},
    {"live_boot_id": "b1"})
add("entry_boot", {"entry": {"boot_id": "b1", "boot_start": None}, "key": "boot_start"},
    {"live_boot_id": "b1"})                                                     # present-null -> None
add("entry_boot", {"entry": {"boot_id": "b1"}, "key": "boot_start"},
    {"live_boot_id": R("SystemExit", "bye")})                                   # propagate

# ── verified_boot_start ──
add("verified_boot_start", {"pid": None, "create_time": 100, "psutil": LINUX})  # guard None
add("verified_boot_start", {"pid": 0, "create_time": 100, "psutil": LINUX})
add("verified_boot_start", {"pid": 5, "create_time": None, "psutil": LINUX})
add("verified_boot_start", {"pid": 5, "create_time": 100, "psutil": LINUX}, {"live_boot_id": None})
add("verified_boot_start", {"pid": 5, "create_time": 100, "psutil": LINUX},
    {"live_boot_id": "b1", "psutil.Process": R("NoSuchProcess")})               # -> None
add("verified_boot_start", {"pid": 5, "create_time": 100, "psutil": LINUX},
    {"live_boot_id": "b1", "psutil.Process": PROC, "process_birth": 100,
     "proc._proc.create_time": 100})                                            # -> 100
add("verified_boot_start", {"pid": 5, "create_time": 100, "psutil": LINUX},
    {"live_boot_id": "b1", "psutil.Process": PROC, "process_birth": 200})       # mismatch -> None
add("verified_boot_start", {"pid": 5, "create_time": 100, "psutil": LINUX},
    {"live_boot_id": R("SystemExit", "bye")})                                   # propagate

# ── same_incarnation ──
add("same_incarnation", {"proc": PROC, "create_time": None, "boot": None, "psutil": NOLINUX})
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"process_birth": 100})
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"process_birth": 101.5})
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"process_birth": 103})                                                     # False
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": 50, "psutil": LINUX},
    {"proc._proc.create_time": 50})                                             # True
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": 50, "psutil": LINUX},
    {"proc._proc.create_time": 60})                                             # False
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": 50, "psutil": NOLINUX},
    {"process_birth": 100})                                                     # boot_start None -> epoch
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": "abc", "psutil": LINUX},
    {"proc._proc.create_time": 50})                                             # float ValueError
add("same_incarnation", {"proc": PROC, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"process_birth": R("SystemExit", "bye")})                                  # propagate

# ── spawned_by_this_process ──
TAG = {"spawner_pid": 5, "spawner_create": 100.0}
ENVOK = {"HERMES_SPAWN": "v1:inst:serve:5:100.000"}
add("spawned_by_this_process", {"pid": "5"},
    {"psutil.Process": PROC, "proc.environ": ENVOK, "parse_spawn_tag": TAG,
     "os.getpid": 5, "process_birth": 100.0})                                   # True
add("spawned_by_this_process", {"pid": 5},
    {"psutil.Process": PROC, "proc.environ": {}, "parse_spawn_tag": None, "os.getpid": 5})
add("spawned_by_this_process", {"pid": 5},
    {"psutil.Process": PROC, "proc.environ": ENVOK, "parse_spawn_tag": TAG, "os.getpid": 999})
add("spawned_by_this_process", {"pid": 5}, {"psutil.Process": R("NoSuchProcess")})  # False
add("spawned_by_this_process", {"pid": 5}, {"psutil.Process": PROC, "proc.environ": R("AccessDenied")})
add("spawned_by_this_process", {"pid": "abc"}, {})                              # int ValueError propagate
add("spawned_by_this_process", {"pid": 5}, {"psutil.Process": R("SystemExit", "bye")})
add("spawned_by_this_process", {"pid": 5},
    {"psutil.Process": PROC, "proc.environ": ENVOK,
     "parse_spawn_tag": {"spawner_pid": 5, "spawner_create": None}, "os.getpid": 5,
     "process_birth": 100.0})                                                   # spawner_create None -> True
add("spawned_by_this_process", {"pid": 5},
    {"psutil.Process": PROC, "proc.environ": ENVOK, "parse_spawn_tag": TAG,
     "os.getpid": 5, "process_birth": R("Error")})                              # mine None -> True

# ── read_ledger_or_quarantine ──
add("read_ledger_or_quarantine", {"path": "/h/spawn-ledger.json"},
    {"_read_ledger": [{"pid": 5}]})
add("read_ledger_or_quarantine", {"path": "/h/spawn-ledger.json"}, {"_read_ledger": []})
add("read_ledger_or_quarantine", {"path": "/h/spawn-ledger.json"},
    {"_read_ledger": None, "os.replace": None, "logger.warning": None})
add("read_ledger_or_quarantine", {"path": "/h/spawn-ledger.json"},
    {"_read_ledger": None, "os.replace": R("PermissionError")})                 # OSError pass
add("read_ledger_or_quarantine", {"path": "/h/spawn-ledger.json"},
    {"_read_ledger": None, "os.replace": R("SystemExit", "bye")})               # propagate
add("read_ledger_or_quarantine", {"path": "/h/spawn-ledger.json"},
    {"_read_ledger": R("SystemExit", "bye")})                                   # propagate
add("read_ledger_or_quarantine", {"path": "/h\\spawn-ledger.json"},
    {"_read_ledger": None, "os.replace": None, "logger.warning": None})         # path normalize

# ── is_desktop_owned_backend ──
add("is_desktop_owned_backend", {"env": {}})                                    # False
add("is_desktop_owned_backend", {"env": {"HERMES_DESKTOP": "0"}})               # False
add("is_desktop_owned_backend", {"env": {"HERMES_DESKTOP": "1", "HERMES_DASHBOARD_SESSION_TOKEN": "tok"}})
add("is_desktop_owned_backend", {"env": {"HERMES_DESKTOP": "1"}, "argv": ["x", "--token-file", "f"]},
    {"is_desktop_ssh_backend_argv": True})
add("is_desktop_owned_backend", {"env": {"HERMES_DESKTOP": "1"}, "argv": None, "sys_argv": ["hermes", "serve"]},
    {"is_desktop_ssh_backend_argv": False})
add("is_desktop_owned_backend", {"env": {"HERMES_DESKTOP": "1"}, "argv": ["x"]},
    {"is_desktop_ssh_backend_argv": R("SystemExit", "bye")})                    # propagate

# ── local_desktop_client ──
add("local_desktop_client", {"identity": {"provider": "local", "native_bootstrap": True}, "declared": True})
add("local_desktop_client", {"identity": {"provider": "local", "native_bootstrap": True}, "declared": False})
add("local_desktop_client", {"identity": {"provider": "local", "native_bootstrap": True}, "declared": 1})
add("local_desktop_client", {"identity": None, "declared": True})
add("local_desktop_client", {"identity": {"provider": "remote", "native_bootstrap": True}, "declared": True})
add("local_desktop_client", {"identity": {"provider": "local", "native_bootstrap": False}, "declared": True})
add("local_desktop_client", {"identity": {"provider": "local"}, "declared": True})

# ── desktop_spawner_identity ──
add("desktop_spawner_identity", {"env": {}})                                    # [null,null]
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "5"}})            # [5,null]
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "5", "HERMES_PARENT_START_MARKER": "winms:1500"}})
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "5", "HERMES_PARENT_START_MARKER": "other:1500"}})
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "5", "HERMES_PARENT_START_MARKER": "winms:abc"}})
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "0"}})
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "-3"}})
add("desktop_spawner_identity", {"env": {"HERMES_PARENT_PID": "5", "HERMES_PARENT_START_MARKER": "winms:"}})

# ── new_entry ──
add("new_entry", {"pid": 5, "create_time": 100, "purpose": "serve", "project_root": "/h",
                  "spawner_pid": 1, "spawner_create": 50, "psutil": LINUX},
    {"install_id": "inst", "time.time": 1000, "live_boot_id": "b1",
     "psutil.Process": PROC, "process_birth": 100, "proc._proc.create_time": 100})
add("new_entry", {"pid": None, "create_time": None, "purpose": "gateway", "project_root": None,
                  "spawner_pid": None, "spawner_create": None, "psutil": NOLINUX},
    {"install_id": "inst", "time.time": 1000, "live_boot_id": None})
add("new_entry", {"pid": 5, "create_time": 100, "purpose": "serve", "project_root": "/h",
                  "spawner_pid": 1, "spawner_create": 50, "psutil": NOLINUX},
    {"install_id": "inst", "time.time": 1000, "live_boot_id": "b1", "process_birth": 100})
add("new_entry", {"pid": 5, "create_time": 100, "purpose": "serve", "project_root": "/h",
                  "spawner_pid": 1, "spawner_create": 50, "psutil": LINUX},
    {"install_id": R("SystemExit", "bye")})                                     # propagate
add("new_entry", {"pid": 5, "create_time": 100, "purpose": "serve", "project_root": "/h",
                  "spawner_pid": 1, "spawner_create": 50, "psutil": LINUX},
    {"install_id": "inst", "time.time": R("OSError")})                          # propagate
add("new_entry", {"pid": 5, "create_time": 100, "purpose": "serve", "project_root": "/h",
                  "spawner_pid": 1, "spawner_create": 50, "psutil": LINUX},
    {"install_id": "inst", "time.time": 1000, "live_boot_id": R("SystemExit", "bye")})

# ── entry_still_running ──
add("entry_still_running", {"entry": {"pid": 5, "create_time": 100, "boot_id": "b1", "boot_start": 50}},
    {"live_boot_id": "b1", "_pid_alive_matches": True})
add("entry_still_running", {"entry": {"pid": 5, "create_time": 100, "boot_id": "b1", "boot_start": 50}},
    {"live_boot_id": "b1", "_pid_alive_matches": False})
add("entry_still_running", {"entry": {"pid": 5, "create_time": 100, "boot_id": "b1", "boot_start": 50}},
    {"live_boot_id": "b1", "_pid_alive_matches": None})                         # is not False -> True
add("entry_still_running", {"entry": {"pid": 0}})                               # False
add("entry_still_running", {"entry": {"pid": "x"}})                             # False
add("entry_still_running", {"entry": {}})                                       # False
add("entry_still_running", {"entry": {"pid": 5, "create_time": 100}},
    {"live_boot_id": None, "_pid_alive_matches": True})                         # boot None dropped
add("entry_still_running", {"entry": {"pid": 5, "create_time": 100}},
    {"live_boot_id": None, "_pid_alive_matches": R("SystemExit", "bye")})       # propagate

# ── is_owner_death_supervisor ──
SP = "/h/process_supervisor.sh"
add("is_owner_death_supervisor", {"argv": ["sh", SP, "123"], "script_path": SP})
add("is_owner_death_supervisor", {"argv": ["sh", "/other.sh"], "script_path": SP})
add("is_owner_death_supervisor", {"argv": ["sh"], "script_path": SP})
add("is_owner_death_supervisor", {"argv": [], "script_path": SP})
add("is_owner_death_supervisor", {"argv": ["sh", SP], "script_path": SP})
add("is_owner_death_supervisor", {"argv": ["sh", "/h\\process_supervisor.sh"], "script_path": SP})

# ── terminate_then_kill ──
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": PROC, "process_birth": 100, "proc.terminate": None, "proc.wait": 0})
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": PROC, "process_birth": 100, "proc.terminate": None,
     "proc.wait": R("TimeoutExpired"), "proc.kill": None})                      # kill -> True
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": PROC, "process_birth": 200})                             # mismatch -> False
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": R("NoSuchProcess"), "logger.debug": None})               # -> False
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": PROC, "process_birth": 100, "proc.terminate": R("AccessDenied"),
     "logger.debug": None})                                                     # -> False
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": PROC, "process_birth": 100, "proc.terminate": None,
     "proc.wait": R("Error"), "logger.debug": None})                            # -> False
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": PROC, "process_birth": 100, "proc.terminate": None,
     "proc.wait": R("TimeoutExpired"), "proc.kill": R("AccessDenied"), "logger.debug": None})
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": None, "psutil": NOLINUX},
    {"psutil.Process": R("SystemExit", "bye")})                                 # propagate
add("terminate_then_kill", {"pid": 5, "create_time": 100, "boot": 50, "psutil": LINUX},
    {"psutil.Process": PROC, "proc._proc.create_time": 50, "proc.terminate": None, "proc.wait": 0})

# ── reparented_orphan ──
add("reparented_orphan", {"pid": 5, "reap_min_age": 180},
    {"_process_ppid": 1, "_lock_owned_serve_pids": [], "time.time": 1000,
     "psutil.Process": PROC, "proc.create_time": 100})                          # True
add("reparented_orphan", {"pid": 5, "reap_min_age": 180}, {"_process_ppid": 5})  # False
add("reparented_orphan", {"pid": 5, "reap_min_age": 180},
    {"_process_ppid": 1, "_lock_owned_serve_pids": [[5]]})                      # claimed -> False
add("reparented_orphan", {"pid": 5, "reap_min_age": 180},
    {"_process_ppid": 0, "_lock_owned_serve_pids": [[]], "time.time": 1000,
     "psutil.Process": PROC, "proc.create_time": 900})                          # age 100 < 180 -> False
add("reparented_orphan", {"pid": 5, "reap_min_age": 180}, {"_process_ppid": R("Error")})
add("reparented_orphan", {"pid": 5, "reap_min_age": 180},
    {"_process_ppid": 1, "_lock_owned_serve_pids": [[]], "time.time": R("OSError")})
add("reparented_orphan", {"pid": 5, "reap_min_age": 180}, {"_process_ppid": R("SystemExit", "bye")})
add("reparented_orphan", {"pid": 5, "reap_min_age": 180}, {})                   # fail-closed -> False

# ── reap_orphaned_backend_owner ──
E5 = {"pid": 5, "purpose": "serve", "create_time": 100, "spawner_pid": 1}
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "kill_fn": None, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": [E5], "spawner_is_dead": True, "live_boot_id": None,
     "psutil.Process": PROC, "process_birth": 100, "proc.terminate": None, "proc.wait": 0,
     "logger.info": None})                                                      # -> 5
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "kill_fn": None, "psutil": NOLINUX,
                                    "reap_min_age": 180},
    {"os.getpid": 999, "ledger_entries": [{"pid": 5, "purpose": "serve", "create_time": 100, "spawner_pid": None}],
     "spawner_is_dead": False, "live_boot_id": None, "_process_ppid": 1, "_lock_owned_serve_pids": [[]],
     "time.time": 1000, "psutil.Process": PROC, "proc.create_time": 100, "process_birth": 100,
     "proc.terminate": None, "proc.wait": 0, "logger.info": None})              # reparent -> 5
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "kill_fn": None, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": [E5], "spawner_is_dead": False})       # live spawner -> None
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "kill_fn": None, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": []})                                   # no entry -> None
add("reap_orphaned_backend_owner", {"pid": 0, "create_time": 100})              # -> None
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100}, {"os.getpid": 5})  # self -> None
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "kill_fn": True, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": [E5], "spawner_is_dead": True, "kill_fn": None,
     "logger.info": None})                                                      # kill_fn -> 5
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "kill_fn": True, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": [E5], "spawner_is_dead": True,
     "kill_fn": R("Error"), "logger.debug": None})                              # -> None
add("reap_orphaned_backend_owner", {"pid": "abc", "create_time": 100})          # int ValueError -> None
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": R("Error")})                           # -> None
add("reap_orphaned_backend_owner", {"pid": 5, "create_time": 100, "psutil": NOLINUX},
    {"os.getpid": 999, "ledger_entries": [{"pid": 5, "purpose": "mcp-helper", "create_time": 100}]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()