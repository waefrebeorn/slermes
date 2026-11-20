#!/usr/bin/env python3
"""Oracle for src/agent/redact_flow2.c (agent/redact.py 40-gap FLOW port).

Drives the LIVE agent/redact.py defs with scripted seams: every regex / foreign
sub-call is a recorder fed the SAME io scripts the C harness feeds its seams.
Compares the ordered seam-name sequence ("trace"), the return ("ret") and the
mutated state ("state"). Live defs are AST-extracted; custom types route through
the FULL builtin exception hierarchy. Catch BaseException; fail closed (0 cases
=> rc 1). Summary: 'ORACLE redact_flow2: N cases, M mismatches'.
"""
import ast
import copy
import json
import os
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "redact.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "redact.py")

# harness func -> live def name
PY = {
    "resolve_profile_policy": "resolve_profile_policy",
    "has_word_bounded_keyword": "_has_word_bounded_keyword",
    "has_password_keyword": "_has_password_keyword",
    "is_glued_credential_env": "_is_glued_credential_env",
    "redact_telegram_token": "_redact_telegram_token",
    "is_b64_pem_key": "_is_b64_pem_key",
    "is_basic_pair_password": "_is_basic_pair_password",
    "last_char_class": "_last_char_class",
    "trailing_guard": "_trailing_guard",
    "compile_prefix_matcher": "_compile_prefix_matcher",
    "compile_prefix_sub_matcher": "_compile_prefix_sub_matcher",
    "is_opaque_prefixed_token": "_is_opaque_prefixed_token",
    "is_glued_identifier": "_is_glued_identifier",
    "redact_approval_display": "redact_approval_display",
    "is_redaction_marker": "_is_redaction_marker",
    "mask_flag_value": "_mask_flag_value",
    "is_flag_placeholder": "_is_flag_placeholder",
    "is_flag_path_reference": "_is_flag_path_reference",
    "redact_cli_secrets": "_redact_cli_secrets",
    "redact_private_key_blocks": "_redact_private_key_blocks",
    "redact_python_repr_fields": "_redact_python_repr_fields",
    "redact_python_diagnostic_repr_fields": "_redact_python_diagnostic_repr_fields",
    "redact_strict_url_credentials": "_redact_strict_url_credentials",
    "redact_json_string_value": "_redact_json_string_value",
    "redact_unterminated_json_string_value": "_redact_unterminated_json_string_value",
    "assignment_sub": "_assignment_sub",
    "redact_json_affixed_key": "_redact_json_affixed_key",
    "redact_json_affixed_key_escaped": "_redact_json_affixed_key_escaped",
    "redact_assignments": "_redact_assignments",
    "redact_userinfo_passwords": "_redact_userinfo_passwords",
    "redact_credential_store_lines": "_redact_credential_store_lines",
    "redact_url_credentials": "_redact_url_credentials",
    "mask_network_path_userinfo": "_mask_network_path_userinfo",
    "redact_phone": "_redact_phone",
    "redact_credential_shapes": "redact_credential_shapes",
    "mask_env_dump_line": "_mask_env_dump_line",
    "plugin_patterns": "_plugin_patterns",
    "rebuild_prefix_matcher": "_rebuild_prefix_matcher",
    "register_redaction_patterns": "register_redaction_patterns",
    "reset_plugin_redaction_patterns": "_reset_plugin_redaction_patterns",
}
# real helpers the ported defs call directly (not themselves seams)
EXTRA_DEFS = {"_is_word_start", "_is_word_end"}


class ReError(Exception):
    pass


_BUILTIN = {
    "ValueError": ValueError, "RuntimeError": RuntimeError, "TypeError": TypeError,
    "KeyError": KeyError, "Exception": Exception, "OSError": OSError,
    "IndexError": IndexError, "AttributeError": AttributeError, "NameError": NameError,
    "LookupError": LookupError, "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "NotImplementedError": NotImplementedError, "ImportError": ImportError, "SyntaxError": SyntaxError,
    "AssertionError": AssertionError, "StopIteration": StopIteration, "BufferError": BufferError,
    "UnicodeError": UnicodeError, "SystemError": SystemError, "Warning": Warning,
    "re_error": ReError, "error": ReError, "JSONDecodeError": json.JSONDecodeError,
}


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    if t == "SystemExit": return SystemExit(msg)
    if t == "KeyboardInterrupt": return KeyboardInterrupt(msg)
    if t == "GeneratorExit": return GeneratorExit(msg)
    if t == "CancelledError":
        import asyncio
        return asyncio.CancelledError(msg)
    cls = _BUILTIN.get(t)
    if cls is None:
        cls = type(t, (Exception,), {})
    try:
        return cls(msg)
    except Exception:
        e = cls.__new__(cls)
        BaseException.__init__(e, msg)
        return e


def extract():
    import re as _re
    ns = {
        "__name__": "agent.redact", "re": _re, "os": os, "json": json, "sys": sys,
        "logging": __import__("logging"), "Any": object, "Optional": object,
        "Callable": object, "match": None,
    }
    ns["_CLI_SECRET_FLAG_RE"] = object()   # identity sentinel for mask_flag_value
    ns["_mask_token"] = lambda s: "***"    # default-arg bindings at def-extract time
    ns["_mask_token_nonreusable"] = lambda s: "***"
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)

    def seg(n):
        return compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec")

    want = set(PY.values()) | EXTRA_DEFS
    got = set()
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in want:
            exec(seg(n), ns)
            got.add(n.name)
    missing = want - got
    if missing:
        print("FATAL redact_flow2: defs missing:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in set(PY.values()) | EXTRA_DEFS}


class Ctx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.state = copy.deepcopy(inp.get("state") or {})

    def script_next(self, name):
        e = self.io.get(name)
        if e is None:
            return None
        if isinstance(e, list):
            i = self.cursors.get(name, 0)
            if i >= len(e):
                return None
            self.cursors[name] = i + 1
            return e[i]
        return e

    def call(self, name):
        self.trace.append(name)
        r = self.script_next(name)
        if isinstance(r, dict):
            if "raise" in r:
                raise make_exc(r["raise"])
            if "ret" in r:
                return r["ret"]
            return r
        return r

    def rec(self, name):
        return lambda *a, **k: self.call(name)


class MatchDouble:
    def __init__(self, d):
        self.d = d or {}
        self.string = self.d.get("string", "")
        self.re = None

    def group(self, *idx):
        if not idx or idx == (0,):
            return self.d.get("g0", "")
        k = str(idx[0])
        return self.d.get("g", {}).get(k, (self.d.get("named") or {}).get(k))

    def groups(self):
        g = self.d.get("g", {})
        keys = sorted([k for k in g if str(k).isdigit()], key=lambda x: int(x))
        return tuple(g.get(k) for k in keys)

    def start(self, g=0):
        return self.d.get("start", 0) if (g == 0 or g == "0") else (self.d.get("starts") or {}).get(str(g), 0)

    def end(self, g=0):
        return self.d.get("end", 0) if (g == 0 or g == "0") else (self.d.get("ends") or {}).get(str(g), 0)


class Rx:
    def __init__(self, ctx, m=None, f=None, s=None, fi=None, sub=None):
        self.ctx, self.m, self.f, self.s, self.fi, self.subn = ctx, m, f, s, fi, sub

    def match(self, x): return self.ctx.call(self.m) if self.m else None
    def fullmatch(self, x): return self.ctx.call(self.f) if self.f else None
    def search(self, x): return self.ctx.call(self.s) if self.s else None
    def finditer(self, x):
        v = self.ctx.call(self.fi) if self.fi else None
        return [MatchDouble(d) for d in (v or [])]
    def sub(self, repl, x): return self.ctx.call(self.subn) if self.subn else x


class DiagRx:
    """Shared diagnostic-line regex double: one "diag_line_match" seam per line."""
    def __init__(self, ctx):
        self.ctx = ctx
        self._line = object()

    def match(self, s):
        if self._line != s:
            self._line = s
            self._val = self.ctx.call("diag_line_match")
        if not isinstance(self._val, dict):
            return None
        d = self._val.get("ret", self._val)
        return MatchDouble({"g": {"prefix": d.get("prefix", ""), "body": d.get("body", "")}}) if isinstance(d, dict) else None


class ReDouble:
    _real = __import__("re")

    def __init__(self, ctx):
        self.ctx = ctx
        self.error = ReError
    def compile(self, p): return self.ctx.call("compile")
    def fullmatch(self, p, s): return self.ctx.call("fullmatch")
    def escape(self, s): return self._real.escape(s)


class LoggingDouble:
    def __init__(self, ctx): self.ctx = ctx
    def getLogger(self, *a): return self
    def debug(self, fmt, *a): return self.ctx.call("logger_debug")


class LoggerDouble:
    def __init__(self, ctx): self.ctx = ctx
    def warning(self, fmt, *a):
        if "empty/non-string" in fmt: n = "log_warning_empty"
        elif "invalid redaction pattern" in fmt: n = "log_warning_invalid"
        else: n = "log_warning_rejected"
        return self.ctx.call(n)
    def debug(self, fmt, *a): return self.ctx.call("log_debug_dup")
    def info(self, fmt, *a): return self.ctx.call("log_info_registered")


class CMDouble:
    def __init__(self, ctx, en, ex): self.ctx, self.en, self.ex = ctx, en, ex
    def __enter__(self): return self.ctx.call(self.en)
    def __exit__(self, *a): self.ctx.call(self.ex); return False


class ContainsList(list):
    def __init__(self, ctx, seam, items=()):
        super().__init__(items); self.ctx, self.seam = ctx, seam
    def __contains__(self, x): return bool(self.ctx.call(self.seam))


class ExtList(list):
    def __init__(self, ctx, seam, *a):
        super().__init__(*a); self.ctx, self.seam = ctx, seam
    def extend(self, x): self.ctx.call(self.seam); super().extend(x)


class RegDict(dict):
    def __init__(self, ctx, seam):
        super().__init__(); self.ctx, self.seam = ctx, seam
    def setdefault(self, k, d):
        if k not in self: super().__setitem__(k, ExtList(self.ctx, self.seam))
        return super().__getitem__(k)
    def clear(self): self.ctx.call("registry_clear"); super().clear()


class PluginDict(dict):
    def __init__(self, ctx): super().__init__(); self.ctx = ctx
    def values(self):
        v = self.ctx.call("plugin_patterns")
        return [v] if isinstance(v, (list, tuple)) else [v]


def install(c, f, inp):
    """Install the live stubs for one function; restore real defs first."""
    for nm, obj in REAL.items():
        NS[nm] = obj
    NS["re"] = ReDouble(c) if f in (
        "trailing_guard", "compile_prefix_matcher", "compile_prefix_sub_matcher",
        "register_redaction_patterns") else __import__("re")
    # default benign module globals
    NS["_CLI_SECRET_FLAG_RE"] = object()
    NS["logging"] = LoggingDouble(c)
    lg = LoggerDouble(c)
    NS["logger"] = lg
    NS["_registry_lock"] = CMDouble(c, "registry_lock_enter", "registry_lock_exit")
    NS["_PLUGIN_PREFIX_PATTERNS"] = PluginDict(c)

    if f == "resolve_profile_policy":
        NS["note_env_policy"] = c.rec("note_env_policy")
        NS["note_config_policy"] = c.rec("note_config_policy")
        agent = types.ModuleType("agent"); agent.__path__ = []
        sys.modules["agent"] = agent
        rs = types.ModuleType("agent.runtime_scope")
        rs.bind_profile_scope = lambda home, hydrate_secrets=False: CMDouble(c, "bind_profile_scope_enter", "bind_profile_scope_exit")
        sys.modules["agent.runtime_scope"] = rs
        hc = types.ModuleType("hermes_constants")
        hc.hermes_home_key = c.rec("hermes_home_key")
        sys.modules["hermes_constants"] = hc
        hcli = types.ModuleType("hermes_cli"); hcli.__path__ = []
        sys.modules["hermes_cli"] = hcli
        cfg = types.ModuleType("hermes_cli.config")
        cfg.load_env = c.rec("load_env")
        cfg.load_config_readonly = c.rec("load_config_readonly")
        sys.modules["hermes_cli.config"] = cfg
    elif f == "has_password_keyword":
        NS["_PASSWORD_WORD_RE"] = Rx(c, fi="finditer")
    elif f == "is_glued_credential_env":
        NS["_GLUED_CREDENTIAL_ENV_RE"] = Rx(c, f="fullmatch")
    elif f == "redact_telegram_token":
        pass
    elif f == "is_b64_pem_key":
        b64 = types.ModuleType("base64")
        def _b64(*a, **k):
            v = c.call("b64decode")
            return v.encode("utf-8") if isinstance(v, str) else v
        b64.b64decode = _b64
        sys.modules["base64"] = b64
    elif f == "is_basic_pair_password":
        NS["_VERSION_TAG_RE"] = Rx(c, s="version_tag_search")
        NS["_WORD_TAG_RE"] = Rx(c, f="word_tag_fullmatch")
    elif f in ("trailing_guard",):
        pass  # re.fullmatch -> "fullmatch"
    elif f in ("compile_prefix_matcher", "compile_prefix_sub_matcher"):
        NS["_trailing_guard"] = c.rec("trailing_guard")
    elif f == "is_opaque_prefixed_token":
        NS["_LONG_ALNUM_RUN_RE"] = Rx(c, fi="finditer")
    elif f == "is_glued_identifier":
        NS["_is_opaque_prefixed_token"] = c.rec("is_opaque_prefixed_token")
    elif f == "redact_approval_display":
        NS["redact_sensitive_text"] = c.rec("redact_sensitive_text")
        NS["sanitize_command_for_display"] = c.rec("sanitize_command_for_display")
    elif f == "is_redaction_marker":
        NS["_REDACTION_MARKER_RE"] = Rx(c, f="fullmatch")
    elif f == "mask_flag_value":
        NS["_is_redaction_marker"] = c.rec("is_redaction_marker")
        NS["_is_flag_placeholder"] = c.rec("is_flag_placeholder")
        NS["_is_flag_path_reference"] = c.rec("is_flag_path_reference")
        NS["_mask_token"] = c.rec("mask")
    elif f == "is_flag_placeholder":
        NS["_FLAG_PLACEHOLDER_RE"] = Rx(c, f="fullmatch")
    elif f == "is_flag_path_reference":
        NS["_PASSWORD_KEY_RE"] = Rx(c, s="password_key_search")
        NS["_OPAQUE_PATH_SEGMENT_RE"] = Rx(c, f="opaque_segment_fullmatch")
    elif f == "redact_cli_secrets":
        for n, kw in {
            "_CLI_SECRET_FLAG_GATE": dict(s="cli_secret_flag_gate"),
            "_CLI_SECRET_FLAG_RE": dict(sub="cli_secret_flag_sub"),
            "_CLI_LOGIN_GATE": dict(s="cli_login_gate"),
            "_CLI_LOGIN_P_RE": dict(sub="cli_login_sub"),
            "_CONNSTR_GATE": dict(s="connstr_gate"),
            "_CONNSTR_SECRET_KEY_RE": dict(sub="connstr_sub"),
        }.items():
            NS[n] = Rx(c, **kw)
    elif f == "redact_private_key_blocks":
        for n, kw in {
            "_PRIVATE_KEY_RE": dict(sub="private_key_sub"),
            "_B64_PEM_RE": dict(sub="b64_pem_sub"),
            "_PGP_PRIVATE_KEY_RE": dict(sub="pgp_private_key_sub"),
            "_PUTTY_PRIVATE_LINES_RE": dict(sub="putty_private_lines_sub"),
            "_AGE_SECRET_KEY_RE": dict(sub="age_secret_key_sub"),
        }.items():
            NS[n] = Rx(c, **kw)
        NS["_redact_partial_private_keys"] = c.rec("redact_partial_private_keys")
    elif f == "redact_python_repr_fields":
        NS["_PYTHON_REPR_FIELD_RE"] = Rx(c, sub="python_repr_field_sub")
    elif f == "redact_python_diagnostic_repr_fields":
        rx = DiagRx(c)
        NS["_PYTEST_DIAGNOSTIC_LINE_RE"] = rx
        NS["_PYTHON_EXCEPTION_LINE_RE"] = rx  # shared: one seam call per line
        NS["_redact_python_repr_fields"] = c.rec("diag_line_repr")
    elif f == "redact_strict_url_credentials":
        NS["_STRICT_URL_PARAM_RE"] = Rx(c, sub="strict_url_param_sub")
        NS["_STRICT_URL_USERINFO_RE"] = Rx(c, sub="strict_url_userinfo_sub")
    elif f == "redact_json_string_value":
        NS["json_header_value_is_credential"] = c.rec("json_header_value_is_credential")
        NS["_mask_token"] = c.rec("mask")
        NS["json"] = _JsonDouble(c)
    elif f == "redact_unterminated_json_string_value":
        NS["_mask_token"] = c.rec("mask")
        NS["json"] = _JsonDouble(c)
    elif f == "assignment_sub":
        NS["_should_redact_assignment"] = c.rec("should_redact_assignment")
    elif f == "redact_json_affixed_key":
        NS["_json_affixed_key_secret"] = c.rec("json_affixed_key_secret")
        NS["_mask_token"] = c.rec("mask")
    elif f == "redact_json_affixed_key_escaped":
        NS["_json_affixed_key_secret"] = c.rec("json_affixed_key_secret")
        NS["_mask_token"] = c.rec("mask")
    elif f == "redact_assignments":
        for n, kw in {
            "_CFG_SECRET_WORD_RE": dict(s="cfg_secret_word_search"),
            "_ENV_ASSIGN_RE": dict(sub="env_assign_sub"),
            "_ENV_ASSIGN_LOWER_RE": dict(sub="env_assign_lower_sub"),
            "_CFG_DOTTED_RE": dict(sub="cfg_dotted_sub"),
            "_CFG_ANCHORED_RE": dict(sub="cfg_anchored_sub"),
            "_SECRET_HEADER_JSON_RE": dict(sub="secret_header_json_sub"),
            "_COOKIE_HEADER_JSON_RE": dict(sub="cookie_header_json_sub"),
            "_SECRET_HEADER_JSON_UNTERMINATED_RE": dict(sub="secret_header_json_unterminated_sub"),
            "_COOKIE_HEADER_JSON_UNTERMINATED_RE": dict(sub="cookie_header_json_unterminated_sub"),
            "_JSON_FIELD_RE": dict(sub="json_field_sub"),
            "_JSON_FIELD_ESCAPED_RE": dict(sub="json_field_escaped_sub"),
            "_JSON_ANY_KEY_FIELD_RE": dict(sub="json_any_key_sub"),
            "_JSON_ANY_KEY_FIELD_ESCAPED_RE": dict(sub="json_any_key_escaped_sub"),
            "_YAML_ASSIGN_RE": dict(sub="yaml_assign_sub"),
        }.items():
            NS[n] = Rx(c, **kw)
        NS["redact_cookie_assignments"] = c.rec("cookie_assignments_sub")
        NS["redact_spaced_cfg_assignments"] = c.rec("spaced_cfg_sub")
        NS["_redact_python_repr_fields"] = c.rec("python_repr_fields_sub")
        NS["_should_redact_assignment"] = c.rec("should_redact_assignment")
        NS["_mask_token"] = c.rec("mask")
        NS["_mask_token_nonreusable"] = c.rec("mask")
    elif f == "redact_userinfo_passwords":
        NS["_URL_USERINFO_PAIR_RE"] = Rx(c, sub="url_userinfo_pair_sub")
    elif f == "redact_credential_store_lines":
        NS["redact_credential_store_lines"] = c.rec("credential_store_lines_sub")
        NS["_mask_token"] = c.rec("mask")
        NS["_mask_token_nonreusable"] = c.rec("mask")
        NS["_should_redact_assignment"] = c.rec("should_redact_assignment")
    elif f == "redact_url_credentials":
        for n, kw in {
            "_DB_CONNSTR_RE": dict(sub="db_connstr_sub"),
            "_WEBHOOK_URL_RE": dict(sub="webhook_url_sub"),
            "_URL_BARE_TOKEN_RE": dict(sub="url_bare_token_sub"),
        }.items():
            NS[n] = Rx(c, **kw)
        NS["redact_db_url_query_passwords"] = c.rec("db_url_query_passwords_sub")
        NS["_redact_userinfo_passwords"] = c.rec("userinfo_passwords_sub")
        NS["_mask_token"] = c.rec("mask")
        NS["_mask_token_nonreusable"] = c.rec("mask")
    elif f == "mask_network_path_userinfo":
        NS["_is_userinfo_placeholder"] = c.rec("is_userinfo_placeholder")
    elif f == "redact_credential_shapes":
        NS["outside_spans"] = lambda text, spans, fn: fn(text)
        for n, kw in {
            "_ZHIPU_API_KEY_RE": dict(sub="zhipu_api_key_sub"),
            "_BASIC_PAIR_ASSIGN_RE": dict(sub="basic_pair_assign_sub"),
            "_AUTH_GATE_RE": dict(s="auth_gate_search"),
            "_AUTH_HEADER_RE": dict(sub="auth_header_sub"),
            "_TELEGRAM_RE": dict(sub="telegram_sub"),
            "_NETWORK_PATH_USERINFO_RE": dict(sub="network_path_userinfo_sub"),
            "_KV_CONNSTR_GATE_RE": dict(s="kv_connstr_gate_search"),
            "_JWT_RE": dict(sub="jwt_sub"),
            "_WEBAPP_LINK_CREDENTIAL_RE": dict(sub="webapp_link_credential_sub"),
            "_SIGNAL_PHONE_RE": dict(sub="signal_phone_sub"),
        }.items():
            NS[n] = Rx(c, **kw)
        for n, s in {
            "_redact_private_key_blocks": "private_key_blocks_sub",
            "_has_known_prefix_substring": "has_known_prefix_substring",
            "mask_control_split_tokens": "mask_control_split_tokens_sub",
            "mask_prefix_matches": "mask_prefix_matches_sub",
            "_redact_discord_tokens": "discord_tokens_sub",
            "_redact_credential_store_lines": "credential_store_lines_sub",
            "aws_credential_process_keys": "aws_credential_process_keys",
            "_redact_assignments": "redact_assignments_sub",
            "_redact_cli_secrets": "cli_secrets_sub",
            "redact_header_credentials": "header_credentials_sub",
            "_redact_url_credentials": "url_credentials_sub",
            "_redact_kv_connection_strings": "kv_connstr_sub",
            "_redact_strict_url_credentials": "strict_url_credentials_sub",
            "_redact_form_body": "form_body_sub",
        }.items():
            NS[n] = c.rec(s)
        NS["_mask_token"] = c.rec("mask")
        NS["_mask_token_nonreusable"] = c.rec("mask")
    elif f == "mask_env_dump_line":
        NS["_mask_token"] = c.rec("mask")
        NS["_should_redact_assignment"] = c.rec("should_redact_assignment")
        NS["_ENV_ASSIGN_RE"] = Rx(c, sub="env_assign_sub")
    elif f == "plugin_patterns":
        pass  # PluginDict installed above
    elif f == "rebuild_prefix_matcher":
        NS["_PREFIX_PATTERNS"] = inp.get("patterns", [])
        NS["_extract_literal_prefix"] = c.rec("extract_literal_prefix")
        NS["_plugin_patterns"] = c.rec("plugin_patterns")
        NS["_compile_prefix_matcher"] = c.rec("compile_prefix_matcher")
        NS["_compile_prefix_sub_matcher"] = c.rec("compile_prefix_sub_matcher")
    elif f == "register_redaction_patterns":
        NS["_PREFIX_PATTERNS"] = ContainsList(c, "is_duplicate")
        NS["_PATTERN_REJECT_RULES"] = [(lambda p: c.call("pattern_reject"), "rejected")]
        NS["_PLUGIN_PREFIX_PATTERNS"] = RegDict(c, "registry_extend")
        NS["_plugin_patterns"] = lambda: []
        NS["_rebuild_prefix_matcher"] = c.rec("rebuild_prefix_matcher")
        NS["logger"] = lg
    elif f == "reset_plugin_redaction_patterns":
        NS["_PLUGIN_PREFIX_PATTERNS"] = RegDict(c, "registry_clear")
        NS["_rebuild_prefix_matcher"] = c.rec("rebuild_prefix_matcher")


class _JsonDouble:
    JSONDecodeError = json.JSONDecodeError

    def __init__(self, ctx): self.ctx = ctx
    def loads(self, s):
        r = self.ctx.call("json_loads")
        if isinstance(r, dict) and "raise" in r:
            raise make_exc(r["raise"])
        return r
    def dumps(self, v, ensure_ascii=False):
        return json.dumps(v, ensure_ascii=ensure_ascii)


def call_live(f, inp):
    g = lambda k, d=None: inp.get(k, d)
    md = lambda: MatchDouble(g("m") or {})
    if f == "resolve_profile_policy": return NS[PY[f]](g("home", "")), False
    if f == "has_word_bounded_keyword": return NS[PY[f]](g("key", ""), Rx(CUR[0], fi="finditer")), False
    if f == "has_password_keyword": return NS[PY[f]](g("key", "")), False
    if f == "is_glued_credential_env": return NS[PY[f]](g("key", "")), False
    if f == "redact_telegram_token": return NS[PY[f]](md()), False
    if f == "is_b64_pem_key": return NS[PY[f]](g("blob", "")), False
    if f == "is_basic_pair_password": return NS[PY[f]](g("password", "")), False
    if f == "last_char_class": return NS[PY[f]](g("pattern", "")), False
    if f == "trailing_guard": return NS[PY[f]](g("pattern", "")), False
    if f == "compile_prefix_matcher": return NS[PY[f]](g("patterns", [])), False
    if f == "compile_prefix_sub_matcher": return NS[PY[f]](g("patterns", [])), False
    if f == "is_opaque_prefixed_token": return NS[PY[f]](g("token", "")), False
    if f == "is_glued_identifier": return NS[PY[f]](md(), token=g("token")), False
    if f == "redact_approval_display": return NS[PY[f]](g("text", ""), force=g("force", False), prose=g("prose", False)), False
    if f == "is_redaction_marker": return NS[PY[f]](md(), g("value", "")), False
    if f == "mask_flag_value":
        _m = md()
        if (inp.get("m") or {}).get("is_cli_flag"): _m.re = NS["_CLI_SECRET_FLAG_RE"]
        return NS[PY[f]](_m, mask=CUR[0].rec("mask")), False
    if f == "is_flag_placeholder": return NS[PY[f]](g("value", "")), False
    if f == "is_flag_path_reference": return NS[PY[f]](g("flag", ""), g("value", "")), False
    if f == "redact_cli_secrets": return NS[PY[f]](g("text", "")), False
    if f == "redact_private_key_blocks": return NS[PY[f]](g("text", "")), False
    if f == "redact_python_repr_fields": return NS[PY[f]](g("text", "")), False
    if f == "redact_python_diagnostic_repr_fields": return NS[PY[f]](g("text", "")), False
    if f == "redact_strict_url_credentials": return NS[PY[f]](g("text", "")), False
    if f == "redact_json_string_value": return NS[PY[f]](md(), mask=CUR[0].rec("mask")), False
    if f == "redact_unterminated_json_string_value": return NS[PY[f]](md(), mask=CUR[0].rec("mask")), False
    if f == "assignment_sub":
        sub = NS[PY[f]](CUR[0].rec("render"), check_keyword=g("check_keyword", True))
        out = []
        for e in g("matches", []):
            out.append(sub(MatchDouble(e)))
        return "".join(x or "" for x in out), False
    if f == "redact_json_affixed_key": return NS[PY[f]](md(), CUR[0].rec("mask")), False
    if f == "redact_json_affixed_key_escaped": return NS[PY[f]](md(), CUR[0].rec("mask")), False
    if f == "redact_assignments": return NS[PY[f]](g("text", ""), mask_nonreusable=g("mask_nonreusable", False)), False
    if f == "redact_userinfo_passwords": return NS[PY[f]](g("text", "")), False
    if f == "redact_credential_store_lines": return NS[PY[f]](g("text", ""), mask_nonreusable=g("mask_nonreusable", False)), False
    if f == "redact_url_credentials": return NS[PY[f]](g("text", ""), g("code_file", False), file_read=g("file_read", False)), False
    if f == "mask_network_path_userinfo": return NS[PY[f]](md()), False
    if f == "redact_phone": return NS[PY[f]](md()), False
    if f == "redact_credential_shapes":
        return NS[PY[f]](g("text", ""), force=g("force", False), code_file=g("code_file", False),
                         file_read=g("file_read", False), secret_file=g("secret_file", False),
                         redact_url_credentials=g("redact_url_credentials", False),
                         keep_phone_numbers=g("keep_phone_numbers", False)), False
    if f == "mask_env_dump_line": return NS[PY[f]](md(), CUR[0].rec("mask")), False
    if f == "plugin_patterns": return NS[PY[f]](), False
    if f == "rebuild_prefix_matcher": NS[PY[f]](); return None, False
    if f == "register_redaction_patterns": return NS[PY[f]](g("patterns", []), source=g("source", "plugin")), False
    if f == "reset_plugin_redaction_patterns": NS[PY[f]](); return None, False
    raise KeyError(f)


CUR = [None]


def run_case(f, inp):
    c = Ctx(inp)
    CUR[0] = c
    install(c, f, inp)
    out = {"ret": None, "trace": c.trace}
    try:
        ret, _ = call_live(f, inp)
        out["ret"] = ret
    except BaseException as e:
        out["ret"] = {"__raise__": type(e).__name__, "message": str(e)}
    out["state"] = {k: c.state[k] for k in (inp.get("state") or {}) if k in c.state}
    return out


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        f = c.get("func", "")
        if f not in PY:
            mism += 1; print(f"PYERR {f}: unknown func"); continue
        try:
            want = run_case(f, c.get("in") or {})
        except BaseException as e:
            mism += 1; print(f"PYERR {f}: {type(e).__name__}: {e}"); continue
        got = c.get("out") or {}
        if not (got.get("trace") == want.get("trace") and got.get("ret") == want.get("ret")
                and got.get("state") == want.get("state")):
            mism += 1
            print(f"MISMATCH {f}")
            print(f"  C  ={json.dumps(got, sort_keys=True, default=str)}")
            print(f"  PY ={json.dumps(want, sort_keys=True, default=str)}")
    print(f"ORACLE redact_flow2: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
