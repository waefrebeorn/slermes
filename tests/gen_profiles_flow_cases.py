#!/usr/bin/env python3
"""Generate tests/fixtures/profiles_flow.jsonl — differential cases for the
unclaimed hermes_cli/profiles.py FLOW ports (src/cli/profiles_flow.c).

Every case is {"func", "in"}; "in.seams" scripts the universal seam (single
value reused, array consumed per call with the last element repeating,
{"__raise__": kind, "msg": ...} scripts an exception, MISSING script is
fail-closed RuntimeError on both sides). The print output seam defaults to null.

Domain (see the port header): ASCII inputs; paths are POSIX strings; integral
floats are kept OUT of str()-computed positions (clean_previous_names items,
error-message names); is_hermes_argv argv is a non-empty string list (one empty
probe models the live IndexError); lstat scripts are {st_mode:int} or an OSError
raise; get_service_manager scripts are a marker string or null; get_config_backend
scripts are {name:str} or null; recorded_served_profiles list results are wrapped
one level deeper ([["default","a"]]) under queue semantics; regex probes stay
single-line.

Run:  python tests/gen_profiles_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "profiles_flow.jsonl")

WD = "/home/u/.local/bin"
REG = 0o100644   # 33188
DIR = 0o040755   # 16877
LNK = 0o120777   # 41471
FIFO = 0o010644  # 4480


def case(func, seams=None, env=None, **inp):
    s = {"print": None}
    s.update(seams or {})
    d = dict(inp)
    d["seams"] = s
    d["env"] = env if env is not None else {}
    return {"func": func, "in": d}


def pure(func, **inp):
    """Pure leaf: no seams (the function performs no seamed call)."""
    d = dict(inp)
    d["seams"] = {}
    d["env"] = {}
    return {"func": func, "in": d}


def bare(func, seams=None, env=None, **inp):
    """Case WITHOUT the default print script (fail-closed probes)."""
    d = dict(inp)
    d["seams"] = dict(seams or {})
    d["env"] = env if env is not None else {}
    return {"func": func, "in": d}


CASES = []

# ── _fold ──
CASES += [
    pure("fold", parts=["AUTH", "X"]),
    pure("fold", parts=[]),
    pure("fold", parts=["a"]),
    pure("fold", parts=["MiXeD", "Case"]),
    pure("fold", parts=[""]),
    pure("fold", parts=[".SSH", "Auth.JSON"]),
]

# ── profile_path_is_private ──
CASES += [
    pure("path_is_private", parts=["auth.json"]),
    pure("path_is_private", parts=["AUTH.JSON"]),
    pure("path_is_private", parts=["auth.json.bak"]),
    pure("path_is_private", parts=["platforms", "pairing"]),
    pure("path_is_private", parts=["platforms"]),
    pure("path_is_private", parts=["config.yaml.bak-20260101t120000z"]),
    pure("path_is_private", parts=["config.yaml.bak-my-note"]),
    pure("path_is_private", parts=["skills"]),
    pure("path_is_private", parts=[]),
    pure("path_is_private", parts=[".env.bak"]),
    pure("path_is_private", parts=["gateway-connections.json.x"]),
    pure("path_is_private", parts=["workspace", "meetings", "node_token.json"]),
]

# ── profile_path_contains_private_store ──
CASES += [
    pure("path_contains_private_store", parts=["platforms"]),
    pure("path_contains_private_store", parts=["platforms", "pairing"]),
    pure("path_contains_private_store", parts=[".config"]),
    pure("path_contains_private_store", parts=["auth.json"]),
    pure("path_contains_private_store", parts=[]),
    pure("path_contains_private_store", parts=["cache"]),
    pure("path_contains_private_store", parts=["workspace", "meetings"]),
]

# ── _suggest_profile_name ──
CASES += [
    pure("suggest_profile_name", name="My Work"),
    pure("suggest_profile_name", name="  --__  "),
    pure("suggest_profile_name", name="!!!"),
    pure("suggest_profile_name", name="A" * 70),
    pure("suggest_profile_name", name="9lives"),
    pure("suggest_profile_name", name="_x"),
    pure("suggest_profile_name", name=""),
    pure("suggest_profile_name", name="ok-name"),
    pure("suggest_profile_name", name="  Hello World  "),
]

# ── error-message builders ──
CASES += [
    pure("invalid_profile_name_error", name="My Work"),
    pure("invalid_profile_name_error", name="k'x"),
    pure("invalid_profile_name_error", name="!!!"),
    pure("invalid_profile_name_error", name=""),
    pure("invalid_profile_name_error", name='a"b'),
    pure("missing_profile_error", canon="coder"),
    pure("missing_profile_error", canon=""),
    pure("missing_profile_error", canon="a-b"),
    pure("missing_profile_error", canon="x_y"),
    pure("missing_profile_error", canon="9lives"),
    pure("unknown_profile_error", canon="coder"),
    pure("unknown_profile_error", canon="x"),
    pure("unknown_profile_error", canon=""),
    pure("unknown_profile_error", canon="a-b"),
    pure("unknown_profile_error", canon="default"),
    pure("profile_exists_error", canon="coder"),
    pure("profile_exists_error", canon="x"),
    pure("profile_exists_error", canon=""),
    pure("profile_exists_error", canon="a-b"),
    pure("profile_exists_error", canon="default"),
]

# ── _clean_previous_names ──
CASES += [
    pure("clean_previous_names", raw=[" a ", "a", None, "", 0, "b"]),
    pure("clean_previous_names", raw="x"),
    pure("clean_previous_names", raw=[5, 5.5, True]),
    pure("clean_previous_names", raw=[]),
    pure("clean_previous_names", raw=[None, None]),
    pure("clean_previous_names", raw=["x", "x", "y"]),
    pure("clean_previous_names", raw=None),
    pure("clean_previous_names", raw=["  ", "\t a \t"]),
]

# ── _is_hermes_argv ──
CASES += [
    pure("is_hermes_argv", argv=["hermes", "chat"]),
    pure("is_hermes_argv", argv=["/usr/bin/python3", "/x/hermes-notes.py"]),
    pure("is_hermes_argv", argv=["/usr/bin/python3.12", "/x/hermes"]),
    pure("is_hermes_argv", argv=["pythonw.exe", "hermes-agent"]),
    pure("is_hermes_argv", argv=["bash", "-c", "tui_gateway"]),
    pure("is_hermes_argv", argv=["foo"]),
    pure("is_hermes_argv", argv=["hermes"]),
    pure("is_hermes_argv", argv=["python3", "hermes-acp"]),
    pure("is_hermes_argv", argv=[]),
]

# ── _argv_profile_selectors ──
CASES += [
    pure("argv_profile_selectors", argv=["-p", "a", "--profile=b", "--profile", "c", "-p"]),
    pure("argv_profile_selectors", argv=[]),
    pure("argv_profile_selectors", argv=["--profile="]),
    pure("argv_profile_selectors", argv=["-p"]),
    pure("argv_profile_selectors", argv=["x", "--profile=y=z"]),
    pure("argv_profile_selectors", argv=["--profile", "a", "-p", "b"]),
]

# ── profile_root_for_env_home ──
CASES += [
    pure("profile_root_for_env_home", env_home="/h/profiles/x", default_root="/def"),
    pure("profile_root_for_env_home", env_home="profiles/x", default_root="/def"),
    pure("profile_root_for_env_home", env_home="/h", default_root="/def"),
    pure("profile_root_for_env_home", env_home="  ", default_root="/def"),
    pure("profile_root_for_env_home", env_home="/profiles/x", default_root="/def"),
    pure("profile_root_for_env_home", env_home="x", default_root="/def"),
    pure("profile_root_for_env_home", env_home="/h/profiles/x/", default_root="/def"),
]

# ── parked_marker_path ──
CASES += [
    pure("parked_marker_path", home="/h"),
    pure("parked_marker_path", home="/h/"),
    pure("parked_marker_path", home="/"),
    pure("parked_marker_path", home="."),
    pure("parked_marker_path", home=""),
    pure("parked_marker_path", home="/a/b"),
]

# ── _export_credential_entries ──
CASES += [
    pure("export_credential_entries", directory="/a/.config", contents=["gh", "x"]),
    pure("export_credential_entries", directory="/a", contents=[".env", "auth.json", "x"]),
    pure("export_credential_entries", directory="/a/.config/gh", contents=["x"]),
    pure("export_credential_entries", directory="rel", contents=[".ssh"]),
    pure("export_credential_entries", directory="/a", contents=[".aws", ".azure"]),
    pure("export_credential_entries", directory="/a", contents=[]),
    pure("export_credential_entries", directory="/a/.config", contents=["gcloud", "gh", "gh"]),
    pure("export_credential_entries", directory="/", contents=["bot-desktop"]),
]

# ── _standalone_truthy ──
CASES += [
    case("standalone_truthy", {"_bool_token": True}, value="true"),
    case("standalone_truthy", {"_bool_token": False}, value="false"),
    case("standalone_truthy", {"_bool_token": 1}, value="1"),
    case("standalone_truthy", {"_bool_token": "true"}, value="yes"),
    case("standalone_truthy", value=True),
    case("standalone_truthy", value=False),
    case("standalone_truthy", value=None),
    case("standalone_truthy", value=1),
    case("standalone_truthy", value=""),
    case("standalone_truthy", value=[]),
    case("standalone_truthy", {"_bool_token": {"__raise__": "SystemExit", "msg": "x"}}, value="true"),
    bare("standalone_truthy", value="true"),
]

# ── _canon_valid ──
CV = {"normalize_profile_name": "coder", "validate_profile_name": None}
CASES += [
    case("canon_valid", CV, name="Coder"),
    case("canon_valid", {"normalize_profile_name": "default", "validate_profile_name": None}, name="Default"),
    case("canon_valid", {"normalize_profile_name": {"__raise__": "ValueError", "msg": "bad"},
                         "validate_profile_name": None}, name="!!!"),
    case("canon_valid", {"normalize_profile_name": "coder",
                         "validate_profile_name": {"__raise__": "ValueError", "msg": "reserved"}}, name="root"),
    case("canon_valid", {"normalize_profile_name": "coder",
                         "validate_profile_name": {"__raise__": "SystemExit", "msg": "x"}}, name="coder"),
    bare("canon_valid", name="coder"),
]

# ── _existing_profile_dir ──
EPD = {"normalize_profile_name": "coder", "validate_profile_name": None,
       "get_profile_dir": "/h/profiles/coder", "is_dir": True}
CASES += [
    case("existing_profile_dir", EPD, name="coder"),
    case("existing_profile_dir", dict(EPD, is_dir=False), name="coder"),
    case("existing_profile_dir", dict(EPD, get_profile_dir={"__raise__": "ValueError", "msg": "x"}), name="coder"),
    case("existing_profile_dir", dict(EPD, normalize_profile_name={"__raise__": "ValueError", "msg": "x"}), name="!!!"),
    case("existing_profile_dir", dict(EPD, is_dir={"__raise__": "SystemExit", "msg": "x"}), name="coder"),
    bare("existing_profile_dir", {"normalize_profile_name": "coder", "validate_profile_name": None}, name="coder"),
]

# ── _wrapper_path ──
CASES += [
    case("wrapper_path", {"_get_wrapper_dir": WD}, alias="coder"),
    case("wrapper_path", {"_get_wrapper_dir": WD}, env={"win32": True}, alias="coder"),
    case("wrapper_path", {"_get_wrapper_dir": {"__raise__": "OSError", "msg": "x"}}, alias="coder"),
    case("wrapper_path", {"_get_wrapper_dir": WD}, env={"win32": True}, alias="a-b"),
    bare("wrapper_path", alias="coder"),
]

# ── _is_our_wrapper ──
HEAD = "#!/bin/sh\nexec hermes -p coder \"$@\"\n"
CASES += [
    case("is_our_wrapper", {"wrapper_read": HEAD}, path=WD + "/coder"),
    case("is_our_wrapper", {"wrapper_read": "#!/bin/sh\necho hi\n"}, path=WD + "/coder"),
    case("is_our_wrapper", {"wrapper_read": {"__raise__": "OSError", "msg": "x"}}, path=WD + "/coder"),
    case("is_our_wrapper", {"wrapper_read": {"__raise__": "UnicodeDecodeError", "msg": "x"}}, path=WD + "/coder"),
    case("is_our_wrapper", {"wrapper_read": {"__raise__": "SystemExit", "msg": "x"}}, path=WD + "/coder"),
    case("is_our_wrapper", {"wrapper_read": ""}, path=WD + "/coder"),
    bare("is_our_wrapper", path=WD + "/coder"),
]

# ── _find_our_wrapper ──
CASES += [
    case("find_our_wrapper", {"_get_wrapper_dir": WD, "wrapper_read": HEAD}, alias="coder"),
    case("find_our_wrapper", {"_get_wrapper_dir": WD, "wrapper_read": "nope"}, alias="coder"),
    case("find_our_wrapper", {"_get_wrapper_dir": WD, "wrapper_read": HEAD}, env={"win32": True}, alias="coder"),
    case("find_our_wrapper", {"_get_wrapper_dir": WD, "wrapper_read": ["nope", HEAD]}, env={"win32": True}, alias="coder"),
    case("find_our_wrapper", {"_get_wrapper_dir": WD, "wrapper_read": ["nope", "nope"]}, env={"win32": True}, alias="coder"),
    case("find_our_wrapper", {"_get_wrapper_dir": WD, "wrapper_read": {"__raise__": "SystemExit", "msg": "x"}}, alias="coder"),
    bare("find_our_wrapper", {"_get_wrapper_dir": WD}, alias="coder"),
]

# ── _non_exportable_entries ──
NE = {"lstat": [{"st_mode": REG}, {"st_mode": DIR}, {"st_mode": LNK}, {"st_mode": FIFO}]}
CASES += [
    case("non_exportable_entries", NE, directory="/h/p",
         contents=["a.txt", "__pycache__", "x.sock", "sub", "link", "fifo"]),
    case("non_exportable_entries", {"lstat": [{"st_mode": REG}]}, directory="/h/p", contents=["a.txt"]),
    case("non_exportable_entries", {"lstat": [{"__raise__": "OSError", "msg": "gone"}]},
         directory="/h/p", contents=["vanish"]),
    case("non_exportable_entries", {"lstat": [{"__raise__": "ValueError", "msg": "x"}]},
         directory="/h/p", contents=["a.txt"]),
    case("non_exportable_entries", {}, directory="/h/p", contents=[]),
    case("non_exportable_entries", {"lstat": [{"st_mode": FIFO}, {"st_mode": REG}]},
         directory="/h/p", contents=["f.pipe", "ok.pyo", "b.tmp"]),
    bare("non_exportable_entries", directory="/h/p", contents=["a.txt"]),
]

# ── _notify_multiplexer ──
CASES += [
    case("notify_multiplexer", {"notify_multiplexer_profiles_changed": None}, canon="coder"),
    case("notify_multiplexer", {"notify_multiplexer_profiles_changed": {"__raise__": "ValueError", "msg": "x"}}, canon="coder"),
    case("notify_multiplexer", {"notify_multiplexer_profiles_changed": {"__raise__": "SystemExit", "msg": "x"}}, canon="coder"),
    bare("notify_multiplexer", canon="coder"),
    case("notify_multiplexer", {"notify_multiplexer_profiles_changed": None}, canon=""),
]

# ── _purge_identity ──
CASES += [
    case("purge_identity", {"purge_profile_identity": True}, canon="coder"),
    case("purge_identity", {"purge_profile_identity": False}, canon="coder"),
    case("purge_identity", {"purge_profile_identity": {"__raise__": "ValueError", "msg": "x"}}, canon="coder"),
    case("purge_identity", {"purge_profile_identity": {"__raise__": "KeyboardInterrupt", "msg": ""}}, canon="coder"),
    bare("purge_identity", canon="coder"),
]

# ── _live_default_multiplexer ──
CASES += [
    case("live_default_multiplexer", {"recorded_served_profiles": [["default", "a"]]}),
    case("live_default_multiplexer", {"recorded_served_profiles": None}),
    case("live_default_multiplexer", {"recorded_served_profiles": [[]]}),
    case("live_default_multiplexer", {"recorded_served_profiles": {"__raise__": "ValueError", "msg": "x"}}),
    case("live_default_multiplexer", {"recorded_served_profiles": {"__raise__": "SystemExit", "msg": "x"}}),
    bare("live_default_multiplexer"),
]

# ── _served_by_running_multiplexer ──
SM = "named_profile_served_by_running_multiplexer"
CASES += [
    case("served_by_running_multiplexer", {SM: True}, profile_name="coder"),
    case("served_by_running_multiplexer", {SM: False}, profile_name="coder"),
    case("served_by_running_multiplexer", {SM: None}, profile_name="coder"),
    case("served_by_running_multiplexer", {SM: {"__raise__": "ValueError", "msg": "x"}}, profile_name="coder"),
    case("served_by_running_multiplexer", {SM: {"__raise__": "SystemExit", "msg": "x"}}, profile_name="coder"),
    bare("served_by_running_multiplexer", profile_name="coder"),
]

# ── _retarget_active_profile ──
RA = {"get_active_profile": "coder", "set_active_profile": None, "print": None}
CASES += [
    case("retarget_active_profile", RA, old="coder", new="default", message="reset"),
    case("retarget_active_profile", dict(RA, get_active_profile="other"), old="coder", new="default", message="reset"),
    case("retarget_active_profile", dict(RA, get_active_profile={"__raise__": "ValueError", "msg": "x"}),
         old="coder", new="default", message="reset"),
    case("retarget_active_profile", dict(RA, set_active_profile={"__raise__": "ValueError", "msg": "x"}),
         old="coder", new="default", message="reset"),
    case("retarget_active_profile", dict(RA, **{"print": {"__raise__": "SystemExit", "msg": "x"}}),
         old="coder", new="default", message="reset"),
    bare("retarget_active_profile", {"set_active_profile": None}, old="coder", new="default", message="reset"),
]

# ── current_profile_name ──
CPN = {"get_hermes_home_override": None, "get_active_profile_name": "coder"}
CASES += [
    case("current_profile_name", CPN, environ={"HERMES_PROFILE_NAME": "envname"}, default="def"),
    case("current_profile_name", CPN, environ={"HERMES_PROFILE_NAME": "  envname  "}, default="def"),
    case("current_profile_name", CPN, environ={}, default="def"),
    case("current_profile_name", CPN, environ={"HERMES_PROFILE": "fallback"}, default="def"),
    case("current_profile_name", dict(CPN, get_hermes_home_override="token"),
         environ={"HERMES_PROFILE_NAME": "envname"}, default="def"),
    case("current_profile_name", dict(CPN, get_active_profile_name={"__raise__": "ValueError", "msg": "x"}),
         environ={}, default="def"),
    case("current_profile_name", dict(CPN, get_active_profile_name={"__raise__": "SystemExit", "msg": "x"}),
         environ={}, default="def"),
    case("current_profile_name", dict(CPN, get_active_profile_name=""), environ={}, default="def"),
    case("current_profile_name", dict(CPN, get_hermes_home_override={"__raise__": "ValueError", "msg": "x"}),
         environ={}, default="def"),
    case("current_profile_name", CPN, environ={}, default=None),
    bare("current_profile_name", {"get_hermes_home_override": None}, environ={}, default="def"),
]

# ── _inside_git_checkout ──
CASES += [
    case("inside_git_checkout", {"resolve": "/h/p", "exists": [True]}, path="/h/p"),
    case("inside_git_checkout", {"resolve": "/h/p", "exists": [False, True]}, path="/h/p"),
    case("inside_git_checkout", {"resolve": "/h/p", "exists": [False, False, False]}, path="/h/p"),
    case("inside_git_checkout", {"resolve": {"__raise__": "OSError", "msg": "x"}}, path="/h/p"),
    case("inside_git_checkout", {"resolve": {"__raise__": "RuntimeError", "msg": "loop"}}, path="/h/p"),
    case("inside_git_checkout", {"resolve": {"__raise__": "ValueError", "msg": "x"}}, path="/h/p"),
    case("inside_git_checkout", {"resolve": "/h/p", "exists": [{"__raise__": "OSError", "msg": "x"}]}, path="/h/p"),
    case("inside_git_checkout", {"resolve": "rel", "exists": [False, True]}, path="rel"),
    bare("inside_git_checkout", path="/h/p"),
]

# ── _s6_runtime_manager ──
S6 = {"detect_service_manager": "s6", "get_service_manager": "mgr", "supports_runtime_registration": True}
CASES += [
    case("s6_runtime_manager", S6),
    case("s6_runtime_manager", dict(S6, supports_runtime_registration=False)),
    case("s6_runtime_manager", dict(S6, detect_service_manager="systemd")),
    case("s6_runtime_manager", dict(S6, detect_service_manager={"__raise__": "ValueError", "msg": "x"})),
    case("s6_runtime_manager", dict(S6, get_service_manager=None)),
    case("s6_runtime_manager", dict(S6, get_service_manager={"__raise__": "SystemExit", "msg": "x"})),
    case("s6_runtime_manager", dict(S6, supports_runtime_registration={"__raise__": "OSError", "msg": "x"})),
    bare("s6_runtime_manager"),
]

# ── _refuse_rename_without_file_tooling ──
RR = {"get_config_backend": {"name": "remote"}, "supports_file_tooling": False}
CASES += [
    case("refuse_rename_without_file_tooling", RR, old_canon="coder"),
    case("refuse_rename_without_file_tooling", dict(RR, supports_file_tooling=True), old_canon="coder"),
    case("refuse_rename_without_file_tooling", dict(RR, get_config_backend=None), old_canon="coder"),
    case("refuse_rename_without_file_tooling", dict(RR, get_config_backend={"name": "k'x"}), old_canon="coder"),
    case("refuse_rename_without_file_tooling", dict(RR, supports_file_tooling={"__raise__": "SystemExit", "msg": "x"}), old_canon="coder"),
    bare("refuse_rename_without_file_tooling", old_canon="coder"),
]

# ── _load_yaml_dict ──
LY = {"is_file": True, "read_text": "description: x", "safe_load": {"description": "x"}}
CASES += [
    case("load_yaml_dict", LY, path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, is_file=False), path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, safe_load=None), path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, safe_load=["a"]), path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, safe_load={}), path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, read_text={"__raise__": "ValueError", "msg": "x"}), path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, safe_load={"__raise__": "SystemExit", "msg": "x"}), path="/h/p/profile.yaml"),
    case("load_yaml_dict", dict(LY, is_file={"__raise__": "OSError", "msg": "x"}), path="/h/p/profile.yaml"),
    bare("load_yaml_dict", {"is_file": True}, path="/h/p/profile.yaml"),
]

# ── _seed_file_if_missing ──
SF = {"exists": False, "write_text": None, "chmod": None}
CASES += [
    case("seed_file_if_missing", SF, path="/h/p/.env", text="header", mode=384),
    case("seed_file_if_missing", dict(SF, exists=True), path="/h/p/.env", text="header", mode=384),
    case("seed_file_if_missing", SF, path="/h/p/.env", text="header", mode=None),
    case("seed_file_if_missing", dict(SF, write_text={"__raise__": "OSError", "msg": "x"}), path="/h/p/.env", text="h", mode=384),
    case("seed_file_if_missing", dict(SF, write_text={"__raise__": "ValueError", "msg": "x"}), path="/h/p/.env", text="h", mode=384),
    case("seed_file_if_missing", dict(SF, chmod={"__raise__": "OSError", "msg": "x"}), path="/h/p/.env", text="h", mode=384),
    case("seed_file_if_missing", dict(SF, exists={"__raise__": "OSError", "msg": "x"}), path="/h/p/.env", text="h", mode=384),
    case("seed_file_if_missing", SF, path="/h/p/.env", text="header", mode=0),
    bare("seed_file_if_missing", path="/h/p/.env", text="h", mode=384),
]

# ── _clone_file ──
CF = {"exists": True, "mkdir": None, "copy2": None, "chmod": None}
CASES += [
    case("clone_file", CF, source_dir="/src", profile_dir="/dst", relpath=".env"),
    case("clone_file", CF, source_dir="/src", profile_dir="/dst", relpath="config.yaml"),
    case("clone_file", dict(CF, exists=False), source_dir="/src", profile_dir="/dst", relpath=".env"),
    case("clone_file", dict(CF, copy2={"__raise__": "OSError", "msg": "x"}), source_dir="/src", profile_dir="/dst", relpath=".env"),
    case("clone_file", dict(CF, mkdir={"__raise__": "OSError", "msg": "x"}), source_dir="/src", profile_dir="/dst", relpath=".env"),
    case("clone_file", dict(CF, chmod={"__raise__": "OSError", "msg": "x"}), source_dir="/src", profile_dir="/dst", relpath=".env"),
    case("clone_file", CF, source_dir="/src", profile_dir="/dst", relpath="memories/MEMORY.md"),
    bare("clone_file", {"exists": True}, source_dir="/src", profile_dir="/dst", relpath=".env"),
]

# ── _resolve_clone_source ──
RS = {"require_file_tooling": None, "get_hermes_home": "/h", "is_dir": True}
RSC = {"require_file_tooling": None, "normalize_profile_name": "coder", "validate_profile_name": None,
       "get_profile_dir": "/h/profiles/coder", "is_dir": True}
CASES += [
    case("resolve_clone_source", RS, clone_from=None),
    case("resolve_clone_source", RSC, clone_from="coder"),
    case("resolve_clone_source", dict(RS, is_dir=False), clone_from=None),
    case("resolve_clone_source", dict(RSC, is_dir=False), clone_from="coder"),
    case("resolve_clone_source", dict(RS, require_file_tooling={"__raise__": "ValueError", "msg": "x"}), clone_from=None),
    case("resolve_clone_source", dict(RS, get_hermes_home={"__raise__": "SystemExit", "msg": "x"}), clone_from=None),
    case("resolve_clone_source", dict(RSC, normalize_profile_name={"__raise__": "ValueError", "msg": "x"}), clone_from="!!!"),
    bare("resolve_clone_source", {"require_file_tooling": None}, clone_from=None),
]

# ── _seed_model_config ──
SMC = {"config_exists": [False, True], "get_hermes_home": "/h",
       "read_user_config_raw": {"model": {"provider": "nous"}},
       "launch_model_seed": {"model": {"provider": "nous"}}, "atomic_config_write": None}
CASES += [
    case("seed_model_config", SMC, profile_dir="/dst"),
    case("seed_model_config", dict(SMC, config_exists=[True]), profile_dir="/dst"),
    case("seed_model_config", dict(SMC, config_exists=[False, False]), profile_dir="/dst"),
    case("seed_model_config", dict(SMC, launch_model_seed={}), profile_dir="/dst"),
    case("seed_model_config", dict(SMC, get_hermes_home={"__raise__": "ValueError", "msg": "x"}), profile_dir="/dst"),
    case("seed_model_config", dict(SMC, read_user_config_raw={"__raise__": "SystemExit", "msg": "x"}), profile_dir="/dst"),
    case("seed_model_config", dict(SMC, config_exists=[{"__raise__": "OSError", "msg": "x"}]), profile_dir="/dst"),
    case("seed_model_config", dict(SMC, atomic_config_write={"__raise__": "ValueError", "msg": "x"}), profile_dir="/dst"),
    bare("seed_model_config", profile_dir="/dst"),
]

# ── _strip_multiplex_flag ──
ST = {"config_exists": True, "read_user_config_raw": {"gateway": {"multiplex_profiles": True, "other": 1}},
      "atomic_config_replace": None}
CASES += [
    case("strip_multiplex_flag", ST, config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, read_user_config_raw={"multiplex_profiles": True}), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, read_user_config_raw={"gateway": {"other": 1}}), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, read_user_config_raw={"gateway": {"multiplex_profiles": True}}), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, config_exists=False), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, read_user_config_raw=None), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, read_user_config_raw={"__raise__": "SystemExit", "msg": "x"}), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, atomic_config_replace={"__raise__": "ValueError", "msg": "x"}), config_path="/dst/config.yaml"),
    case("strip_multiplex_flag", dict(ST, read_user_config_raw={"multiplex_profiles": True, "gateway": "x"}), config_path="/dst/config.yaml"),
    bare("strip_multiplex_flag", config_path="/dst/config.yaml"),
]


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function coverage audit (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    low = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if low:
        print("WARNING funcs with <5 cases: %s" % low)
    else:
        print("all funcs have >=5 cases")


if __name__ == "__main__":
    main()
