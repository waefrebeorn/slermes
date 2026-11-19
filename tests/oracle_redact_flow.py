#!/usr/bin/env python3
"""Oracle for the agent/redact.py FLOW + leaf port (src/agent/redact_flow.c).

Drives the LIVE agent/redact.py redaction decision chains with SCRIPTED I/O:
every regex-dependent / foreign sub-call is a recording stub fed the same scripts
the C harness feeds its seams. Compares the DECISION SEQUENCE (ordered seam names
in "trace"), the return ("ret") and the mutated policy registry ("state").

The pure policy leaves (_key_name_words, _is_qualified_key_name,
_is_python_repr_secret_key, is_secret_field_name, _decode_url_param_name,
_canonical_url_param_name, note_env_policy, note_config_policy) stay REAL so their
nested logic is exercised exactly as the C port hand-ports it. In the FLOW
functions those same leaves are seamed (scripted identically on both sides); they
are verified for real by their own direct cases, so scripting hides no divergence.

Live defs are AST-extracted; only the targeted module constants are seeded
(fail-closed if a name moves). ASCII subset: casefold/isupper/strip and
unquote_plus fixtures are ASCII. Catch BaseException; fail-closed (0 cases => 1).
Summary: 'ORACLE redact_flow: N cases, M mismatches'.
"""
import ast
import copy
import json
import os
import sys
import types
from pathlib import Path
from urllib.parse import unquote_plus

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "redact.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "redact.py")
FS_SRC = str(QUARRY / "agent" / "file_safety.py")

# func (harness name) -> live def name
PY = {
    "should_redact_assignment": "_should_redact_assignment",
    "json_affixed_key_secret": "_json_affixed_key_secret",
    "is_sensitive_url_param": "_is_sensitive_url_param",
    "redact_log_urls": "redact_log_urls",
    "redact_for_egress": "redact_for_egress",
    "redact_code_output": "redact_code_output",
    "redact_derived_text": "redact_derived_text",
    "redact_enabled": "_redact_enabled",
    "command_reads_secret_file": "_command_reads_secret_file",
    "is_secret_file_arg": "_is_secret_file_arg",
    "note_env_policy": "note_env_policy",
    "note_config_policy": "note_config_policy",
    "key_name_words": "_key_name_words",
    "is_qualified_key_name": "_is_qualified_key_name",
    "is_python_repr_secret_key": "_is_python_repr_secret_key",
    "is_secret_field_name": "is_secret_field_name",
    "decode_url_param_name": "_decode_url_param_name",
    "canonical_url_param_name": "_canonical_url_param_name",
}

# Extra real defs the flows/leaves call internally (not themselves PoP ports).
EXTRA_DEFS = {"_assignment_sub", "_has_word_bounded_keyword", "_is_word_start", "_is_word_end"}

# Module constants seeded from redact.py (fail-closed).
WANTED_CONSTS = {
    "_CONFIG_LITERAL_VALUES", "_SENSITIVE_QUERY_PARAMS", "_PYTHON_REPR_SECRET_KEYS",
    "_PYTHON_REPR_ENV_SUFFIXES", "_PYTHON_REPR_CREDENTIAL_SUFFIXES",
    "_KEY_SECRET_QUALIFIERS", "_SHELL_RC_BASENAMES", "_FILE_READ_COMMANDS",
    "_PATTERN_FIRST_COMMANDS", "_HERMES_HOME_PREFIXES", "_HOME_PREFIXES",
    "REDACTION_UNAVAILABLE",
}
# Constants imported from agent/file_safety.py.
FS_CONSTS = {"_BLOCKED_PROJECT_ENV_BASENAMES", "HOME_CREDENTIAL_FILES"}

# Regex globals replaced by recording doubles in the FLOW functions.
# name -> (seam for match/fullmatch/search, tag for _has_word_bounded_keyword, sub seam)
RX_DOUBLES = {
    "_ENV_LOOKUP_VALUE_RE": ("env_lookup_match", None, None),
    "_PATH_OR_VAR_VALUE_RE": ("path_or_var_match", None, None),
    "_OPAQUE_PATH_SEGMENT_RE": ("opaque_path_segment_fullmatch", None, None),
    "_CONFIG_PLACEHOLDER_VALUE_RE": ("config_placeholder_fullmatch", None, None),
    "_JSON_KEY_NAMES_FULL_RE": ("json_key_names_full_fullmatch", None, None),
    "_DSN_PW_PLACEHOLDER_RE": ("dsn_pw_placeholder_fullmatch", None, None),
    "_JSON_VALUE_SUFFIX_RE": (None, None, "json_value_suffix_sub"),
    "_BEARER_RESIDUE_RE": (None, None, "bearer_residue_sub"),
    "_ENV_DUMP_LINE_RE": (None, None, "env_dump_line_sub"),
    "_PASSWORD_KEY_RE": (None, "password", None),
    "_STRONG_KEY_KEYWORD_RE": (None, "strong", None),
    "_YAML_CFG_WORD_RE": (None, "yaml", None),
}

# Module-function siblings seamed per flow func.
FLOW_STUBS = {
    "should_redact_assignment": {
        "_key_has_secret_keyword", "_has_password_keyword", "_is_glued_credential_env",
        "_is_qualified_key_name", "_looks_like_opaque_credential", "_has_word_bounded_keyword",
    },
    "json_affixed_key_secret": {
        "_has_password_keyword", "_has_word_bounded_keyword", "_should_redact_assignment",
        "_is_python_repr_secret_key",
    },
    "is_sensitive_url_param": {
        "_canonical_url_param_name", "_decode_url_param_name", "_is_python_repr_secret_key",
    },
    "redact_log_urls": {"_redact_enabled", "_redact_strict_url_credentials"},
    "redact_for_egress": {"redact_sensitive_text"},
    "redact_code_output": {"redact_sensitive_text", "_redact_enabled"},
    "redact_derived_text": {"redact_sensitive_text"},
    "command_reads_secret_file": {"_command_segments", "_is_secret_file_arg"},
    "is_secret_file_arg": {"_is_under_hermes_home"},
}
FLOW_RX = {
    "should_redact_assignment": {
        "_ENV_LOOKUP_VALUE_RE", "_PATH_OR_VAR_VALUE_RE", "_OPAQUE_PATH_SEGMENT_RE",
        "_CONFIG_PLACEHOLDER_VALUE_RE", "_PASSWORD_KEY_RE", "_STRONG_KEY_KEYWORD_RE",
    },
    "json_affixed_key_secret": {
        "_JSON_KEY_NAMES_FULL_RE", "_DSN_PW_PLACEHOLDER_RE", "_JSON_VALUE_SUFFIX_RE",
        "_YAML_CFG_WORD_RE",
    },
    "is_sensitive_url_param": {"_JSON_KEY_NAMES_FULL_RE", "_JSON_VALUE_SUFFIX_RE"},
    "redact_for_egress": {"_BEARER_RESIDUE_RE"},
    "redact_code_output": {"_ENV_DUMP_LINE_RE"},
    "command_reads_secret_file": {"_PS_COLON_PARAM_RE"},
}
# Real leaf defs restored before every case (a prior flow case may have stubbed them).


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    if t == "SystemExit":
        return SystemExit(msg)
    if t == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if t == "GeneratorExit":
        return GeneratorExit(msg)
    if t == "CancelledError":
        import asyncio
        return asyncio.CancelledError(msg)
    cls = {"ValueError": ValueError, "RuntimeError": RuntimeError, "TypeError": TypeError,
           "KeyError": KeyError, "Exception": Exception}.get(t)
    if cls is None:
        cls = type(t, (Exception,), {})
    return cls(msg)


def extract():
    import re as _re
    import logging as _logging
    import threading as _threading
    import shlex as _shlex
    ns = {
        "__name__": "agent.redact",
        "re": _re, "os": os, "json": json, "sys": sys, "copy": copy,
        "logging": _logging, "logger": _logging.getLogger("agent.redact"),
        "threading": _threading, "shlex": _shlex, "unquote_plus": unquote_plus,
        "Any": object, "Optional": object, "Callable": object,
    }
    sys.modules.setdefault("agent.redact", types.ModuleType("agent.redact"))
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)

    def seg(node, path):
        return compile(ast.Module(body=[node], type_ignores=[]), path, "exec")

    # targeted constants from redact.py
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name) and tgt.id in WANTED_CONSTS:
                exec(seg(node, SRC), ns)
                found.add(tgt.id)
    missing = WANTED_CONSTS - found
    if missing:
        print("FATAL redact_flow: constants missing from redact.py:", sorted(missing))
        sys.exit(2)
    # imported constants from file_safety.py
    fsrc = open(FS_SRC, encoding="utf-8").read()
    ftree = ast.parse(fsrc)
    ffound = set()
    for node in ftree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name) and tgt.id in FS_CONSTS:
                exec(seg(node, FS_SRC), ns)
                ffound.add(tgt.id)
    if FS_CONSTS - ffound:
        print("FATAL redact_flow: constants missing from file_safety.py:", sorted(FS_CONSTS - ffound))
        sys.exit(2)
    ns["_ENV_FILE_BASENAMES"] = ns["_BLOCKED_PROJECT_ENV_BASENAMES"]
    # policy registry defaults (overridden per case from state)
    ns["_REDACT_ENABLED"] = True
    ns["_ENV_POLICY_BY_HOME"] = {}
    ns["_CONFIG_POLICY_BY_HOME"] = {}
    # function defs (targets + extra real helpers)
    want_defs = set(PY.values()) | EXTRA_DEFS
    got_defs = set()
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in want_defs:
            exec(seg(node, SRC), ns)
            got_defs.add(node.name)
    if want_defs - got_defs:
        print("FATAL redact_flow: defs missing:", sorted(want_defs - got_defs))
        sys.exit(2)
    return ns


NS = extract()
# Restore every target def (plus real internal helpers) before each case: a prior
# flow case may have stubbed a name that is itself a target (e.g. _redact_enabled,
# _is_secret_file_arg, _should_redact_assignment are seamed siblings elsewhere).
REAL = {nm: NS[nm] for nm in (set(PY.values()) | {"_has_word_bounded_keyword", "_assignment_sub"})}
CURRENT = {"rec": None}


class RxDouble:
    def __init__(self, seam=None, tag=None, sub_seam=None):
        self.seam = seam
        self.tag = tag
        self.sub_seam = sub_seam

    def _bool_call(self, s):
        return CURRENT["rec"](self.seam)(s)

    def match(self, s):
        return self._bool_call(s)

    def fullmatch(self, s):
        return self._bool_call(s)

    def search(self, s):
        return self._bool_call(s)

    def sub(self, repl, s):
        return CURRENT["rec"](self.sub_seam)(s)


class _MatchDouble:
    def __init__(self, g1):
        self._g1 = g1

    def group(self, n):
        return self._g1


class PsColonRx:
    def fullmatch(self, s):
        ret = CURRENT["rec"]("ps_colon_param_fullmatch")(s)
        if ret is None:
            return None
        return _MatchDouble(ret)


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state", {}) or {})

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return None
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return None
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def recorder(self, name):
        ctx = self

        def call(*args, **kwargs):
            ctx.trace.append(name)
            result = ctx.script_next(name)
            if isinstance(result, dict):
                raise_spec = result.get("raise")
                if isinstance(raise_spec, dict):
                    raise make_exc(raise_spec)
                return result.get("ret")
            return result
        return call


def _hwb_stub(key, regex):
    seam = "has_word_bounded_keyword_" + regex.tag
    return CURRENT["rec"](seam)(key)


def seed_case(ctx, func):
    CURRENT["rec"] = ctx.recorder
    # restore real leaves a previous flow case may have stubbed
    for nm, obj in REAL.items():
        NS[nm] = obj
    # state globals (same dict objects so note_* mutations reflect)
    NS["_REDACT_ENABLED"] = ctx.state.get("_REDACT_ENABLED", True)
    NS["_ENV_POLICY_BY_HOME"] = ctx.state.get("_ENV_POLICY_BY_HOME", {})
    NS["_CONFIG_POLICY_BY_HOME"] = ctx.state.get("_CONFIG_POLICY_BY_HOME", {})
    # install flow seams (recorder installed under the SEAM name = live name minus
    # the leading underscore, matching the C call sites and fixture io keys)
    for nm in FLOW_STUBS.get(func, set()):
        if nm == "_has_word_bounded_keyword":
            NS[nm] = _hwb_stub
        else:
            seam = nm[1:] if nm.startswith("_") else nm
            NS[nm] = ctx.recorder(seam)
    for rx in FLOW_RX.get(func, set()):
        if rx == "_PS_COLON_PARAM_RE":
            NS[rx] = PsColonRx()
        else:
            seam, tag, sub_seam = RX_DOUBLES[rx]
            NS[rx] = RxDouble(seam, tag, sub_seam)
    # _redact_enabled in-function imports
    if func == "redact_enabled":
        agent = sys.modules.setdefault("agent", types.ModuleType("agent"))
        agent.__path__ = []
        hc = types.ModuleType("hermes_constants")
        hc.get_hermes_home_override = ctx.recorder("get_hermes_home_override")
        hc.hermes_home_key = ctx.recorder("hermes_home_key")
        sys.modules["hermes_constants"] = hc
        ss = types.ModuleType("agent.secret_scope")
        ss.current_secret_scope = ctx.recorder("current_secret_scope")
        sys.modules["agent.secret_scope"] = ss


def scalar_state(ctx, inp):
    return {k: ctx.state[k] for k in (inp.get("state") or {}) if k in ctx.state}


def run_case(func, inp):
    ctx = CaseCtx(inp)
    seed_case(ctx, func)
    out = {"ret": None, "trace": ctx.trace}
    py = PY[func]
    try:
        if func == "should_redact_assignment":
            out["ret"] = NS[py](inp.get("key", ""), inp.get("value", ""),
                                check_keyword=inp.get("check_keyword", True))
        elif func == "json_affixed_key_secret":
            out["ret"] = NS[py](inp.get("key", ""), inp.get("value", ""))
        elif func == "is_sensitive_url_param":
            out["ret"] = NS[py](inp.get("name", ""))
        elif func == "redact_log_urls":
            out["ret"] = NS[py](inp.get("text"))
        elif func == "redact_for_egress":
            out["ret"] = NS[py](inp.get("text"), urls=inp.get("urls", False),
                                keep_phone_numbers=inp.get("keep_phone_numbers", False))
        elif func == "redact_code_output":
            out["ret"] = NS[py](inp.get("output", ""), force=inp.get("force", False))
        elif func == "redact_derived_text":
            out["ret"] = NS[py](inp.get("text"))
        elif func == "redact_enabled":
            out["ret"] = NS[py]()
        elif func == "command_reads_secret_file":
            out["ret"] = NS[py](inp.get("command"))
        elif func == "is_secret_file_arg":
            out["ret"] = NS[py](inp.get("arg", ""))
        elif func == "note_env_policy":
            NS[py](inp.get("home_key", ""), inp.get("raw"))
            out["ret"] = None
        elif func == "note_config_policy":
            NS[py](inp.get("home_key", ""), inp.get("raw"))
            out["ret"] = None
        elif func == "key_name_words":
            out["ret"] = NS[py](inp.get("key", ""))
        elif func == "is_qualified_key_name":
            out["ret"] = NS[py](inp.get("key", ""))
        elif func == "is_python_repr_secret_key":
            out["ret"] = NS[py](inp.get("key", ""))
        elif func == "is_secret_field_name":
            out["ret"] = NS[py](inp.get("key"))
        elif func == "decode_url_param_name":
            out["ret"] = NS[py](inp.get("name", ""))
        elif func == "canonical_url_param_name":
            out["ret"] = NS[py](inp.get("name", ""))
        else:
            raise KeyError(func)
    except BaseException as e:  # fail-closed: propagate scripted BaseException specs
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    out["state"] = scalar_state(ctx, inp)
    return out


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except BaseException as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (got.get("trace") == want.get("trace")
              and got.get("ret") == want.get("ret")
              and got.get("state") == want.get("state"))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE redact_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
