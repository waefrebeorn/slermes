#!/usr/bin/env python3
"""Generate tests/fixtures/config_load_save_flow.jsonl for the config load/save
FLOW oracle. Each line is {"func": <name>, "in": {...}}; "in.io" carries the
scripted seam results ({"ret","raise","import_raise","state"} or a queue LIST)
consumed identically by the C harness dispatcher and the oracle's recording
stubs. List-valued seam results are always wrapped in {"ret": [...]} so a queue
is never confused with a single list result. Pure data — no quarry import.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "config_load_save_flow.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def X(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"raise": d}


def IMP(type, message="", **extra):
    d = {"type": type, "message": message}
    d.update(extra)
    return {"import_raise": d}


DC = {
    "model": {"default": "anthropic/claude-sonnet-4", "provider": "auto"},
    "agent": {"max_turns": 90},
    "terminal": {"backend": "local", "docker_env": {}},
    "compression": {"model_thresholds": {}},
    "gateway": {"strict": True},
    "approvals": {"mode": "suggest"},
    "security": {"redact_secrets": True},
}
PCK = ["platforms"]
OPEN = ["providers", "mcp_servers", "discord", "telegram", "plugins", "custom_providers", "sessions"]
EXTRA = ["fallback_model", "platform_toolsets", "smart_model_routing", "reset_triggers",
         "filter_silence_narration", "timeouts"]
LCT = ["terminal.singularity_image"]
VSTATE = {"DEFAULT_CONFIG": DC, "_PLATFORM_CONTAINER_KEYS": PCK,
          "_OPEN_SUBKEY_TOP_LEVEL_KEYS": OPEN, "_EXTRA_KNOWN_ROOT_KEYS": EXTRA,
          "LEFT_CORE_TERMINAL_KEYS": LCT}
SIG = [1, 2, "i"]
MSIG = [1, 2, "i", 0, 0, 0, 0, ""]


def fcr(data, err):
    return {"__failed_read__": {"data": data, "error": err}}


# ── _worker_config_snapshot ──────────────────────────────────────────────
case("worker_config_snapshot", io={"worker_config_snapshot": R({"model": "x"})})
case("worker_config_snapshot", io={"worker_config_snapshot": R(None)})
case("worker_config_snapshot", io={"worker_config_snapshot": IMP("ModuleNotFoundError", "", name="agent")})
case("worker_config_snapshot", io={"worker_config_snapshot": IMP("ModuleNotFoundError", "", name="agent.safe_worker_policy")})
case("worker_config_snapshot", io={"worker_config_snapshot": IMP("ImportError", "boom")})
case("worker_config_snapshot", io={"worker_config_snapshot": R({"agent": {"max_turns": 5}})})

# ── _raw_config_cache_hit ────────────────────────────────────────────────
case("raw_config_cache_hit", path_key="/h/c.yaml", cache_key=SIG,
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"data": 1}]}})
case("raw_config_cache_hit", path_key="/h/c.yaml", cache_key=[999, 2, "i"],
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"data": 1}]}})
case("raw_config_cache_hit", path_key="/absent.yaml", cache_key=SIG,
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"data": 1}]}})
case("raw_config_cache_hit", path_key="/h/c.yaml", cache_key=[],
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"data": 1}]}})
case("raw_config_cache_hit", path_key="/h/c.yaml", cache_key=SIG,
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2]}})
case("raw_config_cache_hit", path_key="/h/c.yaml", cache_key=SIG,
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {}]}})

# ── _raw_config_cache_store ──────────────────────────────────────────────
case("raw_config_cache_store", path_key="/h/c.yaml", cache_key=SIG, data={"a": 1},
     state={"_RAW_CONFIG_CACHE": {}})
case("raw_config_cache_store", path_key="/h/c.yaml", cache_key=[9, 9, "z"], data={"new": 1},
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"old": 1}]}})
case("raw_config_cache_store", path_key="/h/c.yaml", cache_key=[], data={},
     state={"_RAW_CONFIG_CACHE": {}})
case("raw_config_cache_store", path_key="/h/c.yaml", cache_key=SIG, data={"x": {"y": [1, 2]}},
     state={"_RAW_CONFIG_CACHE": {}})
case("raw_config_cache_store", path_key="/h/c.yaml", cache_key=[0, 0, 0, 0, ""], data={"k": "v"},
     state={"_RAW_CONFIG_CACHE": {}})
case("raw_config_cache_store", path_key="/other.yaml", cache_key=SIG, data={"a": 1},
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"keep": 1}]}})

# ── _read_raw_config_impl ────────────────────────────────────────────────
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R({"snap": 1})})
case("read_raw_config_impl", want_deepcopy=False,
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"cached": 1}]}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None), "get_config_path": R("/h/c.yaml"),
         "config_cache_version": R(SIG)})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [R(SIG), R(SIG)],
         "read_config_doc": R({"user": 1})})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [X("RuntimeError", "x"), R("/h/c.yaml")],
         "config_cache_version": [X("FileNotFoundError", "missing")]})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [R(SIG), X("PermissionError", "denied")]})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [R(SIG), R(SIG)],
         "read_config_doc": X("YAMLError", "bad yaml", problem="mapping values"),
         "_warn_config_parse_failure": R(None)})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [R(SIG), R(SIG)],
         "read_config_doc": R([1, 2])})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {"/h/c.yaml": ["old"]}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [R(SIG), R(SIG)],
         "read_config_doc": R(None)})
case("read_raw_config_impl", want_deepcopy=True,
     state={"_RAW_CONFIG_CACHE": {}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [R(SIG), R(SIG)],
         "read_config_doc": R("scalar")})
case("read_raw_config_impl", want_deepcopy=False,
     state={"_RAW_CONFIG_CACHE": {"/h/c.yaml": [1, 2, "i", {"cached": 1}]}, "_CONFIG_PARSE_FAILURES": {}},
     io={"worker_config_snapshot": R(None),
         "get_config_path": [R("/h/c.yaml"), R("/h/c.yaml")],
         "config_cache_version": [X("RuntimeError", "fast"), R(SIG)]})

# ── _load_config_cache_sig ───────────────────────────────────────────────
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": R(SIG), "get_managed_dir": R(None)})
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": X("FileNotFoundError", "missing"), "get_managed_dir": R(None)})
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": X("FileNotFoundError", "missing"),
         "get_managed_dir": R("/managed"), "file_cache_version": R([9, 9, "m"])})
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": R(SIG), "get_managed_dir": R("/managed"),
         "file_cache_version": X("OSError", "io")})
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": R([]), "get_managed_dir": R(None)})
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": X("PermissionError", "denied")})
case("load_config_cache_sig", config_path="/h/c.yaml",
     io={"config_cache_version": R(SIG), "get_managed_dir": X("RuntimeError", "boom")})

# ── _load_config_cache_hit ───────────────────────────────────────────────
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [{"m": 1}, {}]}})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=None,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [{"m": 1}, {}]}})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": [999, 2, "i", 0, 0, 0, 0, "", {"m": 1}, {}]}})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [fcr({}, {"type": "PermissionError", "message": "x"}), {}]}},
     io={"probe_config_readable": X("PermissionError", "still")})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [fcr({}, {"type": "PermissionError", "message": "x"}), {}]}},
     io={"probe_config_readable": R(None)})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [fcr({}, {"type": "ValueError", "message": "bad"}), {}]}})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [{"m": 1}, {"FOO": "bar"}]}},
     io={"current_secret_scope": R(None), "os.environ.get": R("bar")})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [{"m": 1}, {"FOO": "bar"}]}},
     io={"current_secret_scope": R(None), "os.environ.get": R("changed")})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [{"m": 1}, {"A": "1", "B": "2"}]}},
     io={"current_secret_scope": R(None), "os.environ.get": [R("1"), R("wrong")]})
case("load_config_cache_hit", path_key="/h/c.yaml", cache_sig=MSIG,
     state={"_LOAD_CONFIG_CACHE": {"/h/c.yaml": MSIG + [{"m": 1}]}})

# ── _last_known_good_fallback ────────────────────────────────────────────
YERR = {"type": "YAMLError", "message": "bad", "problem": "mapping values"}
LKG_STATE = {"_LAST_EXPANDED_CONFIG_BY_PATH": {}, "_LOAD_CONFIG_CACHE": {},
             "DEFAULT_CONFIG": {"model": {"default": "d"}, "agent": {"max_turns": 90}}}
case("last_known_good_fallback", config_path="/h/c.yaml", path_key="/h/c.yaml", cache_sig=SIG, exc=YERR,
     state={"_LAST_EXPANDED_CONFIG_BY_PATH": {"/h/c.yaml": {"model": "x"}}, "_LOAD_CONFIG_CACHE": {},
            "DEFAULT_CONFIG": DC},
     io={"_warn_config_parse_failure": R(None)})
case("last_known_good_fallback", config_path="/h/c.yaml", path_key="/h/c.yaml", cache_sig=SIG, exc=YERR,
     state=LKG_STATE,
     io={"load_newest_good_backup": R(None), "_warn_config_parse_failure": R(None)})
case("last_known_good_fallback", config_path="/h/c.yaml", path_key="/h/c.yaml", cache_sig=SIG, exc=YERR,
     state=LKG_STATE,
     io={"load_newest_good_backup": R({"model": "m", "moa": {"presets": {"p": {}}}}),
         "apply_user_moa_presets": R({"model": "m", "moa": {"presets": {"p": {"x": 1}}}, "agent": {"max_turns": 90}}),
         "_canonicalize_config": R({"model": {"default": "m"}, "agent": {"max_turns": 90}, "moa": {"presets": {"p": {"x": 1}}}}),
         "load_managed_config": R({}), "_warn_config_parse_failure": R(None)})
case("last_known_good_fallback", config_path="/h/c.yaml", path_key="/h/c.yaml", cache_sig=SIG, exc=YERR,
     state={"_LAST_EXPANDED_CONFIG_BY_PATH": {"/h/c.yaml": {"key": "${FOO}"}}, "_LOAD_CONFIG_CACHE": {},
            "DEFAULT_CONFIG": DC},
     io={"_warn_config_parse_failure": R(None), "current_secret_scope": R(None), "os.environ.get": R("val")})
case("last_known_good_fallback", config_path="/h/c.yaml", path_key="/h/c.yaml", cache_sig=None, exc=YERR,
     state={"_LAST_EXPANDED_CONFIG_BY_PATH": {"/h/c.yaml": {"model": "x"}}, "_LOAD_CONFIG_CACHE": {},
            "DEFAULT_CONFIG": DC},
     io={"_warn_config_parse_failure": R(None)})
case("last_known_good_fallback", config_path="/h/c.yaml", path_key="/h/c.yaml", cache_sig=SIG, exc=YERR,
     state=LKG_STATE,
     io={"load_newest_good_backup": R({"a": 1}),
         "_canonicalize_config": R({"a": 1}),
         "load_managed_config": R({"b": "${env:B}"}),
         "_normalize_root_model_keys": R({"b": "${env:B}"}),
         "current_secret_scope": R(None), "os.environ.get": R("bv"),
         "_warn_config_parse_failure": R(None)})

# ── _merge_managed_overlay ───────────────────────────────────────────────
case("merge_managed_overlay", expanded={"a": 1}, io={"load_managed_config": R({})})
case("merge_managed_overlay", expanded={"a": 1}, io={"load_managed_config": R(None)})
case("merge_managed_overlay", expanded={"a": 1},
     io={"load_managed_config": R({"model": "m"}), "_normalize_root_model_keys": R({"model": "m"})})
case("merge_managed_overlay", expanded={"a": 1},
     io={"load_managed_config": R({"b": "${env:B}"}), "_normalize_root_model_keys": R({"b": "${env:B}"}),
         "current_secret_scope": R(None), "os.environ.get": R("v")})
case("merge_managed_overlay", expanded={"a": 1},
     io={"load_managed_config": R({"model": {"default": "x"}, "b": 2}),
         "_normalize_root_model_keys": R({"model": {"default": "x"}, "b": 2})})
case("merge_managed_overlay", expanded={"a": 1}, io={"load_managed_config": X("PermissionError", "denied")})

# ── _write_config_state ──────────────────────────────────────────────────
case("write_config_state", config_path="/h/c.yaml", data={"a": 1}, allow_omissions=True,
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": R(None)})
case("write_config_state", config_path="/h/c.yaml", data={"a": 1}, allow_omissions=False,
     io={"refuse_redacted_config_credentials": R(None),
         "require_readable_config_before_write": R({"a": 1, "b": 2})})
case("write_config_state", config_path="/h/c.yaml", data={"a": 1, "c": 3}, allow_omissions=False,
     io={"refuse_redacted_config_credentials": R(None),
         "require_readable_config_before_write": R({"a": 1}), "write_config_document": R(None)})
case("write_config_state", config_path="/h/c.yaml",
     data=fcr({}, {"type": "PermissionError", "message": "denied"}), allow_omissions=True, io={})
case("write_config_state", config_path="/h/c.yaml",
     data=fcr({}, {"type": "YAMLError", "message": "bad", "problem": "mapping values", "problem_mark_line": 11}),
     allow_omissions=True, io={"display_hermes_home": R("/home/u/.hermes")})
case("write_config_state", config_path="/h/c.yaml",
     data=fcr({}, {"type": "SystemExit", "message": "1", "code": 1}), allow_omissions=True, io={})
case("write_config_state", config_path="/h/c.yaml", data={"a": 1}, allow_omissions=True,
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": X("PermissionError", "denied")})
case("write_config_state", config_path="/h/c.yaml", data={"a": 1}, allow_omissions=False,
     io={"refuse_redacted_config_credentials": R(None),
         "require_readable_config_before_write": X("PermissionError", "denied")})
_big = {f"a{i}": i for i in range(14)}
case("write_config_state", config_path="/h/c.yaml", data={}, allow_omissions=False,
     io={"refuse_redacted_config_credentials": R(None), "require_readable_config_before_write": R(_big)})

# ── atomic_config_replace ────────────────────────────────────────────────
case("atomic_config_replace", config_path="/h/c.yaml", data={"a": 1},
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": R(None)})
case("atomic_config_replace", config_path="/h/c.yaml", data={"a": 1},
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": R(None)})
case("atomic_config_replace", config_path="/h/c.yaml",
     data=fcr({}, {"type": "PermissionError", "message": "denied"}), io={})
case("atomic_config_replace", config_path="/h/c.yaml", data={"a": 1},
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": X("OSError", "io")})
case("atomic_config_replace", config_path="/h/c.yaml", data={"a": 1}, extra_content_on_create="# comment",
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": R(None)})
case("atomic_config_replace", config_path="/h/c.yaml", data={},
     io={"refuse_redacted_config_credentials": R(None), "write_config_document": R(None)})

# ── _strip_managed_keys_for_save ─────────────────────────────────────────
case("strip_managed_keys_for_save", config={"a": 1}, io={"managed_config_keys": R([])})
case("strip_managed_keys_for_save", config={"a": 1, "b": 2}, io={"managed_config_keys": R(["a"])})
case("strip_managed_keys_for_save", config={"x": {"y": 1, "z": 2}, "b": 3},
     io={"managed_config_keys": R(["x.y"])})
case("strip_managed_keys_for_save", config={"a": 1, "b": 2, "c": 3},
     io={"managed_config_keys": R(["a", "b"])})
case("strip_managed_keys_for_save", config={"a": 1}, io={"managed_config_keys": R(["missing"])})
case("strip_managed_keys_for_save", config={"a": 1}, io={"managed_config_keys": R(None)})
case("strip_managed_keys_for_save", config={"a": 1, "b": 2}, io={"managed_config_keys": R(["b", "a"])})

# ── seed_config_file ─────────────────────────────────────────────────────
case("seed_config_file", config_path="/h/config.yaml", template="/t/cli-config.yaml.example",
     state={"DEFAULT_CONFIG": {"model": "d"}},
     io={"mkdir_parents": R(None), "path_exists": R(True), "copy2": R(None), "_secure_file": R(None)})
case("seed_config_file", config_path="/h/config.yaml", template=None,
     state={"DEFAULT_CONFIG": {"model": "d"}},
     io={"get_project_root": R("/proj"), "mkdir_parents": R(None), "path_exists": R(False), "save_config": R(None)})
case("seed_config_file", config_path="/h/config.yaml", template=None,
     state={"DEFAULT_CONFIG": {"model": "d"}},
     io={"get_project_root": R("/proj"), "mkdir_parents": R(None), "path_exists": R(True),
         "copy2": R(None), "_secure_file": R(None)})
case("seed_config_file", config_path="/h/config.yaml", template="/t/example.yaml",
     state={"DEFAULT_CONFIG": {"model": "d"}}, io={"mkdir_parents": X("PermissionError", "denied")})
case("seed_config_file", config_path="/h/config.yaml", template="/t/example.yaml",
     state={"DEFAULT_CONFIG": {"model": "d"}},
     io={"mkdir_parents": R(None), "path_exists": R(True), "copy2": X("OSError", "io")})
case("seed_config_file", config_path="/h/config.yaml", template=None,
     state={"DEFAULT_CONFIG": {"model": "d"}},
     io={"get_project_root": R("/proj"), "mkdir_parents": R(None), "path_exists": R(False),
         "save_config": X("RuntimeError", "boom")})

# ── _env_ref_lookup ──────────────────────────────────────────────────────
case("env_ref_lookup", name="FOO", io={"current_secret_scope": R(None), "os.environ.get": R("bar")})
case("env_ref_lookup", name="FOO", io={"current_secret_scope": R({}), "get_secret": R("scoped")})
case("env_ref_lookup", name="FOO",
     io={"current_secret_scope": IMP("ModuleNotFoundError", "", name="agent.secret_scope"),
         "os.environ.get": R("envval")})
case("env_ref_lookup", name="MISSING", io={"current_secret_scope": R(None), "os.environ.get": R(None)})
case("env_ref_lookup", name="FOO", io={"current_secret_scope": R("scopeobj"), "get_secret": R(None)})
case("env_ref_lookup", name="FOO",
     io={"current_secret_scope": IMP("ImportError", "x"), "os.environ.get": R("v")})

# ── _expand_env_vars ─────────────────────────────────────────────────────
case("expand_env_vars", obj="plain", io={})
case("expand_env_vars", obj="${FOO}", io={"current_secret_scope": R(None), "os.environ.get": R("bar")})
case("expand_env_vars", obj="${env:FOO}", io={"current_secret_scope": R(None), "os.environ.get": R(None)})
case("expand_env_vars", obj="${vault:SECRET}", io={})
case("expand_env_vars", obj={"a": "${X}", "b": [1, "${env:Y}"], "c": True},
     io={"current_secret_scope": R(None), "os.environ.get": [R("xv"), R("yv")]})
case("expand_env_vars", obj="${env:}", io={})
case("expand_env_vars", obj="${ FOO }", io={"current_secret_scope": R(None), "os.environ.get": R("v")})
case("expand_env_vars", obj=42, io={})
case("expand_env_vars", obj="pre${A}post${B}",
     io={"current_secret_scope": R(None), "os.environ.get": [R("1"), R("2")]})
case("expand_env_vars", obj="${}", io={})

# ── _validate_config_key ─────────────────────────────────────────────────
case("validate_config_key", key="agent.max_turns", state=VSTATE, io={})
case("validate_config_key", key="", state=VSTATE, io={})
case("validate_config_key", key="modle.default", state=VSTATE,
     io={"_suggest_closest_key": R("model"), "is_runtime_read_key": R(False)})
case("validate_config_key", key="some.runtime.key", state=VSTATE,
     io={"_suggest_closest_key": R(None), "is_runtime_read_key": R(True)})
case("validate_config_key", key="gateway.discord.gateway_restart_notification", state=VSTATE,
     io={"is_runtime_read_key": R(False)})
case("validate_config_key", key="platforms.x.y", state=VSTATE, io={})

# ── _validate_schema_key ─────────────────────────────────────────────────
case("validate_schema_key", key="", state=VSTATE, io={})
case("validate_schema_key", key="_internal", state=VSTATE, io={})
case("validate_schema_key", key="platforms.discord.token", state=VSTATE, io={})
case("validate_schema_key", key="terminal.singularity_image", state=VSTATE, io={})
case("validate_schema_key", key="modle.default", state=VSTATE, io={"_suggest_closest_key": R("model")})
case("validate_schema_key", key="xyz", state=VSTATE, io={"_suggest_closest_key": R(None)})
case("validate_schema_key", key="providers.myprov.models", state=VSTATE, io={})
case("validate_schema_key", key="agent.max_turns", state=VSTATE, io={})
case("validate_schema_key", key="agent.max_turn", state=VSTATE, io={"_suggest_closest_key": R("max_turns")})
case("validate_schema_key", key="gateway.discord.gateway_restart_notification", state=VSTATE, io={})
case("validate_schema_key", key="terminal.backend.extra", state=VSTATE, io={})
case("validate_schema_key", key="compression.model_thresholds.grok", state=VSTATE, io={})
case("validate_schema_key", key="agent.gateway.strict", state=VSTATE, io={})
case("validate_schema_key", key="security.redact_secret", state=VSTATE, io={"_suggest_closest_key": R("redact_secrets")})

# ── _is_wrong_prefix_suggestion ──────────────────────────────────────────
case("is_wrong_prefix_suggestion", key="gateway.discord.gateway_restart_notification",
     suggestion="discord.gateway_restart_notification", state=VSTATE, io={})
case("is_wrong_prefix_suggestion", key="agent.max_turns", suggestion="max_turns", state=VSTATE,
     io={"_suggest_closest_key": R(None), "is_runtime_read_key": R(False)})
case("is_wrong_prefix_suggestion", key="a.b.c", suggestion="x.y", state=VSTATE, io={})
case("is_wrong_prefix_suggestion", key="a.b", suggestion="a.b.c", state=VSTATE, io={})
case("is_wrong_prefix_suggestion", key="gateway.discord.token", suggestion="discord.token", state=VSTATE, io={})
case("is_wrong_prefix_suggestion", key="a.b", suggestion=None, state=VSTATE, io={})
case("is_wrong_prefix_suggestion", key="a.b", suggestion="", state=VSTATE, io={})

# ── _coerce_config_set_value ─────────────────────────────────────────────
case("coerce_config_set_value", key="model.default", value="anthropic/claude-sonnet-4",
     io={"_default_value_for_key": R("anthropic/claude-sonnet-4")})
case("coerce_config_set_value", key="gateway.strict", value="true", io={"_default_value_for_key": R(True)})
case("coerce_config_set_value", key="gateway.strict", value='"true"',
     io={"_default_value_for_key": R(True), "_quoted_bool": R(True)})
case("coerce_config_set_value", key="agent.max_turns", value="42",
     io={"_default_value_for_key": R(90), "_coerce_int": R(42)})
case("coerce_config_set_value", key="some.float", value="1.5",
     io={"_default_value_for_key": R(None), "_coerce_int": R(None), "_coerce_float": R(1.5)})
case("coerce_config_set_value", key="model", value="[1, 2]",
     io={"_default_value_for_key": R("anthropic/claude-sonnet-4"), "_coerce_int": R(None),
         "_coerce_float": R(None), "yaml.safe_load": R([1, 2])})
case("coerce_config_set_value", key="custom_providers", value="not structured",
     io={"_default_value_for_key": R(None), "_coerce_int": R(None), "_coerce_float": R(None)})
case("coerce_config_set_value", key="hooks", value="[broken",
     io={"_default_value_for_key": R(None), "_coerce_int": R(None), "_coerce_float": R(None),
         "yaml.safe_load": X("YAMLError", "yaml error", problem="expected node")})
case("coerce_config_set_value", key="x", value="null", io={"_default_value_for_key": R(None)})
case("coerce_config_set_value", key="x", value="OFF", io={"_default_value_for_key": R(None)})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for k in sorted(counts):
        flag = "" if counts[k] >= 6 else "  (<6!)"
        print(f"  {k}: {counts[k]}{flag}")
    assert all(v >= 6 for v in counts.values()), "every function needs >=6 cases"
    assert len(counts) == 18, f"expected 18 functions, got {len(counts)}"


if __name__ == "__main__":
    main()
