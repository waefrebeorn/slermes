#!/usr/bin/env python3
"""Generate tests/fixtures/backup_flow3.jsonl -- scripted-seam FLOW cases for
the 26 unclaimed backup.py path/filesystem defs ported in backup_flow3.c.

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that builtin in the live drive and
is delivered as a raise descriptor to the C universal seam. A MISSING seam key
fail-closes (RuntimeError) on BOTH sides; a present JSON null is a valid None.
{"__arr__": v} is an explicit array VALUE, not a queue.

Run: python tests/gen_backup_flow3_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "backup_flow3.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── 1. is_non_regular_path ──
add("is_non_regular_path", {"path": "a.txt"}, {"lstat": {"mode": 0o100644}})
add("is_non_regular_path", {"path": "dir"}, {"lstat": {"mode": 0o40755}})
add("is_non_regular_path", {"path": "link"}, {"lstat": {"mode": 0o120777}})
add("is_non_regular_path", {"path": "sock"}, {"lstat": {"mode": 0o140600}})
add("is_non_regular_path", {"path": "gone"}, {"lstat": rs("FileNotFoundError", "gone")})
add("is_non_regular_path", {"path": "odd"}, {"lstat": rs("ValueError", "v")})

# ── 2. is_link_path ──
add("is_link_path", {"path": "a.txt", "os_name_nt": False}, {"lstat": {"mode": 0o100644}})
add("is_link_path", {"path": "link", "os_name_nt": False}, {"lstat": {"mode": 0o120777}})
add("is_link_path", {"path": "junction", "os_name_nt": True},
    {"lstat": {"mode": 0o40755, "st_file_attributes": 0x400}})
add("is_link_path", {"path": "junction", "os_name_nt": False},
    {"lstat": {"mode": 0o40755, "st_file_attributes": 0x400}})
add("is_link_path", {"path": "gone", "os_name_nt": True}, {"lstat": rs("PermissionError", "no")})
add("is_link_path", {"path": "odd", "os_name_nt": True}, {"lstat": rs("RuntimeError", "r")})

# ── 3. snapshot_db ──
add("snapshot_db", {"src": "s", "dst": "d"},
    {"is_store_file": False, "safe_copy_db": True})
add("snapshot_db", {"src": "s", "dst": "d"},
    {"is_store_file": False, "safe_copy_db": False})
add("snapshot_db", {"src": "s", "dst": "d"},
    {"is_store_file": True, "open_store_admin": {"unsupported": False}, "admin_backup_to": True})
add("snapshot_db", {"src": "s", "dst": "d"},
    {"is_store_file": True, "open_store_admin": {"unsupported": False}, "admin_backup_to": False})
add("snapshot_db", {"src": "s", "dst": "d"},
    {"is_store_file": True, "open_store_admin": {"unsupported": True}})
add("snapshot_db", {"src": "s", "dst": "d"},
    {"is_store_file": rs("OSError", "stat")})

# ── 4. discard_failed_zip_members ──
E = [{"name": "a.txt"}, {"name": "b.txt"}, {"name": "c.txt"}]
add("discard_failed_zip_members", {"zf": "z", "filelist_len": 3},
    {"filelist_len": 3})
add("discard_failed_zip_members", {"zf": "z", "filelist_len": 1, "entries": E},
    {"filelist_len": 3, "header_offset": 4096, "fp_seek": None,
     "fp_truncate": None, "nametoinfo": None})
add("discard_failed_zip_members", {"zf": "z", "filelist_len": 0, "entries": E},
    {"filelist_len": rs("OSError", "len")})
add("discard_failed_zip_members", {"zf": "z", "filelist_len": 1, "entries": E},
    {"filelist_len": 3, "header_offset": rs("ValueError", "off")})
add("discard_failed_zip_members", {"zf": "z", "filelist_len": 1, "entries": E},
    {"filelist_len": 3, "header_offset": 7, "fp_seek": rs("OSError", "seek")})
add("discard_failed_zip_members", {"zf": "z", "filelist_len": 1, "entries": E},
    {"filelist_len": 3, "header_offset": 7, "fp_seek": None, "fp_truncate": None,
     "nametoinfo": rs("RuntimeError", "nti")})

# ── 5. zip_sqlite_snapshot ──
ZS = {"abs_path": "/h/state.db", "rel_path": "state.db", "out_path": "/h/out.zip"}
add("zip_sqlite_snapshot", ZS,
    {"staged_backup": "/tmp/s.db", "snapshot_db": True,
     "write_zip_file": None, "stat_size": 1234})
add("zip_sqlite_snapshot", ZS,
    {"staged_backup": "/tmp/s.db", "snapshot_db": False, "warn_legacy": None})
add("zip_sqlite_snapshot", ZS, {"staged_backup": rs("OSError", "stage")})
add("zip_sqlite_snapshot", ZS,
    {"staged_backup": "/tmp/s.db", "snapshot_db": True,
     "write_zip_file": rs("OSError", "write")})
add("zip_sqlite_snapshot", ZS,
    {"staged_backup": "/tmp/s.db", "snapshot_db": True,
     "write_zip_file": None, "stat_size": rs("OSError", "stat")})
add("zip_sqlite_snapshot", ZS,
    {"staged_backup": "/tmp/s.db", "snapshot_db": rs("ValueError", "snap")})

# ── 6. write_zip_entries ──
add("write_zip_entries", {"files": [["/h/a.txt", "a.txt"]], "track_bytes": False},
    {"warn_legacy": None, "write_zip_file": None})
add("write_zip_entries", {"files": [["/h/a.txt", "a.txt"]], "track_bytes": True},
    {"warn_legacy": None, "write_zip_file": None, "stat_size": 50})
add("write_zip_entries", {"files": [["/h/s.db", "s.db"]], "track_bytes": False},
    {"zip_sqlite_snapshot": 100})
add("write_zip_entries", {"files": [["/h/s.db", "s.db"]], "track_bytes": False},
    {"zip_sqlite_snapshot": None, "db_failure": None})
add("write_zip_entries", {"files": [["/h/s.db", "s.db"]], "track_bytes": False},
    {"zip_sqlite_snapshot": None, "db_failure": rs("ValueError", "db fail"),
     "on_error": None})
add("write_zip_entries", {"files": [["/h/a.txt", "a.txt"]], "track_bytes": False},
    {"warn_legacy": None, "write_zip_file": rs("OSError", "read fail"), "on_error": None})
add("write_zip_entries", {"files": [["/h/a.txt", "a.txt"], ["/h/s.db", "s.db"]], "track_bytes": True},
    {"warn_legacy": None, "write_zip_file": None, "stat_size": [10, None],
     "zip_sqlite_snapshot": 200})

# ── 7. resolve_backup_output_path ──
add("resolve_backup_output_path", {"output": "/h/x.zip", "suffix": ".zip", "default_name": "snap.zip"},
    {"resolve_output": "/h/x.zip", "is_dir": False, "mkdir": None})
add("resolve_backup_output_path", {"output": "/h/backups", "suffix": ".zip", "default_name": "snap.zip"},
    {"resolve_output": "/h/backups", "is_dir": True, "mkdir": None})
add("resolve_backup_output_path", {"output": "/h/x.tar", "suffix": ".tar", "default_name": "snap.zip"},
    {"resolve_output": "/h/x.tar", "is_dir": False,
     "with_suffix": "/h/x.tar.zip", "mkdir": None})
add("resolve_backup_output_path", {"output": "/no/x.zip", "suffix": ".zip", "default_name": "snap.zip"},
    {"resolve_output": rs("PermissionError", "denied"),
     "print": None})
add("resolve_backup_output_path", {"output": "/h/x.zip", "suffix": ".zip", "default_name": "snap.zip"},
    {"resolve_output": "/h/x.zip", "is_dir": False,
     "mkdir": rs("OSError", "mkdir fail"), "print": None})
add("resolve_backup_output_path", {"output": "/h/x.zip", "suffix": ".zip", "default_name": "snap.zip"},
    {"resolve_output": rs("ValueError", "bad")})

# ── 8. collect_external_entries ──
add("collect_external_entries", {},
    {"home_resolve": "/h", "external_paths": [["/h/.honcho"]],
     "base_resolve": True, "iter_external_files": [["honcho/config.json"]],
     "fpath_resolve": "honcho/config.json"})
add("collect_external_entries", {},
    {"home_resolve": "/h", "external_paths": [["/outside"]],
     "base_resolve": rs("ValueError", "outside")})
add("collect_external_entries", {},
    {"home_resolve": "/h", "external_paths": [["/h/.honcho"]],
     "base_resolve": True, "iter_external_files": [["a", "b"]],
     "fpath_resolve": ["honcho/a", "honcho/b"]})
add("collect_external_entries", {},
    {"home_resolve": "/h", "external_paths": [["/h/.honcho"]],
     "base_resolve": True, "iter_external_files": [["a"]],
     "fpath_resolve": rs("OSError", "skip")})
add("collect_external_entries", {},
    {"home_resolve": "/h", "external_paths": {"__arr__": []}})
add("collect_external_entries", {},
    {"home_resolve": "/h", "external_paths": [["/h/.honcho"]],
     "base_resolve": rs("OSError", "boom")})

# ── 9. run_import (preflight) ──
add("run_import", {"zipfile": "b.zip"},
    {"require_file_tooling": None, "expanduser_resolve": "/h/b.zip",
     "is_file": False, "print": None})
add("run_import", {"zipfile": "b.zip"},
    {"require_file_tooling": None, "expanduser_resolve": "/h/b.zip",
     "is_file": True, "is_zipfile": False, "print": None})
add("run_import", {"zipfile": "b.zip"},
    {"require_file_tooling": None, "expanduser_resolve": "/h/b.zip",
     "is_file": True, "is_zipfile": True,
     "validate_zip": {"ok": False, "reason": "bad manifest"}, "print": None})
add("run_import", {"zipfile": "b.zip"}, {"require_file_tooling": rs("OSError", "no tool")})
add("run_import", {"zipfile": "b.zip"},
    {"require_file_tooling": None, "expanduser_resolve": rs("RuntimeError", "boom")})
add("run_import", {"zipfile": "b.zip"},
    {"require_file_tooling": None, "expanduser_resolve": "/h/b.zip",
     "is_file": True, "is_zipfile": True, "validate_zip": rs("ValueError", "v")})

# ── 10. import_members ──
add("import_members", {"hermes_root": "/h", "members": [], "file_count": 0},
    {"resolve_root": "/h", "keep_host_homes": [],
     "import_members_exclusive": [1, 0, [], [], []]})
add("import_members", {"hermes_root": "/h", "members": ["config.yaml"], "file_count": 1},
    {"resolve_root": "/h", "member_parts": ["config.yaml"], "keep_host_homes": [],
     "import_members_exclusive": [1, 0, [], [], []]})
add("import_members", {"hermes_root": "/h", "members": ["a/b/c.txt"], "file_count": 1},
    {"resolve_root": "/h", "member_parts": ["a", "b", "c.txt"], "keep_host_homes": [],
     "import_members_exclusive": [1, 0, [], [], []]})
add("import_members", {"hermes_root": "/h", "members": ["_external/x"], "file_count": 1},
    {"resolve_root": "/h", "keep_host_homes": [],
     "import_members_exclusive": [0, 1, [], [], []]})
add("import_members", {"hermes_root": "/h", "members": ["config.yaml"], "file_count": 1, "keep_host": True},
    {"resolve_root": "/h", "member_parts": ["config.yaml"],
     "keep_host_homes": ["/h/p1"], "import_members_exclusive": [2, 0, [], [], []]})
add("import_members", {"hermes_root": "/h", "members": ["config.yaml"], "file_count": 1},
    {"resolve_root": "/h", "member_parts": rs("ValueError", "traversal")})

# ── 11. import_external_member ──
add("import_external_member", {"member": "_external/honcho/config.json", "home_dir": "/h", "secret": True},
    {"relative_to": True, "mkdir": None, "extract": None, "chmod": None})
add("import_external_member", {"member": "_external/notes.txt", "home_dir": "/h", "secret": False},
    {"relative_to": True, "mkdir": None, "extract": None})
add("import_external_member", {"member": "_external/../x", "home_dir": "/h", "secret": False},
    {"relative_to": rs("ValueError", "traversal")})
add("import_external_member", {"member": "_external/a/b", "home_dir": "/h", "secret": False},
    {"relative_to": True, "mkdir": rs("OSError", "denied")})
add("import_external_member", {"member": "_external/a/b", "home_dir": "/h", "secret": False},
    {"relative_to": True, "mkdir": None, "extract": rs("OSError", "bad member")})
add("import_external_member", {"member": "_external/config.json", "home_dir": "/h", "secret": True},
    {"relative_to": True, "mkdir": None, "extract": None, "chmod": rs("OSError", "chmod")})

# ── 12. import_members_exclusive ──
add("import_members_exclusive", {"hermes_root": "/h", "members": [], "file_count": 0},
    {"home_resolve": "/h", "default_mode": 384})
add("import_members_exclusive", {"hermes_root": "/h", "members": ["_external/a"], "file_count": 1},
    {"home_resolve": "/h", "default_mode": 384,
     "import_external_member": [[True, None]]})
add("import_members_exclusive", {"hermes_root": "/h", "members": ["_external/a"], "file_count": 1},
    {"home_resolve": "/h", "default_mode": 384,
     "import_external_member": [[False, "err"]]})
add("import_members_exclusive", {"hermes_root": "/h", "members": ["_external/a", "_external/b"], "file_count": 2},
    {"home_resolve": "/h", "default_mode": 384,
     "import_external_member": [[True, None], [True, None]]})
add("import_members_exclusive", {"hermes_root": "/h", "members": [], "file_count": 0},
    {"home_resolve": rs("OSError", "home")})

# ── 13. revive_gateway_after_import ──
add("revive_gateway_after_import", {"hermes_root": "/h", "native_default": "/h"},
    {"native_default": "/h", "service_running": True})
add("revive_gateway_after_import", {"hermes_root": "/h", "native_default": "/h"},
    {"native_default": "/h", "service_running": False, "print": None, "ensure_service": None})
add("revive_gateway_after_import", {"hermes_root": "/sandbox", "native_default": "/h"},
    {"native_default": "/h", "marker_exists": True, "print": None})
add("revive_gateway_after_import", {"hermes_root": "/sandbox", "native_default": "/h"},
    {"native_default": "/h", "marker_exists": False, "store_exists": False,
     "service_running": True})
add("revive_gateway_after_import", {"hermes_root": "/h", "native_default": "/h"},
    {"native_default": "/h", "service_running": rs("RuntimeError", "boom"),
     "logger_debug": None, "print": None})
add("revive_gateway_after_import", {"hermes_root": "/h", "native_default": "/h"},
    {"native_default": "/h", "service_running": rs("KeyboardInterrupt", "")})

# ── 14. newest_first ──
EM = {"a.zip": {"keep": True}, "b.txt": {"keep": False}, "c.zip": {"keep": True}}
add("newest_first", {"root": "/h/backups", "entries_map": EM},
    {"path_exists": True, "path_iterdir": {"__arr__": [{"name": "a.zip", "keep": True},
                                                       {"name": "b.txt", "keep": False},
                                                       {"name": "c.zip", "keep": True}]}})
add("newest_first", {"root": "/h/backups", "entries_map": {}},
    {"path_exists": False})
add("newest_first", {"root": "/h/backups", "entries_map": {}},
    {"path_exists": rs("OSError", "exists")})
add("newest_first", {"root": "/h/backups", "entries_map": {}},
    {"path_exists": True, "path_iterdir": rs("OSError", "iter")})
add("newest_first", {"root": "/h/backups", "entries_map": {"only": {"keep": True}}},
    {"path_exists": True, "path_iterdir": {"__arr__": [{"name": "only", "keep": True}]}})

# ── 15. prune_oldest ──
add("prune_oldest", {"names": ["c.zip", "b.zip", "a.zip"], "keep": 1, "what": "backup"},
    {"unlink": None})
add("prune_oldest", {"names": ["c.zip", "b.zip", "a.zip"], "keep": 0, "what": "backup"},
    {"unlink": rs("PermissionError", "locked"), "logger_warning": None})
add("prune_oldest", {"names": ["b.zip", "a.zip"], "keep": 1, "what": "backup"}, {})
add("prune_oldest", {"names": ["b.zip", "a.zip"], "keep": 0, "what": "backup"},
    {"unlink": rs("ValueError", "v")})
add("prune_oldest", {"names": [], "keep": 0, "what": "x"}, {})

# ── 16. create_prefixed_full_backup ──
add("create_prefixed_full_backup",
    {"hermes_home": "/h", "prefix": "pre-", "keep": 2, "what": "update", "prune_what": "backup"},
    {"is_dir": True, "mkdir": None, "chmod": None, "write_full_zip": "/h",
     "prune_prefixed": None})
add("create_prefixed_full_backup",
    {"hermes_home": "/h", "prefix": "pre-", "keep": 2, "what": "update", "prune_what": "backup"},
    {"is_dir": False})
add("create_prefixed_full_backup",
    {"hermes_home": "/h", "prefix": "pre-", "keep": 2, "what": "update", "prune_what": "backup"},
    {"is_dir": True, "mkdir": rs("OSError", "no dir"), "logger_warning": None})
add("create_prefixed_full_backup",
    {"hermes_home": "/h", "prefix": "pre-", "keep": 2, "what": "update", "prune_what": "backup"},
    {"is_dir": True, "mkdir": None, "chmod": None, "write_full_zip": None,
     "prune_incomplete": None})
add("create_prefixed_full_backup",
    {"hermes_home": "/h", "prefix": "pre-", "keep": 2, "what": "update", "prune_what": "backup"},
    {"is_dir": rs("OSError", "dir")})

# ── 17. too_large ──
add("too_large", {"path": "/h/a", "rel_name": "a", "max_file_size": 100,
                  "fmt_size": "50", "fmt_max": "100"},
    {"stat_size": 50})
add("too_large", {"path": "/h/a", "rel_name": "a", "max_file_size": 100,
                  "fmt_size": "200", "fmt_max": "100"},
    {"stat_size": 200, "print": None, "logger_warning": None})
add("too_large", {"path": "/h/a", "rel_name": "a", "max_file_size": None}, {})
add("too_large", {"path": "/h/a", "rel_name": "a", "max_file_size": 100},
    {"stat_size": rs("OSError", "no stat")})
add("too_large", {"path": "/h/a", "rel_name": "a", "max_file_size": 100},
    {"stat_size": rs("ValueError", "v")})
add("too_large", {"path": "/h/a", "rel_name": "a", "max_file_size": 100,
                  "fmt_size": "200", "fmt_max": "100"},
    {"stat_size": 200, "print": rs("RuntimeError", "print")})

# ── 18. is_trusted_root_auth_alias ──
add("is_trusted_root_auth_alias", {"path": "/p/auth.json", "home": "/p"},
    {"is_symlink": False})
add("is_trusted_root_auth_alias", {"path": "/p/auth.json", "home": "/p"},
    {"is_symlink": True, "get_default_root": "/root", "resolve_equal": True})
add("is_trusted_root_auth_alias", {"path": "/p/auth.json", "home": "/p"},
    {"is_symlink": True, "get_default_root": "/root", "resolve_equal": False,
     "resolve_equal_trusted": True})
add("is_trusted_root_auth_alias", {"path": "/p/auth.json", "home": "/p"},
    {"is_symlink": True, "get_default_root": "/root", "resolve_equal": False,
     "resolve_equal_trusted": False})
add("is_trusted_root_auth_alias", {"path": "/p/auth.json", "home": "/p"},
    {"is_symlink": True, "get_default_root": rs("OSError", "root")})
add("is_trusted_root_auth_alias", {"path": "/p/auth.json", "home": "/p"},
    {"is_symlink": True, "get_default_root": "/root",
     "resolve_equal": rs("ValueError", "rel")})

# ── 19. quick_snapshot_dir ──
add("quick_snapshot_dir", {"snapshot_id": "snap1", "hermes_home": "/h"},
    {"snapshot_root": "ROOT", "relative_to": True, "is_dir": True,
     "manifest_exists": True})
add("quick_snapshot_dir", {"snapshot_id": "snap1", "hermes_home": "/h"},
    {"snapshot_root": "ROOT", "relative_to": True, "is_dir": False})
add("quick_snapshot_dir", {"snapshot_id": "snap1", "hermes_home": "/h"},
    {"snapshot_root": "ROOT", "relative_to": True, "is_dir": True,
     "manifest_exists": False})
add("quick_snapshot_dir", {"snapshot_id": "../evil", "hermes_home": "/h"},
    {"snapshot_root": "ROOT", "logger_error": None})
add("quick_snapshot_dir", {"snapshot_id": "", "hermes_home": "/h"},
    {"snapshot_root": "ROOT", "logger_error": None})
add("quick_snapshot_dir", {"snapshot_id": "snap1", "hermes_home": "/h"},
    {"snapshot_root": "ROOT", "relative_to": rs("ValueError", "traversal"),
     "logger_error": None})

# ── 20. restore_quick_snapshot_exclusive ──
add("restore_quick_snapshot_exclusive", {"snapshot_id": "none", "home": "/h"},
    {"quick_snapshot_dir": None})
add("restore_quick_snapshot_exclusive", {"snapshot_id": "", "home": "/h"},
    {"quick_snapshot_dir": None})
add("restore_quick_snapshot_exclusive", {"snapshot_id": "../x", "home": "/h"},
    {"quick_snapshot_dir": None})
add("restore_quick_snapshot_exclusive", {"snapshot_id": "z", "home": "/p"},
    {"quick_snapshot_dir": None})
add("restore_quick_snapshot_exclusive", {"snapshot_id": "a", "home": "/h"},
    {"quick_snapshot_dir": rs("OSError", "boom")})

# ── 21. entry_paths ──
add("entry_paths", {"rel": "config.yaml"},
    {"resolve_src": True, "resolve_dst": True})
add("entry_paths", {"rel": "profiles/a/state.db"},
    {"resolve_src": True, "resolve_dst": True})
add("entry_paths", {"rel": "../evil"},
    {"resolve_src": False})
add("entry_paths", {"rel": "auth.json"},
    {"resolve_src": True, "resolve_dst": False, "trusted_alias": True})
add("entry_paths", {"rel": "auth.json"},
    {"resolve_src": True, "resolve_dst": False, "trusted_alias": False})
add("entry_paths", {"rel": "x"},
    {"resolve_src": rs("OSError", "src")})

# ── 22. restore_config_model_settings_if_rewritten ──
PROT = ["model.provider", "model.default", "moa"]
add("restore_config_model_settings_if_rewritten",
    {"snapshot_id": "", "hermes_home": "/h", "protected": PROT}, {})
add("restore_config_model_settings_if_rewritten",
    {"snapshot_id": "s1", "hermes_home": "/h", "protected": PROT},
    {"supports_file_tooling": False})
add("restore_config_model_settings_if_rewritten",
    {"snapshot_id": "s1", "hermes_home": "/h", "protected": PROT},
    {"supports_file_tooling": True, "read_yaml": {}})
add("restore_config_model_settings_if_rewritten",
    {"snapshot_id": "s1", "hermes_home": "/h", "protected": PROT},
    {"supports_file_tooling": True, "read_yaml": [{"v": 1}, None]})
add("restore_config_model_settings_if_rewritten",
    {"snapshot_id": "s1", "hermes_home": "/h", "protected": PROT},
    {"supports_file_tooling": True, "read_yaml": [{"v": 1}, {"v": 1}],
     "get_config_value": ["anthropic", "anthropic"]})
add("restore_config_model_settings_if_rewritten",
    {"snapshot_id": "s1", "hermes_home": "/h", "protected": PROT},
    {"supports_file_tooling": True, "read_yaml": [{"v": 1}, {"v": 2}],
     "get_config_value": ["anthropic", "openai"],
     "set_config_value": None, "atomic_write": None, "logger_warning": None})

# ── 23. db_failure ──
add("db_failure", {"rel_path": "state.db"}, {"logger_warning": None})
add("db_failure", {"rel_path": "profiles/a/state.db"}, {"logger_warning": None})
add("db_failure", {"rel_path": "x.db"}, {"logger_warning": rs("OSError", "log")})
add("db_failure", {"rel_path": "a.db"}, {"logger_warning": [None, None]})
add("db_failure", {"rel_path": "b.db"}, {"logger_warning": rs("RuntimeError", "boom")})

# ── 24. capped_errors ──
add("capped_errors", {"errors": []}, {})
add("capped_errors", {"errors": ["e1", "e2"]}, {})
add("capped_errors", {"errors": ["e%d" % i for i in range(10)]}, {})
add("capped_errors", {"errors": ["e%d" % i for i in range(11)]}, {})
add("capped_errors", {"errors": ["e%d" % i for i in range(15)]}, {})

# ── 25. publish_path ──
add("publish_path", {"errors": [], "nfiles": 5, "out_path": "/h/out.zip", "salvage_path": "/h/s.zip"}, {})
add("publish_path", {"errors": ["e1"], "nfiles": 5, "out_path": "/h/out.zip", "salvage_path": "/h/s.zip"}, {})
add("publish_path", {"errors": ["e1", "e2"], "nfiles": 2, "out_path": "/h/out.zip", "salvage_path": "/h/s.zip"}, {})
add("publish_path", {"errors": ["e1"], "nfiles": 1, "out_path": "/h/out.zip", "salvage_path": "/h/s.zip"}, {})
add("publish_path", {"errors": [], "nfiles": 0, "out_path": "/h/out.zip", "salvage_path": "/h/s.zip"}, {})

# ── 26. progress ──
add("progress", {"i": 1, "file_count": 10}, {"print": None, "logger_info": None})
add("progress", {"i": 500, "file_count": 1000}, {"print": None, "logger_info": None})
add("progress", {"i": 2, "file_count": 3}, {"print": rs("OSError", "print")})
add("progress", {"i": 3, "file_count": 3}, {"print": None, "logger_info": rs("RuntimeError", "log")})
add("progress", {"i": 10, "file_count": 10}, {"print": None, "logger_info": None})


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
