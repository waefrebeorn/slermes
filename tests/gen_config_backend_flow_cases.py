#!/usr/bin/env python3
"""Generate tests/fixtures/config_backend_flow.jsonl for the config_backend_flow
FLOW oracle (hermes_cli/config_backend.py). >=5 cases per ported function,
exercising decision branches, error paths, BaseException propagation and
fail-closed missing scripts. Script semantics match the harness/oracle:
array = per-call queue (last repeats), single value = constant, missing = None,
{"__raise__": cls, "msg": ...} = exception. ASCII / POSIX-lexical domain.

Domain notes: path_stat and open are always scripted when reached (a missing
path_stat script would project to an empty stat double on the PY side); the
fail-closed missing-script cases use the route / backend_* / get_config_backend
seams where both sides agree. Changes objects use canonical key order
(document, set, unset, extra_content_on_create).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_backend_flow.jsonl")

CP = "/h/u/config.yaml"
HOME = "/h/u"
EXPLICIT = "/h/u/other.yaml"


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def locked(path="model", by="managed", msg=None):
    m = msg or ("%s is locked by the %s level" % (path, by))
    return {"__raise__": "ConfigLockedError", "msg": m, "path": path, "locked_by": by}


ROUTE = {"backend": {"name": "remote"}, "home": HOME}
ROUTE_FILE = {"backend": {"name": "file"}, "home": HOME}
STAT = {"st_mode": 0o644, "st_mtime_ns": 1000, "st_size": 5, "st_ino": 1}
SIG = [1000, 5, 1, 0]
DOC = {"model": "x", "provider": "p"}
CH_EMPTY = {"document": None, "set": {}, "unset": [], "extra_content_on_create": None}
CH_SET = {"document": None, "set": {"model": "y", "provider": None}, "unset": ["old"],
          "extra_content_on_create": None}
CH_DOC = {"document": {"a": 1}, "set": {}, "unset": [], "extra_content_on_create": "# hi\n"}

CASES = []


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# ── read_config_doc_readonly / config_version / probe_config_readable /
#    config_exists / config_destination (route + one backend method) ──
for func, seam_name, ret in [
    ("read_config_doc_readonly", "backend_read_user_doc_readonly", DOC),
    ("config_version", "backend_version", SIG),
    ("probe_config_readable", "backend_probe_readable", None),
    ("config_exists", "backend_exists", True),
    ("config_destination", "backend_destination", CP),
]:
    add(func, {"config_path": CP}, {"route": ROUTE, seam_name: ret})
    add(func, {"config_path": CP}, {"route": ROUTE_FILE, seam_name: ret})
    add(func, {"config_path": CP}, {"route": R("FileNotFoundError", "no config"), seam_name: ret})
    add(func, {"config_path": CP}, {"route": R("SystemExit", "bye"), seam_name: ret})
    add(func, {"config_path": CP}, {"route": ROUTE, seam_name: R("OSError", "io")})
    add(func, {"config_path": CP}, {"route": ROUTE, seam_name: R("ConfigBackendUnavailable", "no backend")})
    add(func, {"config_path": CP}, {"route": ROUTE})  # missing backend script -> None

# config_exists boolean variety
add("config_exists", {"config_path": CP}, {"route": ROUTE, "backend_exists": False})
add("config_exists", {"config_path": EXPLICIT}, {"route": ROUTE, "backend_exists": True})

# ── config_version_or_none (catches OSError only) ──
add("config_version_or_none", {"config_path": CP}, {"route": ROUTE, "backend_version": SIG})
add("config_version_or_none", {"config_path": CP}, {"route": ROUTE, "backend_version": R("FileNotFoundError", "absent")})
add("config_version_or_none", {"config_path": CP}, {"route": ROUTE, "backend_version": R("PermissionError", "denied")})
add("config_version_or_none", {"config_path": CP}, {"route": ROUTE, "backend_version": R("SystemExit", "bye")})
add("config_version_or_none", {"config_path": CP}, {"route": ROUTE, "backend_version": R("ConfigBackendUnavailable", "nb")})
add("config_version_or_none", {"config_path": CP}, {"route": R("OSError", "route io")})
add("config_version_or_none", {"config_path": CP}, {"route": ROUTE})

# ── config_cache_version (config_version then route + cache_version) ──
add("config_cache_version", {"config_path": CP},
    {"route": ROUTE, "backend_version": SIG, "backend_cache_version": SIG + ["d"]})
add("config_cache_version", {"config_path": CP},
    {"route": ROUTE, "backend_version": R("FileNotFoundError", "absent"), "backend_cache_version": SIG + ["d"]})
add("config_cache_version", {"config_path": CP},
    {"route": [ROUTE, R("OSError", "second route")], "backend_version": SIG, "backend_cache_version": SIG + ["d"]})
add("config_cache_version", {"config_path": CP},
    {"route": ROUTE, "backend_version": SIG, "backend_cache_version": R("OSError", "digest io")})
add("config_cache_version", {"config_path": CP},
    {"route": ROUTE, "backend_version": SIG, "backend_cache_version": R("SystemExit", "bye")})
add("config_cache_version", {"config_path": CP},
    {"route": ROUTE, "backend_version": SIG})

# ── write_config_document ──
add("write_config_document", {"config_path": CP, "document": {"a": 1}, "extra_content_on_create": "# hi\n"},
    {"route": ROUTE, "backend_write_changes": None})
add("write_config_document", {"config_path": CP, "document": None, "extra_content_on_create": None},
    {"route": ROUTE, "backend_write_changes": None})
add("write_config_document", {"config_path": CP, "document": {"a": 1}},
    {"route": R("OSError", "route io")})
add("write_config_document", {"config_path": CP, "document": {"a": 1}},
    {"route": ROUTE, "backend_write_changes": locked()})
add("write_config_document", {"config_path": CP, "document": {"a": 1}},
    {"route": ROUTE, "backend_write_changes": R("SystemExit", "bye")})
add("write_config_document", {"config_path": EXPLICIT, "document": {"b": 2}, "extra_content_on_create": None},
    {"route": ROUTE_FILE, "backend_write_changes": None})

# ── write_config_key (claimed write_config_keys seam) ──
add("write_config_key", {"config_path": CP, "key_path": "model", "value": "y"},
    {"write_config_keys": None})
add("write_config_key", {"config_path": CP, "key_path": "model", "value": None},
    {"write_config_keys": None})
add("write_config_key", {"config_path": CP, "key_path": "a.b", "value": 3},
    {"write_config_keys": R("ConfigValueError", "bad value")})
add("write_config_key", {"config_path": CP, "key_path": "model", "value": "y"},
    {"write_config_keys": R("SystemExit", "bye")})
add("write_config_key", {"config_path": EXPLICIT, "key_path": "k", "value": "v"},
    {"write_config_keys": None})
add("write_config_key", {"config_path": CP, "key_path": "model", "value": "y"})

# ── keeping_read_base (get_config_backend + preserve_read_base + read_config_doc) ──
add("keeping_read_base", {"config_path": CP},
    {"get_config_backend": {"name": "file"}, "backend_preserve_read_base": {"__cm__": "nullcontext"},
     "read_config_doc": DOC})
add("keeping_read_base", {"config_path": CP},
    {"get_config_backend": {"name": "remote"}, "backend_preserve_read_base": {"__cm__": "nullcontext"},
     "read_config_doc": R("FileNotFoundError", "absent")})
add("keeping_read_base", {"config_path": CP},
    {"get_config_backend": R("SystemExit", "bye")})
add("keeping_read_base", {"config_path": CP},
    {"get_config_backend": {"name": "remote"}, "backend_preserve_read_base": R("OSError", "cm")})
add("keeping_read_base", {"config_path": CP},
    {"get_config_backend": {"name": "file"}, "backend_preserve_read_base": {"__cm__": "nullcontext"},
     "read_config_doc": None})
add("keeping_read_base", {"config_path": CP},
    {"get_config_backend": {"name": "file"}, "read_config_doc": DOC})

# ── write_changes (module flow: refuse_redacted_sets + route + write_changes) ──
add("write_changes_flow", {"config_path": CP, "changes": CH_SET},
    {"refuse_redacted_sets": None, "route": ROUTE, "backend_write_changes": None})
add("write_changes_flow", {"config_path": CP, "changes": CH_EMPTY},
    {"refuse_redacted_sets": None, "route": ROUTE, "backend_write_changes": None})
add("write_changes_flow", {"config_path": CP, "changes": CH_SET},
    {"refuse_redacted_sets": R("ConfigValueError", "redacted"), "route": ROUTE, "backend_write_changes": None})
add("write_changes_flow", {"config_path": CP, "changes": CH_DOC},
    {"refuse_redacted_sets": None, "route": R("OSError", "io"), "backend_write_changes": None})
add("write_changes_flow", {"config_path": CP, "changes": CH_SET},
    {"refuse_redacted_sets": None, "route": ROUTE, "backend_write_changes": locked()})
add("write_changes_flow", {"config_path": CP, "changes": CH_SET},
    {"refuse_redacted_sets": None, "route": ROUTE, "backend_write_changes": R("SystemExit", "bye")})
add("write_changes_flow", {"config_path": CP, "changes": CH_DOC},
    {"refuse_redacted_sets": None, "route": ROUTE_FILE, "backend_write_changes": None})

# ── deployment_env_names / supports_file_tooling (module) ──
for func, seam_name, ret in [
    ("deployment_env_names_flow", "backend_deployment_env_names", ["HERMES_CONFIG_BACKEND", "HERMES_PLANE"]),
    ("supports_file_tooling_flow", "backend_supports_file_tooling", True),
]:
    add(func, {}, {"get_config_backend": {"name": "remote"}, seam_name: ret})
    add(func, {}, {"get_config_backend": {"name": "file"}, seam_name: ret})
    add(func, {}, {"get_config_backend": R("SystemExit", "bye")})
    add(func, {}, {"get_config_backend": {"name": "remote"}, seam_name: R("OSError", "io")})
    add(func, {}, {"get_config_backend": {"name": "remote"}})
add("deployment_env_names_flow", {}, {"get_config_backend": {"name": "file"}, "backend_deployment_env_names": []})
add("supports_file_tooling_flow", {}, {"get_config_backend": {"name": "remote"}, "backend_supports_file_tooling": False})

# ── require_file_tooling ──
add("require_file_tooling", {"what": "profile clone"},
    {"get_config_backend": {"name": "file"}, "backend_supports_file_tooling": True})
add("require_file_tooling", {"what": "profile clone"},
    {"get_config_backend": [{"name": "remote"}, {"name": "remote"}], "backend_supports_file_tooling": False})
add("require_file_tooling", {"what": "backup"},
    {"get_config_backend": [{"name": "file"}, {"name": "file"}], "backend_supports_file_tooling": False})
add("require_file_tooling", {"what": "config edit"},
    {"get_config_backend": R("SystemExit", "bye")})
add("require_file_tooling", {"what": "restore"},
    {"get_config_backend": [{"name": "remote"}, R("OSError", "second")], "backend_supports_file_tooling": False})
add("require_file_tooling", {"what": "clone"},
    {"get_config_backend": {"name": "remote"}})  # missing supports -> falsy -> raise

# ── _bootstrap_names ──
add("bootstrap_names", {"kind": "remote"},
    {"backend_for": {"name": "remote"}, "backend_bootstrap_env_names": ["A", "B"]})
add("bootstrap_names", {"kind": "file"},
    {"backend_for": {"name": "file"}, "backend_bootstrap_env_names": []})
add("bootstrap_names", {"kind": "nope"},
    {"backend_for": R("ConfigBackendUnavailable", "unknown")})
add("bootstrap_names", {"kind": "remote"},
    {"backend_for": R("SystemExit", "bye")})
add("bootstrap_names", {"kind": "remote"},
    {"backend_for": R("ImportError", "broken backend")})
add("bootstrap_names", {"kind": "remote"},
    {"backend_for": {"name": "remote"}, "backend_bootstrap_env_names": R("OSError", "io")})

# ── bootstrap_deployment ──
add("bootstrap_deployment", {"home": HOME, "project_env": "/h/u/.env"}, {"bootstrap": None})
add("bootstrap_deployment", {"home": None, "project_env": None}, {"bootstrap": None})
add("bootstrap_deployment", {"home": HOME, "project_env": None}, {"bootstrap": R("OSError", "io")})
add("bootstrap_deployment", {"home": HOME, "project_env": "/p/.env"}, {"bootstrap": R("SystemExit", "bye")})
add("bootstrap_deployment", {"home": None, "project_env": "/p/.env"}, {"bootstrap": None})
add("bootstrap_deployment", {"home": HOME, "project_env": "/h/u/.env"})

# ── local_file_layer (pure) ──
add("local_file_layer", {"config_path": CP})
add("local_file_layer", {"config_path": EXPLICIT})
add("local_file_layer", {"config_path": "/a/b/c.yaml"})
add("local_file_layer", {"config_path": "rel.yaml"})
add("local_file_layer", {"config_path": "/only.yaml"})
add("local_file_layer", {"config_path": "/h/u/sub/config.yaml"})

# ── drop_other_access ──
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": False, "path_stat": {"st_mode": 0o644}})
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": False, "path_stat": {"st_mode": 0o666},
     "os_chmod": None})
add("drop_other_access", {"path": CP},
    {"get_managed_system": "managed"})
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": True})
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": False, "path_stat": R("FileNotFoundError", "absent")})
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": False, "path_stat": {"st_mode": 0o607},
     "os_chmod": R("PermissionError", "denied")})
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": False, "path_stat": R("SystemExit", "bye")})
add("drop_other_access", {"path": CP},
    {"get_managed_system": None, "container_or_chmod_skipped": False, "path_stat": {"st_mode": 0o600}})

# ── contextvar managers + getters ──
add("refusing_locked_omissions", {"prefixes": ["model"], "cv": {"refuse": [], "pinned": []}},
    {"cm_body": None, "cm_after": None})
add("refusing_locked_omissions", {"prefixes": ["model", "provider"], "cv": {"refuse": ["existing"], "pinned": []}},
    {"cm_body": None, "cm_after": None})
add("refusing_locked_omissions", {"prefixes": [], "cv": {"refuse": [], "pinned": ["p"]}},
    {"cm_body": None, "cm_after": None})
add("refusing_locked_omissions", {"prefixes": ["model"], "cv": {"refuse": [], "pinned": []}},
    {"cm_body": R("SystemExit", "bye")})
add("refusing_locked_omissions", {"prefixes": ["b"], "cv": {"refuse": ["a"], "pinned": []}},
    {"cm_body": None, "cm_after": None})
add("pinning_keys", {"keys": ["model"], "cv": {"refuse": [], "pinned": []}},
    {"cm_body": None, "cm_after": None})
add("pinning_keys", {"keys": ["model", "provider"], "cv": {"refuse": [], "pinned": ["existing"]}},
    {"cm_body": None, "cm_after": None})
add("pinning_keys", {"keys": [], "cv": {"refuse": ["r"], "pinned": []}},
    {"cm_body": None, "cm_after": None})
add("pinning_keys", {"keys": ["k"], "cv": {"refuse": [], "pinned": []}},
    {"cm_body": R("OSError", "io")})
add("pinning_keys", {"keys": ["b"], "cv": {"refuse": [], "pinned": ["a"]}},
    {"cm_body": None, "cm_after": None})
for func, key in [("refused_locked_omission_prefixes", "refuse"), ("pinned_keys", "pinned")]:
    add(func, {"cv": {"refuse": [], "pinned": []}})
    add(func, {"cv": {"refuse": ["a", "b"], "pinned": []}})
    add(func, {"cv": {"refuse": [], "pinned": ["x"]}})
    add(func, {"cv": {"refuse": ["model"], "pinned": ["provider"]}})
    add(func, {"cv": {}})

# ── FileBackend leaves ──
for home in [HOME, "/h/alice", "/x", "rel", "/h/u/sub"]:
    add("file_config_path", {"home": home})
    add("file_describe", {"home": home})
    add("file_destination", {"home": home})

# file_version_path
add("file_version_path", {"path": CP}, {"path_stat": STAT, "file_signature": SIG})
add("file_version_path", {"path": CP}, {"path_stat": R("FileNotFoundError", "absent")})
add("file_version_path", {"path": CP}, {"path_stat": R("SystemExit", "bye")})
add("file_version_path", {"path": CP}, {"path_stat": STAT, "file_signature": R("OSError", "sig")})
add("file_version_path", {"path": EXPLICIT}, {"path_stat": STAT, "file_signature": [2000, 9, 2, 1]})

# file_read_path
add("file_read_path", {"path": CP}, {"open": CP, "fast_safe_load": DOC})
add("file_read_path", {"path": CP}, {"open": R("FileNotFoundError", "absent")})
add("file_read_path", {"path": CP}, {"open": CP, "fast_safe_load": R("ValueError", "yaml")})
add("file_read_path", {"path": CP}, {"open": R("SystemExit", "bye")})
add("file_read_path", {"path": CP}, {"open": CP, "fast_safe_load": None})
add("file_read_path", {"path": EXPLICIT}, {"open": EXPLICIT, "fast_safe_load": {"k": "v"}})

# file_read_path_readonly
add("file_read_path_readonly", {"path": CP}, {"load_yaml_file_readonly": DOC})
add("file_read_path_readonly", {"path": CP}, {"load_yaml_file_readonly": R("FileNotFoundError", "absent")})
add("file_read_path_readonly", {"path": CP}, {"load_yaml_file_readonly": R("SystemExit", "bye")})
add("file_read_path_readonly", {"path": CP}, {"load_yaml_file_readonly": None})
add("file_read_path_readonly", {"path": EXPLICIT}, {"load_yaml_file_readonly": {"a": 1}})

# file_read_user_doc / file_read_user_doc_readonly / file_version (compose config_path)
add("file_read_user_doc", {"home": HOME}, {"open": CP, "fast_safe_load": DOC})
add("file_read_user_doc", {"home": HOME}, {"open": R("FileNotFoundError", "absent")})
add("file_read_user_doc", {"home": HOME}, {"open": CP, "fast_safe_load": R("ValueError", "yaml")})
add("file_read_user_doc", {"home": HOME}, {"open": R("SystemExit", "bye")})
add("file_read_user_doc", {"home": "/h/alice"}, {"open": "/h/alice/config.yaml", "fast_safe_load": None})
add("file_read_user_doc_readonly", {"home": HOME}, {"load_yaml_file_readonly": DOC})
add("file_read_user_doc_readonly", {"home": HOME}, {"load_yaml_file_readonly": R("FileNotFoundError", "a")})
add("file_read_user_doc_readonly", {"home": HOME}, {"load_yaml_file_readonly": R("SystemExit", "bye")})
add("file_read_user_doc_readonly", {"home": HOME}, {"load_yaml_file_readonly": None})
add("file_read_user_doc_readonly", {"home": "/h/alice"}, {"load_yaml_file_readonly": {"z": 1}})
add("file_version", {"home": HOME}, {"path_stat": STAT, "file_signature": SIG})
add("file_version", {"home": HOME}, {"path_stat": R("FileNotFoundError", "absent")})
add("file_version", {"home": HOME}, {"path_stat": R("SystemExit", "bye")})
add("file_version", {"home": HOME}, {"path_stat": STAT, "file_signature": R("OSError", "sig")})
add("file_version", {"home": "/h/alice"}, {"path_stat": STAT, "file_signature": SIG})

# file_cache_version
add("file_cache_version", {"home": HOME, "version": SIG},
    {"worker_config_snapshot": None, "path_stat": STAT, "file_signature": SIG, "racy_content_digest": "d1"})
add("file_cache_version", {"home": HOME, "version": SIG},
    {"worker_config_snapshot": {"snap": 1}})
add("file_cache_version", {"home": HOME, "version": SIG},
    {"worker_config_snapshot": R("OSError", "io")})
add("file_cache_version", {"home": HOME, "version": SIG},
    {"worker_config_snapshot": None, "path_stat": R("FileNotFoundError", "absent")})
add("file_cache_version", {"home": HOME, "version": SIG},
    {"worker_config_snapshot": None, "path_stat": STAT, "file_signature": SIG, "racy_content_digest": R("OSError", "d")})
add("file_cache_version", {"home": HOME, "version": SIG},
    {"worker_config_snapshot": R("SystemExit", "bye")})
add("file_cache_version", {"home": HOME, "version": [7, 8]},
    {"worker_config_snapshot": {"snap": 1}})

# file_exists
add("file_exists", {"home": HOME}, {"path_exists": True})
add("file_exists", {"home": HOME}, {"path_exists": False})
add("file_exists", {"home": "/h/alice"}, {"path_exists": True})
add("file_exists", {"home": HOME}, {"path_exists": R("OSError", "io")})
add("file_exists", {"home": HOME}, {"path_exists": R("SystemExit", "bye")})

# file_probe_readable
add("file_probe_readable", {"home": HOME}, {"open": CP, "file_read": "x"})
add("file_probe_readable", {"home": HOME}, {"open": R("FileNotFoundError", "absent")})
add("file_probe_readable", {"home": HOME}, {"open": CP, "file_read": R("PermissionError", "denied")})
add("file_probe_readable", {"home": HOME}, {"open": R("SystemExit", "bye")})
add("file_probe_readable", {"home": "/h/alice"}, {"open": "/h/alice/config.yaml", "file_read": "y"})

# file_write_changes (claimed write_path seam)
add("file_write_changes", {"home": HOME, "changes": CH_SET}, {"write_path": None})
add("file_write_changes", {"home": HOME, "changes": CH_DOC}, {"write_path": None})
add("file_write_changes", {"home": HOME, "changes": CH_EMPTY}, {"write_path": locked()})
add("file_write_changes", {"home": HOME, "changes": CH_SET}, {"write_path": R("SystemExit", "bye")})
add("file_write_changes", {"home": "/h/alice", "changes": CH_SET}, {"write_path": R("OSError", "io")})

# trivial constant leaves (>=5 each, varied inputs)
for i, home in enumerate([HOME, "/h/alice", "/x", "rel", "/h/u/sub"]):
    add("file_locked", {"home": home, "dotted": "model%d" % i})
    add("file_boot", {"home": home})
    add("file_doctor_warnings", {"home": home})
    add("file_apply_in_memory", {"home": home, "doc": {"m": i}})
for _ in range(5):
    add("file_supports_file_tooling", {})
    add("file_honors_managed_config", {})
    add("file_protected_env_names", {})
    add("file_deployment_env_names", {})
    add("file_bootstrap_env_names", {})
    add("file_preserve_read_base", {})

# explicit_config_path
add("explicit_config_path", {"path": EXPLICIT, "home": HOME})
add("explicit_config_path", {"path": CP, "home": "/other"})
add("explicit_config_path", {"path": "/a/b.yaml", "home": "/a"})
add("explicit_config_path", {"path": "rel.yaml", "home": "."})
add("explicit_config_path", {"path": "/only.yaml", "home": "/"})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-function coverage report
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    low = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if low:
        print("WARNING funcs with <5 cases: %s" % low)
        raise SystemExit(1)


if __name__ == "__main__":
    main()