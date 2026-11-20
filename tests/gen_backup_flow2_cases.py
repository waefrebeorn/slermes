#!/usr/bin/env python3
"""Generate tests/fixtures/backup_flow2.jsonl — scripted-seam FLOW cases for
the backup_flow2 port (the still-UNCLAIMED remainder of hermes_cli/backup.py:
12 FLOW cores; the 33 already claimed by backup.c/backup_gaps.c are excluded).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides; a present JSON null is a
valid None result. {"__arr__": v} is an explicit array VALUE, not a queue.

Domains (documented in the port header):
- ASCII strings; integral numbers only. Path objects project to strings;
  directory entries are {"name":str,"is_file":bool}.
- Fixture raise specs are the uniform {"__raise__":cls,"msg":m} shape.
Run: python tests/gen_backup_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "backup_flow2.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def arr(v):
    return {"__arr__": v}


# ══════════════ snapshot_recovery_hint (no seams) ══════════════
for i in range(6):
    add("snapshot_recovery_hint", {"label": "s%d" % i}, {})


# ══════════════ print_capped (seam: print) ══════════════
add("print_capped", {"header": "H", "indent": "  ", "lines": []}, {})
add("print_capped", {"header": "Files", "indent": "  ", "lines": ["a", "b"]}, {})
add("print_capped", {"header": "Files", "indent": "", "lines": ["x"] * 10}, {})
add("print_capped", {"header": "Files", "indent": "\t", "lines": ["x"] * 11}, {})
add("print_capped", {"header": "H", "indent": "  ", "lines": ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m"]}, {})
add("print_capped", {"header": "", "indent": " ", "lines": ["one", "", "three"]}, {})


# ══════════════ import_skipped (seams: normalize_archive_parts, profile_root_entry) ══════════════
def imp(rel, parts, proot="", nm=None, pr=None):
    s = {"normalize_archive_parts": arr(parts) if nm is None else nm}
    if proot or pr is not None:
        s["profile_root_entry"] = pr if pr is not None else proot
    return s


add("import_skipped", {"rel": "x/gateway_state.json"}, imp("x/gateway_state.json", ["x", "gateway_state.json"]))
add("import_skipped", {"rel": "models/v1/config.json"}, imp("models/v1/config.json", ["models", "v1", "config.json"], proot="models"))
add("import_skipped", {"rel": "state.db-wal"}, imp("state.db-wal", ["state.db-wal"], proot=""))
add("import_skipped", {"rel": "config.yaml"}, imp("config.yaml", ["config.yaml"], proot=""))
add("import_skipped", {"rel": "bad/../x"}, imp("bad/../x", [], nm=rs("ValueError", "traversal")))
add("import_skipped", {"rel": "weird"}, imp("weird", [], nm=rs("RuntimeError", "boom")))
add("import_skipped", {"rel": "runtimes/py/bin"}, imp("runtimes/py/bin", ["runtimes", "py", "bin"], pr=rs("OSError", "proot boom")))


# ══════════════ import_member_rel ══════════════
add("import_member_rel", {"member": "_external/home/u/.env", "prefix": ""}, {})
add("import_member_rel", {"member": "root/config.yaml", "prefix": "root/"},
    {"normalize_archive_parts": arr(["config.yaml"]), "profile_root_entry": ""})
add("import_member_rel", {"member": "config.yaml", "prefix": "other/"},
    {"normalize_archive_parts": arr(["config.yaml"]), "profile_root_entry": ""})
add("import_member_rel", {"member": "root/gateway_state.json", "prefix": "root/"},
    {"normalize_archive_parts": arr(["gateway_state.json"])})
add("import_member_rel", {"member": "root/models/x", "prefix": "root/"},
    {"normalize_archive_parts": arr(["models", "x"]), "profile_root_entry": "models"})
add("import_member_rel", {"member": "root/state.db-shm", "prefix": "root/"},
    {"normalize_archive_parts": arr(["state.db-shm"]), "profile_root_entry": ""})


# ══════════════ prune_prefixed_zips ══════════════
def ents(*names_files):
    return [{"name": n, "is_file": f} for n, f in names_files]


add("prune_prefixed_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "keep": 0, "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-2.zip", True), ("hermes-backup-1.zip", True)))})
add("prune_prefixed_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "keep": 1, "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-2.zip", True), ("hermes-backup-1.zip", True)))})
add("prune_prefixed_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "keep": 1, "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-3.zip", True), ("hermes-backup-2.zip", True), ("hermes-backup-1.zip", True))),
     "unlink": [None, rs("OSError", "locked")]})
add("prune_prefixed_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "keep": 0, "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("other.zip", True), ("hermes-backup-1.ZIP", True), ("hermes-backup-1.zip.incomplete.zip", True), ("hermes-backup-1.zip", False)))})
add("prune_prefixed_zips", {"backup_dir": "/h/backups", "prefix": "p-", "keep": 2, "what": "x"},
    {"path_exists": False})
add("prune_prefixed_zips", {"backup_dir": "/h/backups", "prefix": "p-", "keep": 0, "what": "x"},
    {"path_exists": rs("OSError", "no dir")})


# ══════════════ prune_incomplete_zips ══════════════
add("prune_incomplete_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-2.incomplete.zip", True), ("hermes-backup-1.incomplete.zip", True)))})
add("prune_incomplete_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-1.incomplete.zip", True)))})
add("prune_incomplete_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-3.incomplete.zip", True), ("hermes-backup-2.incomplete.zip", True), ("hermes-backup-1.incomplete.zip", True))),
     "unlink": [None, rs("PermissionError", "denied")]})
add("prune_incomplete_zips", {"backup_dir": "/h/backups", "prefix": "hermes-backup-", "what": "backup"},
    {"path_exists": True,
     "path_iterdir": arr(ents(("hermes-backup-1.zip", True), ("hermes-backup-1.incomplete.zip", False)))})
add("prune_incomplete_zips", {"backup_dir": "/h/backups", "prefix": "p-", "what": "x"},
    {"path_exists": False})
add("prune_incomplete_zips", {"backup_dir": "/h/backups", "prefix": "p-", "what": "x"},
    {"path_exists": rs("ValueError", "bad")})


# ══════════════ write_zip_file ══════════════
add("write_zip_file", {"zf": "zf1", "path": "/h/a.txt", "arcname": "a.txt"}, {"zip_filelist_len": 3})
add("write_zip_file", {"zf": "zf1", "path": "/h/b.txt", "arcname": "b.txt"}, {"zip_filelist_len": 0})
add("write_zip_file", {"zf": "zf1", "path": "/h/c.txt", "arcname": "c.txt"},
    {"zip_filelist_len": 5, "zip_write": rs("OSError", "read fail")})
add("write_zip_file", {"zf": "zf1", "path": "/h/d.txt", "arcname": "d.txt"},
    {"zip_filelist_len": 5, "zip_write": rs("ValueError", "bad zip"),
     "discard_failed_zip_members": None})
add("write_zip_file", {"zf": "zf1", "path": "/h/e.txt", "arcname": "e.txt"},
    {"zip_filelist_len": 7, "zip_write": rs("OSError", "read fail"),
     "discard_failed_zip_members": rs("RuntimeError", "discard boom")})
add("write_zip_file", {"zf": "zf1", "path": "/h/f.txt", "arcname": "f.txt"},
    {"zip_filelist_len": rs("RuntimeError", "len boom")})


# ══════════════ find_corrupt_members ══════════════
add("find_corrupt_members", {"zf": "zf1", "members": ["a.txt"]},
    {"zip_open": None, "zip_read": ["chunk", ""]})
add("find_corrupt_members", {"zf": "zf1", "members": ["a.txt", "b.txt"]},
    {"zip_open": [None, rs("OSError", "bad member")], "zip_read": ["x", ""]})
add("find_corrupt_members", {"zf": "zf1", "members": ["a.txt"]},
    {"zip_open": None, "zip_read": rs("zlib.error", "crc mismatch")})
add("find_corrupt_members", {"zf": "zf1", "members": ["a.txt"]},
    {"zip_open": None, "zip_read": [rs("EOFError", "truncated")], "zip_open_exit": None})
add("find_corrupt_members", {"zf": "zf1", "members": ["a.txt"]},
    {"zip_open": rs("BadZipFile", "no member")})
add("find_corrupt_members", {"zf": "zf1", "members": ["a.txt", "b.txt"]},
    {"zip_open": [None, None], "zip_read": ["x", "", "y", "", "z", ""]})


# ══════════════ snapshot_epochs_restorable ══════════════
def epi(exists, name, rname):
    return [exists, name, rname]


add("snapshot_epochs_restorable", {"files": ["state.db", "config.yaml"]},
    {"entry_paths": [epi(True, "state.db", "state.db"), epi(True, "config.yaml", "config.yaml")]})
add("snapshot_epochs_restorable", {"files": ["state.db"]},
    {"entry_paths": arr(epi(True, "state.db", "state.db"))})
add("snapshot_epochs_restorable", {"files": ["profiles/p/state.db"]},
    {"entry_paths": arr(epi(True, "other.db", "state.db")),
     "restore_destination_epoch": rs("OSError", "epoch mismatch")})
add("snapshot_epochs_restorable", {"files": ["state.db"]},
    {"entry_paths": arr(epi(False, "state.db", "state.db"))})
add("snapshot_epochs_restorable", {"files": ["state.db"]},
    {"entry_paths": arr(epi(True, "state.db", "state.db")),
     "restore_destination_epoch": rs("RuntimeError", "boom")})
add("snapshot_epochs_restorable", {"files": ["config.yaml"]},
    {"entry_paths": rs("ValueError", "traversal")})


# ══════════════ create_pre_update_snapshots_all_profiles ══════════════
SIB = [["alpha", "/h/alpha"], ["beta", "/h/beta"]]
add("create_pre_update_snapshots_all_profiles", {"invoking_home": "/h", "keep": 3, "max_file_size": 1000000},
    {"sibling_profile_homes": arr(SIB), "create_quick_snapshot": "snap-1"})
add("create_pre_update_snapshots_all_profiles", {"invoking_home": "/h", "keep": None, "max_file_size": None},
    {"sibling_profile_homes": arr(SIB), "create_quick_snapshot": [None, "snap-2"]})
add("create_pre_update_snapshots_all_profiles", {"invoking_home": "/h", "keep": 3, "max_file_size": 1},
    {"sibling_profile_homes": arr(SIB), "create_quick_snapshot": rs("RuntimeError", "snap boom")})
add("create_pre_update_snapshots_all_profiles", {"invoking_home": "/h", "keep": 3, "max_file_size": 1},
    {"sibling_profile_homes": arr(SIB), "create_quick_snapshot": rs("KeyboardInterrupt", "")})
add("create_pre_update_snapshots_all_profiles", {"invoking_home": "/h", "keep": 3, "max_file_size": 1},
    {"sibling_profile_homes": arr([])})
add("create_pre_update_snapshots_all_profiles", {"invoking_home": "/h", "keep": 3, "max_file_size": 1},
    {"sibling_profile_homes": arr(SIB), "create_quick_snapshot": "x"})


# ══════════════ restore_config_model_settings_all_profiles ══════════════
SNAPS = {"alpha": "snap-a", "beta": "snap-b", "ghost": "snap-g"}
add("restore_config_model_settings_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_config_model_settings_if_rewritten": {"restored": True, "keys": ["model.provider"]}})
add("restore_config_model_settings_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_config_model_settings_if_rewritten": [None, {"restored": True, "keys": ["moa"]}]})
add("restore_config_model_settings_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_config_model_settings_if_rewritten": rs("ValueError", "cfg boom")})
add("restore_config_model_settings_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_config_model_settings_if_rewritten": rs("SystemExit", "")})
add("restore_config_model_settings_all_profiles", {"profile_snapshots": {}, "invoking_home": "/h"}, {})
add("restore_config_model_settings_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_config_model_settings_if_rewritten": rs("TypeError", "t")})


# ══════════════ restore_cron_jobs_all_profiles ══════════════
add("restore_cron_jobs_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_cron_jobs_if_emptied": {"restored": True, "count": 2}})
add("restore_cron_jobs_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_cron_jobs_if_emptied": [None, {"restored": True, "count": 1}]})
add("restore_cron_jobs_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_cron_jobs_if_emptied": rs("OSError", "cron boom")})
add("restore_cron_jobs_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_cron_jobs_if_emptied": rs("GeneratorExit", "")})
add("restore_cron_jobs_all_profiles", {"profile_snapshots": {}, "invoking_home": "/h"}, {})
add("restore_cron_jobs_all_profiles", {"profile_snapshots": SNAPS, "invoking_home": "/h"},
    {"sibling_profile_homes": arr(SIB), "restore_cron_jobs_if_emptied": rs("AttributeError", "a")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
