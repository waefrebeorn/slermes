#!/usr/bin/env python3
"""Oracle for the config.py pure-gap closure batch (t_port_config_pure.c).

Reads the harness's JSONL on stdin, replays each case's "in" against the LIVE
hermes_cli/config.py defs (AST-extracted — the module import is heavy and
side-effectful), and diffs parsed JSON values. Exception cases compare the
exact message text ("err" field). Summary: ORACLE config_pure: N cases,
M mismatches; exit 1 on any mismatch, fail-closed on 0 cases.

Seeding follows tests/oracle_container_boot.py: module constants (Assign AND
AnnAssign) are exec'd before the defs, and every name the targets read is
asserted bound. The dynamic env_writer_launch_denylist() registry is fed to
the live def through a sys.modules stub carrying the SAME set the C seam
receives as its launch_denylist input. _warn_once_per_provider is a logging-
only no-op stub (it cannot affect returned values).
"""
import ast
import contextlib
import copy
import dataclasses
import io
import json
import os
import sys
import tempfile
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = Path(r"C:\Users\eman5\hermes-agent")
CFG = QUARRY / "hermes_cli" / "config.py"


def _exec_assigns(src, path, ns, only=None):
    """Exec top-level Assign/AnnAssign (both shapes!) in file order."""
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            names = [node.target.id]
        else:
            continue
        if only is not None and not (set(names) & only):
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"), ns)
        except Exception:
            pass


def _exec_defs(src, path, ns, wanted):
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"), ns)


def extract():
    import re as _re
    import platform as _platform
    import threading as _threading
    import logging as _logging
    import difflib as _difflib
    import math as _math
    from typing import Any, Optional, Dict, List, Tuple, Set, Literal
    ns = {
        "re": _re, "os": os, "sys": sys, "json": json, "platform": _platform,
        "threading": _threading, "logging": _logging, "difflib": _difflib,
        "math": _math, "copy": copy, "Path": Path, "dataclasses": dataclasses,
        "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "Any": Any, "Optional": Optional, "Dict": Dict, "List": List,
        "Tuple": Tuple, "Set": Set, "Literal": Literal,
        "logger": _logging.getLogger("config_pure_oracle"),
    }

    # DEFAULT_CONFIG: pure-data leaf chain (auxiliary factory + voice block +
    # the big dict). Exec in dependency order; assert the binding landed.
    aux_src = (QUARRY / "hermes_cli" / "config_defaults_auxiliary.py").read_text(encoding="utf-8")
    _exec_defs(aux_src, "config_defaults_auxiliary.py", ns, {"_aux"})
    voice_path = QUARRY / "hermes_cli" / "config_defaults_voice.py"
    _exec_assigns(voice_path.read_text(encoding="utf-8"), voice_path, ns)
    defaults_path = QUARRY / "hermes_cli" / "config_defaults.py"
    defaults_src = defaults_path.read_text(encoding="utf-8")
    for node in ast.parse(defaults_src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign, ast.FunctionDef)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(defaults_path), "exec"), ns)
            except Exception:
                pass
    if "DEFAULT_CONFIG" not in ns:
        sys.stderr.write("FATAL: DEFAULT_CONFIG did not bind\n")
        sys.exit(2)

    # LAUNCH_ONLY_ENV_KEYS unions into the env writer denylist.
    const_path = QUARRY / "hermes_constants.py"
    _exec_assigns(const_path.read_text(encoding="utf-8"), const_path, ns,
                  only={"LAUNCH_ONLY_ENV_KEYS"})

    # ConfigIssue / _issue (dataclass records the validation cluster appends).
    issues_path = QUARRY / "hermes_cli" / "config_issues.py"
    issues_src = issues_path.read_text(encoding="utf-8")
    for node in ast.parse(issues_src).body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(issues_path), "exec"), ns)

    # Real siblings the targets call (extract, do not stub to constants).
    prov_path = QUARRY / "hermes_cli" / "config_providers.py"
    _exec_defs(prov_path.read_text(encoding="utf-8"), prov_path, ns, {"coerce_provider_id"})
    model_sec_path = QUARRY / "hermes_cli" / "config_model_section.py"
    _exec_defs(model_sec_path.read_text(encoding="utf-8"), model_sec_path, ns,
               {"_normalize_root_model_keys"})
    # logging-only warning hook: cannot affect returned values
    ns["_warn_once_per_provider"] = lambda *a, **k: None

    # The dynamic ACP/left-core launch denylist registry: a stub module whose
    # env_writer_launch_denylist() returns the per-case set (the same set the
    # C port receives through its launch_denylist seam).
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    sys.modules.setdefault("hermes_cli", hc)
    acp = types.ModuleType("hermes_cli.auth_external_process")
    acp.env_writer_launch_denylist = lambda: frozenset()
    sys.modules["hermes_cli.auth_external_process"] = acp
    ns["_acp_stub"] = acp

    # config.py: constants first (Assign AND AnnAssign), then the wanted defs.
    src = CFG.read_text(encoding="utf-8")
    _exec_assigns(src, CFG, ns)
    wanted = {
        "_split_key_path", "_greedy_literal_match", "_phantom_sibling",
        "_locate_nested", "_get_nested", "cfg_get", "_require_fields",
        "_validate_entry_list", "_validate_custom_providers",
        "_validate_fallback_model", "_strip_dotted_keys",
        "_is_non_env_secret_ref", "_env_var_policy_name",
        "validate_env_var_name_for_write", "_canonicalize_config",
        "_normalize_max_turns_config", "_ensure_dict",
        "_commented_sections_for_save", "_is_secret_config_key",
        "_is_env_config_key", "_looks_structured_value",
        "_expected_container_type", "_refuse_container_type_mismatch",
        "_omitted_config_paths", "_exit_invalid", "is_uv_tool_install",
        "is_unsupported_install_method", "format_unsupported_install_warning",
        "_install_method_stamp",
    }
    _exec_defs(src, CFG, ns, wanted)

    required = wanted | {
        "_IS_WINDOWS", "_ENV_VAR_NAME_RE", "_ENV_VAR_NAME_DENY_PREFIXES",
        "_ENV_VAR_NAME_DENYLIST", "_SUPPORTED_INSTALL_METHODS",
        "_ENV_CONFIG_KEYS", "_CP_REQUIRED_FIELDS", "_FB_REQUIRED_FIELDS",
        "_FB_SINGLE_REQUIRED_FIELDS", "_CP_LIST_HINT",
        "_CUSTOM_PROVIDER_LIKE_FIELDS", "_SECRET_CONFIG_KEYS",
        "_SECRET_CONFIG_KEY_SUFFIXES", "_NON_SECRET_KEY_SUFFIXES",
        "_KNOWN_CONTAINER_TYPES", "_SCALAR_AS_ONE_ITEM_LIST_KEYS",
        "_SECURITY_COMMENT", "_FALLBACK_COMMENT", "_MISSING",
        "DEFAULT_CONFIG", "LAUNCH_ONLY_ENV_KEYS", "ConfigIssue", "_issue",
        "coerce_provider_id", "_normalize_root_model_keys", "_aux",
    }
    missing = sorted(n for n in required if n not in ns)
    if missing:
        sys.stderr.write("FATAL: unbound seeds: %r\nseeded: %r\n"
                         % (missing, sorted(ns.keys())))
        sys.exit(2)
    return ns


def main():
    ns = extract()
    tmpdir = tempfile.mkdtemp(prefix="config_pure_oracle_")
    cases = 0
    mism = 0

    def fail(fn, rec, got, want):
        nonlocal mism
        mism += 1
        print(f"MISMATCH {fn} in={json.dumps(rec.get('in'), ensure_ascii=False)} "
              f"C={got!r} PY={want!r}")

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = rec.get("func")
        inp = rec.get("in") or {}
        got = rec.get("out")
        cerr = rec.get("err")
        cases += 1
        try:
            if fn == "config_py_split_key_path":
                want = ns["_split_key_path"](inp["key"])
            elif fn == "config_py_greedy_literal_match":
                r = ns["_greedy_literal_match"](inp["container"], inp["parts"])
                want = [r[0], r[1]] if r is not None else None
            elif fn == "config_py_phantom_sibling":
                want = ns["_phantom_sibling"](inp["container"], inp["part"])
            elif fn == "config_py_locate_nested":
                r = ns["_locate_nested"](inp["config"], inp["parts"])
                if r is None:
                    want = None
                else:
                    parents, container, key = r
                    want = {"parents": [[c, k] for c, k in parents],
                            "container": container, "key": key}
            elif fn == "config_py_validate_entry_list":
                issues = []
                ns["_validate_entry_list"](
                    inp["entries"], inp["label"], issues,
                    [tuple(p) for p in inp["fields"]],
                    non_dict=(inp["severity"], inp["message"], inp["hint"]))
                want = [dataclasses.asdict(i) for i in issues]
            elif fn == "config_py_validate_custom_providers":
                issues = []
                ns["_validate_custom_providers"](inp["cp"], issues)
                want = [dataclasses.asdict(i) for i in issues]
            elif fn == "config_py_validate_fallback_model":
                issues = []
                ns["_validate_fallback_model"](inp["fb"], issues)
                want = [dataclasses.asdict(i) for i in issues]
            elif fn == "config_py_strip_dotted_keys":
                cfg = copy.deepcopy(inp["cfg"])
                c, s = ns["_strip_dotted_keys"](cfg, set(inp["dotted_keys"]))
                want = {"cfg": c, "stripped": sorted(s)}
            elif fn == "config_py_is_non_env_secret_ref":
                want = ns["_is_non_env_secret_ref"](inp["ref"])
            elif fn == "config_py_canonicalize_config":
                try:
                    want = ns["_canonicalize_config"](copy.deepcopy(inp["config"]))
                except Exception as e:
                    if cerr != f"{type(e).__name__}: {e}":
                        fail(fn, rec, cerr, f"{type(e).__name__}: {e}")
                    continue
                if cerr is not None:
                    fail(fn, rec, cerr, None)
                    continue
            elif fn == "config_py_ensure_dict":
                parent = copy.deepcopy(inp["parent"])
                child = ns["_ensure_dict"](parent, inp["key"])
                want = {"parent": parent, "child": child}
            elif fn == "config_py_commented_sections_for_save":
                try:
                    want = ns["_commented_sections_for_save"](inp["normalized"])
                except AttributeError as e:
                    if cerr != f"AttributeError: {e}":
                        fail(fn, rec, cerr, f"AttributeError: {e}")
                    continue
                if cerr is not None:
                    fail(fn, rec, cerr, None)
                    continue
            elif fn == "config_py_is_secret_config_key":
                want = ns["_is_secret_config_key"](inp["key"])
            elif fn == "config_py_looks_structured_value":
                want = ns["_looks_structured_value"](inp["value"])
            elif fn == "config_py_expected_container_type":
                want = ns["_expected_container_type"](inp["key"], inp["user_config"])
            elif fn == "config_py_refuse_container_type_mismatch":
                buf = io.StringIO()
                try:
                    with contextlib.redirect_stderr(buf):
                        want = ns["_refuse_container_type_mismatch"](
                            inp["key"], inp["value"], inp["user_config"], inp["force"])
                except SystemExit as e:
                    printed = buf.getvalue()
                    if printed.endswith("\n"):
                        printed = printed[:-1]
                    if cerr != printed or e.code != 1:
                        fail(fn, rec, {"err": cerr, "code": e.code},
                             {"err": printed, "code": 1})
                    continue
                if cerr is not None:
                    fail(fn, rec, cerr, None)
                    continue
            elif fn == "config_py_is_uv_tool_install":
                want = ns["is_uv_tool_install"]()
            elif fn == "config_py_is_unsupported_install_method":
                want = ns["is_unsupported_install_method"](inp["method"])
            elif fn == "config_py_format_unsupported_install_warning":
                want = ns["format_unsupported_install_warning"](inp["method"])
            elif fn == "config_py_env_var_policy_name":
                want = ns["_env_var_policy_name"](inp["key"], is_windows=inp["is_windows"])
            elif fn == "config_py_validate_env_var_name_for_write":
                ns["_acp_stub"].env_writer_launch_denylist = \
                    lambda: frozenset(inp.get("launch_denylist") or [])
                try:
                    ns["validate_env_var_name_for_write"](inp["key"])
                    want = None
                except ValueError as e:
                    if cerr != str(e):
                        fail(fn, rec, cerr, str(e))
                    continue
                if cerr is not None:
                    fail(fn, rec, cerr, None)
                    continue
            elif fn == "config_py_omitted_config_paths":
                want = ns["_omitted_config_paths"](inp["existing"], inp["proposed"])
            elif fn == "config_py_install_method_stamp":
                text = inp.get("text")
                if text is None:
                    p = Path(tmpdir) / "absent_stamp"
                    if p.exists():
                        p.unlink()
                else:
                    p = Path(tmpdir) / f"stamp_{cases}"
                    p.write_text(text, encoding="utf-8")
                want = ns["_install_method_stamp"](p)
            else:
                fail(fn, rec, got, "<unknown func>")
                continue
            if got != want:
                fail(fn, rec, got, want)
        except Exception as e:
            fail(fn, rec, got, f"ORACLE-ERROR {type(e).__name__}: {e}")

    print(f"ORACLE config_pure: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
