#!/usr/bin/env python3
"""Generate tests/fixtures/config_backend_flow2.jsonl for the config_backend_flow2
FLOW oracle (hermes_cli/config_backend.py UNCLAIMED remainder). >=5 cases per ported
function (27 funcs), exercising decision branches, error paths, BaseException
propagation, ctor default/present-null semantics and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None); single
value = constant; scripted null = None; {"__raise__": cls, "msg": ...} = exception.
ASCII / POSIX-lexical domain. Ctor kwargs restricted to declared fields; PRESENT null
passes through verbatim, ABSENT takes the default. The Protocol methods ignore args.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_backend_flow2.jsonl")

CP = "/h/u/config.yaml"


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


CASES = []


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


# ── refuse_redacted_sets (imported collaborator seam) ──
add("refuse_redacted_sets", {"config_path": CP, "sets": {"model": "x"}},
    {"refuse_redacted_config_sets": None})
add("refuse_redacted_sets", {"config_path": CP, "sets": {}},
    {"refuse_redacted_config_sets": None})
add("refuse_redacted_sets", {"config_path": CP, "sets": {"a.b": 1, "c": "s"}},
    {"refuse_redacted_config_sets": None})
add("refuse_redacted_sets", {"config_path": CP, "sets": {"model": "x"}},
    {"refuse_redacted_config_sets": R("ConfigValueError", "redacted")})
add("refuse_redacted_sets", {"config_path": CP, "sets": {"model": "x"}},
    {"refuse_redacted_config_sets": R("ConfigLockedError", "model is locked by the managed level")})
add("refuse_redacted_sets", {"config_path": CP, "sets": {"model": "x"}},
    {"refuse_redacted_config_sets": R("SystemExit", "bye")})
add("refuse_redacted_sets", {"config_path": CP, "sets": {"model": "x"}},
    {"refuse_redacted_config_sets": R("OSError", "io")})
add("refuse_redacted_sets", {"config_path": CP, "sets": {"model": "x"}}, {})  # fail-closed
add("refuse_redacted_sets", {"config_path": CP, "sets": None},
    {"refuse_redacted_config_sets": None})

# ── read_old (keeping_read_base seam; `or {}` + except FileNotFoundError) ──
add("read_old", {"config_path": CP}, {"keeping_read_base": {"model": "x"}})
add("read_old", {"config_path": CP}, {"keeping_read_base": None})
add("read_old", {"config_path": CP}, {"keeping_read_base": {}})
add("read_old", {"config_path": CP}, {"keeping_read_base": R("FileNotFoundError", "absent")})
add("read_old", {"config_path": CP}, {"keeping_read_base": R("PermissionError", "denied")})
add("read_old", {"config_path": CP}, {"keeping_read_base": R("SystemExit", "bye")})
add("read_old", {"config_path": CP}, {})  # fail-closed
add("read_old", {"config_path": CP}, {"keeping_read_base": ""})
add("read_old", {"config_path": CP}, {"keeping_read_base": 0})
add("read_old", {"config_path": CP}, {"keeping_read_base": [1, 2]})
add("read_old", {"config_path": CP}, {"keeping_read_base": "doc"})
add("read_old", {"config_path": CP}, {"keeping_read_base": R("OSError", "io")})

# ── config_write_error_init (message, code default) ──
add("config_write_error_init", {"message": "boom"})
add("config_write_error_init", {"message": "boom", "code": "custom"})
add("config_write_error_init", {"message": "boom", "code": None})
add("config_write_error_init", {"message": ""})
add("config_write_error_init", {"message": "x y z"})
add("config_write_error_init", {"message": "locked", "code": "config_key_locked"})

# ── config_locked_error_init (message or f-string default) ──
add("config_locked_error_init", {"path": "model", "locked_by": "managed"})
add("config_locked_error_init", {"path": "model", "locked_by": "managed", "message": None})
add("config_locked_error_init", {"path": "model", "locked_by": "managed", "message": ""})
add("config_locked_error_init", {"path": "model", "locked_by": "managed", "message": "custom msg"})
add("config_locked_error_init", {"path": "a.b", "locked_by": "remote", "message": None})
add("config_locked_error_init", {"path": "p", "locked_by": "q", "message": "override"})

# ── config_value_error_init (message, code default) ──
add("config_value_error_init", {"message": "bad"})
add("config_value_error_init", {"message": "bad", "code": "custom"})
add("config_value_error_init", {"message": "bad", "code": None})
add("config_value_error_init", {"message": ""})
add("config_value_error_init", {"message": "yaml type"})

# ── explicit_file_init (path normalization) ──
add("explicit_file_init", {"path": "/h/u/other.yaml"})
add("explicit_file_init", {"path": "/h/u\\other.yaml"})
add("explicit_file_init", {"path": "/h/u/other.yaml/"})
add("explicit_file_init", {"path": "rel.yaml"})
add("explicit_file_init", {"path": "/a///"})
add("explicit_file_init", {"path": "/"})

# ── user_layer_init (dataclass defaults; present-null verbatim) ──
add("user_layer_init", {"fields": {"doc": {"model": "x"}, "version": [1, 2, 3]}})
add("user_layer_init", {"fields": {"doc": None, "version": [], "locks": {"a": "b"}, "provenance": "p"}})
add("user_layer_init", {"fields": {"doc": {}, "version": [0], "locks": None, "provenance": None}})
add("user_layer_init", {"fields": {"doc": "s", "version": [1], "locks": {}, "provenance": ""}})
add("user_layer_init", {"fields": {"doc": {"nested": {"k": 1}}, "version": [5, 6], "provenance": "diag"}})
add("user_layer_init", {"fields": {"doc": None, "version": None}})

# ── changes_init (dataclass defaults; present-null verbatim) ──
add("changes_init", {"fields": {}})
add("changes_init", {"fields": {"document": {"a": 1}, "set": {"k": "v"}, "unset": ["x"],
                                "extra_content_on_create": "# hi"}})
add("changes_init", {"fields": {"document": None, "set": None, "unset": None,
                                "extra_content_on_create": None}})
add("changes_init", {"fields": {"set": {"model": "y"}}})
add("changes_init", {"fields": {"unset": ["a", "b"], "extra_content_on_create": "c"}})
add("changes_init", {"fields": {"document": {}, "set": {}, "unset": [], "extra_content_on_create": ""}})

# ── ConfigBackend Protocol surface (19 methods; all return None, ignore args) ──
PROTO_SIG = {
    "read_user_doc": ["home"], "read_user_doc_readonly": ["home"], "version": ["home"],
    "cache_version": ["home", "version"], "exists": ["home"], "probe_readable": ["home"],
    "write_changes": ["home", "changes"], "locked": ["home", "dotted"],
    "supports_file_tooling": [], "honors_managed_config": [], "boot": ["home"],
    "protected_env_names": [], "deployment_env_names": [], "bootstrap_env_names": [],
    "preserve_read_base": [], "doctor_warnings": ["home"], "apply_in_memory": ["home", "doc"],
    "describe": ["home"], "destination": ["home"],
}
HOMES = ["/h/u", "/h/v", "", "rel", "/x/y"]
for name, sig in PROTO_SIG.items():
    for i, home in enumerate(HOMES):
        inp = {}
        if "home" in sig:
            inp["home"] = home
        if "version" in sig:
            inp["version"] = [i, i + 1]
        if "changes" in sig:
            inp["changes"] = {"document": None, "set": {"k": i}, "unset": [],
                              "extra_content_on_create": None}
        if "dotted" in sig:
            inp["dotted"] = "model.%d" % i
        if "doc" in sig:
            inp["doc"] = {"a": i}
        add("proto_" + name, inp)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    # per-func count audit (>=5 required)
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
