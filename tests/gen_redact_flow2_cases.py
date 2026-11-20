#!/usr/bin/env python3
"""Generate tests/fixtures/redact_flow2.jsonl (>=5 cases per ported func).

Case shape: {"func", "in": {args..., "io": {seam -> script}, "m": match-double}}.
A bare scalar entry is a constant return; a list entry is a per-call queue;
{"ret": v} is a return of v; {"raise": {...}} drives the builtin exception
hierarchy. List-valued seam results are double-wrapped: L(x) == [x].
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "redact_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def L(v):
    return [v]


def X(t, m=""):
    return {"raise": {"type": t, "message": m}}


def M(g0="", g=None, start=0, end=0, string="", starts=None, ends=None, cli=False):
    d = {"g0": g0, "g": g or {}, "start": start, "end": end, "string": string}
    if starts: d["starts"] = starts
    if ends: d["ends"] = ends
    if cli: d["is_cli_flag"] = True
    return d


# ── resolve_profile_policy ──────────────────────────────────────────────
add("resolve_profile_policy", home="/home/u", io={
    "bind_profile_scope_enter": R({}), "hermes_home_key": R("HOME"),
    "load_env": R({"HERMES_REDACT_SECRETS": "true"}), "note_env_policy": R(None),
    "load_config_readonly": R({"security": {"redact_secrets": False}}),
    "note_config_policy": R(None), "bind_profile_scope_exit": R(None)})
add("resolve_profile_policy", home="/home/a", io={
    "bind_profile_scope_enter": R({}), "hermes_home_key": X("ValueError", "no home"),
    "bind_profile_scope_exit": R(None), "logger_debug": R(None)})
add("resolve_profile_policy", home="/home/b", io={
    "bind_profile_scope_enter": R({}), "hermes_home_key": R("HOME"),
    "load_env": X("SystemExit", "bye"), "bind_profile_scope_exit": R(None),
    "logger_debug": R(None)})
add("resolve_profile_policy", home="/home/c", io={
    "bind_profile_scope_enter": R({}), "hermes_home_key": R("HOME"),
    "load_env": X("KeyboardInterrupt", "ctrl-c"), "bind_profile_scope_exit": R(None)})
add("resolve_profile_policy", home="/home/d", io={
    "bind_profile_scope_enter": R({}), "hermes_home_key": R("HOME"),
    "load_env": R({}), "note_env_policy": R(None),
    "load_config_readonly": X("OSError", "disk"), "bind_profile_scope_exit": R(None),
    "logger_debug": R(None)})

# ── has_word_bounded_keyword ────────────────────────────────────────────
add("has_word_bounded_keyword", key="token", io={"finditer": L([{"start": 0, "end": 5}])})
add("has_word_bounded_keyword", key="dbtoken", io={"finditer": L([{"start": 2, "end": 7}])})
add("has_word_bounded_keyword", key="APIToken", io={"finditer": L([{"start": 0, "end": 8}])})
add("has_word_bounded_keyword", key="foo", io={"finditer": L([])})
add("has_word_bounded_keyword", key="clientSecret", io={"finditer": L([{"start": 6, "end": 12}])})

# ── has_password_keyword ────────────────────────────────────────────────
add("has_password_keyword", key="dbpassword", io={"finditer": L([{"end": 10}])})
add("has_password_keyword", key="passwordless", io={"finditer": L([{"end": 8}])})
add("has_password_keyword", key="mypasswd", io={"finditer": L([{"end": 8}])})
add("has_password_keyword", key="foo", io={"finditer": L([])})
add("has_password_keyword", key="secret", io={"finditer": L([{"end": 6}])})

# ── is_glued_credential_env ─────────────────────────────────────────────
add("is_glued_credential_env", key="AWS_SECRET_ACCESS_KEY", io={"fullmatch": R("m")})
add("is_glued_credential_env", key="\"PGPASSWORD\"", io={"fullmatch": R(None)})
add("is_glued_credential_env", key="foo", io={"fullmatch": R(None)})
add("is_glued_credential_env", key="OPENAI_API_KEY", io={"fullmatch": R("m")})
add("is_glued_credential_env", key="", io={"fullmatch": R(None)})

# ── redact_telegram_token ───────────────────────────────────────────────
add("redact_telegram_token", m=M(g0="123:ABC", g={"boundary": ":", "id": "123", "bot": "ABC"}))
add("redact_telegram_token", m=M(g0="%41x:1", g={"boundary": "%41", "csi": None, "id": "1", "bot": None}))
add("redact_telegram_token", m=M(g0="%2fx:1", g={"boundary": "%2f", "csi": None, "id": "1", "bot": None}))
add("redact_telegram_token", m=M(g0="x%41:1", g={"boundary": "%41", "csi": "y", "id": "1", "bot": "B"}))
add("redact_telegram_token", m=M(g0="%110000x:1", g={"boundary": "%110000", "csi": None, "id": "1"}))

# ── is_b64_pem_key ──────────────────────────────────────────────────────
add("is_b64_pem_key", blob="LS0tLS1CRUdJTiBSU0EgUFJJVkFURSBLRVktLS0tLQ", io={"b64decode": R("-----BEGIN RSA PRIVATE KEY-----")})
add("is_b64_pem_key", blob="aGVsbG8=", io={"b64decode": R("hello")})
add("is_b64_pem_key", blob="!!!!", io={"b64decode": X("ValueError", "bad")})
add("is_b64_pem_key", blob="", io={"b64decode": R("")})
add("is_b64_pem_key", blob="Y2VydA==", io={"b64decode": R("-----BEGIN CERTIFICATE-----")})

# ── is_basic_pair_password ──────────────────────────────────────────────
add("is_basic_pair_password", password="hunter2abc", io={"version_tag_search": R(None), "word_tag_fullmatch": R(None)})
add("is_basic_pair_password", password="v1.2.3", io={"version_tag_search": R("m")})
add("is_basic_pair_password", password="12345678", io={"version_tag_search": R(None), "word_tag_fullmatch": R(None)})
add("is_basic_pair_password", password="TAG", io={"version_tag_search": R(None), "word_tag_fullmatch": R("m")})
add("is_basic_pair_password", password="abc", io={"version_tag_search": R(None), "word_tag_fullmatch": R(None)})

# ── last_char_class ─────────────────────────────────────────────────────
add("last_char_class", pattern="ghp_[A-Za-z0-9]{10,}")
add("last_char_class", pattern="abc")
add("last_char_class", pattern="sk-[a-z]\\d+")
add("last_char_class", pattern="[^_]foo[0-9]")
add("last_char_class", pattern="x\\[y\\]")

# ── trailing_guard ──────────────────────────────────────────────────────
add("trailing_guard", pattern="abc", io={"fullmatch": []})
add("trailing_guard", pattern="x[A-Za-z0-9_]+", io={"fullmatch": [R(True), R(False)]})
add("trailing_guard", pattern="x[a-z]+", io={"fullmatch": [R(False), R(False)]})
add("trailing_guard", pattern="x[a-z-]+", io={"fullmatch": [R(False), R(True)]})
add("trailing_guard", pattern="x[0-9_]+", io={"fullmatch": [R(True), R(False)]})

# ── compile_prefix_matcher ──────────────────────────────────────────────
add("compile_prefix_matcher", patterns=["ghp_[A-Za-z0-9]+"], io={"trailing_guard": R("(?![A-Za-z0-9])"), "compile": R("RX")})
add("compile_prefix_matcher", patterns=["sk-[a-z]+", "xoxb-[0-9]+"], io={"trailing_guard": [R("G1"), R("G2")], "compile": R("RX")})
add("compile_prefix_matcher", patterns=[], io={"compile": R("RX")})
add("compile_prefix_matcher", patterns=["ya29\\.[A-Za-z0-9_-]+"], io={"trailing_guard": R("G"), "compile": R("RX")})
add("compile_prefix_matcher", patterns=["a", "b", "c"], io={"trailing_guard": [R("A"), R("B"), R("C")], "compile": R("RX")})

# ── compile_prefix_sub_matcher ──────────────────────────────────────────
add("compile_prefix_sub_matcher", patterns=["ghp_[A-Za-z0-9]+"], io={"trailing_guard": R("G"), "compile": R("RX")})
add("compile_prefix_sub_matcher", patterns=["sk-[a-z]+", "gho_[0-9]+"], io={"trailing_guard": [R("G1"), R("G2")], "compile": R("RX")})
add("compile_prefix_sub_matcher", patterns=[], io={"compile": R("RX")})
add("compile_prefix_sub_matcher", patterns=["a+b", "a.c"], io={"trailing_guard": [R("G1"), R("G2")], "compile": R("RX")})
add("compile_prefix_sub_matcher", patterns=["x[0-9]+", "y[a-z]+", "z-foo"], io={"trailing_guard": [R("A"), R("B"), R("C")], "compile": R("RX")})

# ── is_opaque_prefixed_token ────────────────────────────────────────────
add("is_opaque_prefixed_token", token="abc", io={"finditer": L([{"g0": "abc"}])})
add("is_opaque_prefixed_token", token="123", io={"finditer": L([{"g0": "123"}])})
add("is_opaque_prefixed_token", token="aB3", io={"finditer": L([{"g0": "aB3"}])})
add("is_opaque_prefixed_token", token="abc", io={"finditer": L([])})
add("is_opaque_prefixed_token", token="foo_1", io={"finditer": L([{"g0": "foo"}, {"g0": "1"}])})

# ── is_glued_identifier ─────────────────────────────────────────────────
add("is_glued_identifier", m=M(start=5, string="backup-skh", g={"1": "skh"}), io={"is_opaque_prefixed_token": R(False)})
add("is_glued_identifier", m=M(start=0, string="skh", g={"1": "skh"}))
add("is_glued_identifier", m=M(start=5, string="backup-GOCSPX-abc", g={"1": "GOCSPX-abc"}))
add("is_glued_identifier", m=M(start=5, string="backup-aB3", g={"1": "aB3"}), io={"is_opaque_prefixed_token": R(True)})
add("is_glued_identifier", m=M(start=5, string="backup-ya29.x", g={"1": "ya29.x"}), token="ya29.x")

# ── redact_approval_display ─────────────────────────────────────────────
add("redact_approval_display", text="--password=secret", force=True, prose=False,
    io={"redact_sensitive_text": R("--password=***"), "sanitize_command_for_display": R("SAN")})
add("redact_approval_display", text=None, io={"redact_sensitive_text": R(""), "sanitize_command_for_display": R("")})
add("redact_approval_display", text="hello", io={"redact_sensitive_text": R("hello"), "sanitize_command_for_display": R("hello")})
add("redact_approval_display", text="x", force=True, prose=True,
    io={"redact_sensitive_text": R("x"), "sanitize_command_for_display": R("P")})
add("redact_approval_display", text="y", io={"redact_sensitive_text": X("ValueError", "boom")})

# ── is_redaction_marker ─────────────────────────────────────────────────
add("is_redaction_marker", value="[REDACTED", m=M(string="[REDACTED PRIVATE KEY]", ends={"value": 9}))
add("is_redaction_marker", value="[REDACTED", m=M(string="[REDACTED OTHER]", ends={"value": 9}))
add("is_redaction_marker", value="[redacted]", io={"fullmatch": R("m")})
add("is_redaction_marker", value="hello", io={"fullmatch": R(None)})
add("is_redaction_marker", value="", io={"fullmatch": R(None)})

# ── mask_flag_value ─────────────────────────────────────────────────────
add("mask_flag_value", m=M(g0="--password=secret", g={"value": "secret", "sep": "=", "flag": "--password"},
                            starts={"value": 11}, ends={"value": 17}, cli=True),
    io={"is_redaction_marker": R(False), "mask": R("***")})
add("mask_flag_value", m=M(g0="--token $VAR", g={"value": "$VAR", "sep": " ", "flag": "--token"},
                            starts={"value": 8}, ends={"value": 12}, cli=True),
    io={"is_redaction_marker": R(False)})
add("mask_flag_value", m=M(g0="--token 123", g={"value": "123", "sep": " ", "flag": "--token"},
                            starts={"value": 8}, ends={"value": 11}, cli=True),
    io={"is_redaction_marker": R(False)})
add("mask_flag_value", m=M(g0="--token placeholder", g={"value": "placeholder", "sep": " ", "flag": "--token"},
                            starts={"value": 8}, ends={"value": 19}, cli=True),
    io={"is_redaction_marker": R(False), "is_flag_placeholder": R(True)})
add("mask_flag_value", m=M(g0="--password=secret", g={"value": "secret", "sep": "=", "flag": "--password"},
                            starts={"value": 11}, ends={"value": 17}, cli=False),
    io={"is_redaction_marker": R(True)})

# ── is_flag_placeholder ─────────────────────────────────────────────────
add("is_flag_placeholder", value="placeholder", io={"fullmatch": R("m")})
add("is_flag_placeholder", value="secret123", io={"fullmatch": R(None)})
add("is_flag_placeholder", value="'quoted'", io={"fullmatch": R("m")})
add("is_flag_placeholder", value="", io={"fullmatch": R(None)})
add("is_flag_placeholder", value="word", io={"fullmatch": R("m")})

# ── is_flag_path_reference ──────────────────────────────────────────────
add("is_flag_path_reference", flag="--password", value="~/.config/creds.json",
    io={"password_key_search": R(None), "opaque_segment_fullmatch": [R(False), R(False), R(False)]})
add("is_flag_path_reference", flag="--token", value="abc", io={"password_key_search": R(None)})
add("is_flag_path_reference", flag="--credentials", value="~/.config/x",
    io={"password_key_search": R(None), "opaque_segment_fullmatch": [R(False), R(False), R(True)]})
add("is_flag_path_reference", flag="--password", value="/x", io={"password_key_search": R("m")})
add("is_flag_path_reference", flag="--token", value="../a/b",
    io={"password_key_search": R(None), "opaque_segment_fullmatch": [R(False), R(False), R(False)]})

# ── redact_cli_secrets ──────────────────────────────────────────────────
add("redact_cli_secrets", text="--password=secret",
    io={"cli_secret_flag_gate": R(True), "cli_secret_flag_sub": R("--password=***"),
        "cli_login_gate": R(False), "connstr_gate": R(False)})
add("redact_cli_secrets", text="hello", io={"connstr_gate": R(False)})
add("redact_cli_secrets", text="login --token abc",
    io={"cli_secret_flag_gate": R(False), "cli_login_gate": R(True), "cli_login_sub": R("login --token ***"),
        "connstr_gate": R(False)})
add("redact_cli_secrets", text="Password=abc;", io={"connstr_gate": R(True), "connstr_sub": R("Password=***;")})
add("redact_cli_secrets", text="--password=s", io={"cli_secret_flag_gate": X("ValueError", "bad")})

# ── redact_private_key_blocks ───────────────────────────────────────────
add("redact_private_key_blocks", text="-----BEGIN X-----\nbody\n-----END X-----", io={"private_key_sub": R("[REDACTED PRIVATE KEY]")})
add("redact_private_key_blocks", text="PRIVATE KEY----- partial", io={"redact_partial_private_keys": R("clean")})
add("redact_private_key_blocks", text="LS0tLS1CRUdJTi", io={"b64_pem_sub": R("[REDACTED PRIVATE KEY]")})
add("redact_private_key_blocks", text="END PGP PRIVATE KEY BLOCK", io={"pgp_private_key_sub": R("[REDACTED PRIVATE KEY]")})
add("redact_private_key_blocks", text="Private-Lines: abc", io={"putty_private_lines_sub": R("clean")})

# ── redact_python_repr_fields ───────────────────────────────────────────
add("redact_python_repr_fields", text="{'API_KEY': 'secret'}", io={"python_repr_field_sub": R("{'API_KEY': '***'}")})
add("redact_python_repr_fields", text="hello", io={"python_repr_field_sub": R("hello")})
add("redact_python_repr_fields", text="{'x': 'y'}", io={"python_repr_field_sub": R("{'x': 'y'}")})
add("redact_python_repr_fields", text="", io={"python_repr_field_sub": R("")})
add("redact_python_repr_fields", text="a", io={"python_repr_field_sub": X("ValueError", "bad")})

# ── redact_python_diagnostic_repr_fields ────────────────────────────────
add("redact_python_diagnostic_repr_fields", text="assert 1 == 2\n",
    io={"diag_line_match": [R({"prefix": "assert ", "body": "1 == 2"})], "diag_line_repr": R("REDACTED")})
add("redact_python_diagnostic_repr_fields", text="plain line\n",
    io={"diag_line_match": [R(None)]})
add("redact_python_diagnostic_repr_fields", text="E   AssertionError: x\r\n",
    io={"diag_line_match": [R({"prefix": "E   ", "body": "AssertionError: x"})], "diag_line_repr": R("E   MASK")})
add("redact_python_diagnostic_repr_fields", text="", io={})
add("redact_python_diagnostic_repr_fields", text="a\nb\n",
    io={"diag_line_match": [R(None), R({"prefix": "", "body": "b"})], "diag_line_repr": R("Z")})

# ── redact_strict_url_credentials ──────────────────────────────────────
add("redact_strict_url_credentials", text="http://h/?token=abc",
    io={"strict_url_param_sub": R("http://h/?token=***"), "strict_url_userinfo_sub": R("http://h/?token=***")})
add("redact_strict_url_credentials", text="http://u:p@h",
    io={"strict_url_param_sub": R("http://u:p@h"), "strict_url_userinfo_sub": R("//***:***@h")})
add("redact_strict_url_credentials", text="hello",
    io={"strict_url_param_sub": R("hello"), "strict_url_userinfo_sub": R("hello")})
add("redact_strict_url_credentials", text="", io={"strict_url_param_sub": R(""), "strict_url_userinfo_sub": R("")})
add("redact_strict_url_credentials", text="x", io={"strict_url_param_sub": X("ValueError", "bad")})

# ── redact_json_string_value ────────────────────────────────────────────
add("redact_json_string_value", m=M(g0="\"api_key\": \"secret\"", g={"1": "\"api_key\"", "2": "secret", "3": "\""}),
    io={"json_loads": R("secret"), "json_header_value_is_credential": R(True), "mask": R("***")})
add("redact_json_string_value", m=M(g0="\"page\": \"1\"", g={"1": "\"page\"", "2": "1", "3": "\""}),
    io={"json_loads": R("1"), "json_header_value_is_credential": R(False)})
add("redact_json_string_value", m=M(g0="\"api_key\": \"bad\\q\"", g={"1": "\"api_key\"", "2": "bad\\q", "3": "\""}),
    io={"json_loads": X("JSONDecodeError", "bad"), "json_header_value_is_credential": R(True), "mask": R("***")})
add("redact_json_string_value", m=M(g0="a", g={"1": "\"token\"", "2": "x", "3": "\""}),
    io={"json_loads": R("x"), "json_header_value_is_credential": R(True), "mask": R("M")})
add("redact_json_string_value", m=M(g0="b", g={"1": "\"k\"", "2": "v", "3": "\""}),
    io={"json_loads": R("v"), "json_header_value_is_credential": R(False)})

# ── redact_unterminated_json_string_value ───────────────────────────────
add("redact_unterminated_json_string_value", m=M(g0="\"api_key\": \"secret", g={"1": "\"api_key\": \"", "2": "secret"}),
    io={"json_loads": R("secret"), "mask": R("***")})
add("redact_unterminated_json_string_value", m=M(g0="a", g={"1": "P", "2": "v"}),
    io={"json_loads": X("JSONDecodeError", "bad"), "mask": R("***")})
add("redact_unterminated_json_string_value", m=M(g0="b", g={"1": "P", "2": "x"}),
    io={"json_loads": R("x"), "mask": R("M")})
add("redact_unterminated_json_string_value", m=M(g0="c", g={"1": "P", "2": ""}),
    io={"json_loads": R(""), "mask": R("")})
add("redact_unterminated_json_string_value", m=M(g0="d", g={"1": "P", "2": "abc"}),
    io={"json_loads": R("abc"), "mask": R("R")})

# ── assignment_sub ──────────────────────────────────────────────────────
add("assignment_sub", check_keyword=True,
    matches=[{"g0": "API_KEY=abc", "g": {"0": "API_KEY", "1": "abc", "last": "abc"}, "has_render": True}],
    io={"should_redact_assignment": R(True), "render": R("API_KEY=***")})
add("assignment_sub", check_keyword=True,
    matches=[{"g0": "NAME=abc", "g": {"0": "NAME", "1": "abc", "last": "abc"}}],
    io={"should_redact_assignment": R(False)})
add("assignment_sub", check_keyword=False,
    matches=[{"g0": "a=1", "g": {"0": "a", "1": "1", "last": "1"}, "has_render": True},
             {"g0": "b=2", "g": {"0": "b", "1": "2", "last": "2"}}],
    io={"should_redact_assignment": [R(True), R(False)], "render": R("a=***")})
add("assignment_sub", matches=[], io={})
add("assignment_sub", check_keyword=True,
    matches=[{"g0": "K=v", "g": {"0": "K", "1": "v", "last": "v"}, "has_render": True}],
    io={"should_redact_assignment": X("ValueError", "bad")})

# ── redact_json_affixed_key ─────────────────────────────────────────────
add("redact_json_affixed_key", m=M(g0="\"apiKey\": \"v\"", g={"1": "\"apiKey\"", "2": "apiKey", "3": "v"}),
    io={"json_affixed_key_secret": R(True), "mask": R("***")})
add("redact_json_affixed_key", m=M(g0="\"foo\": \"v\"", g={"1": "\"foo\"", "2": "foo", "3": "v"}),
    io={"json_affixed_key_secret": R(False)})
add("redact_json_affixed_key", m=M(g0="a", g={"1": "K", "2": "token", "3": "s"}),
    io={"json_affixed_key_secret": R(True), "mask": R("M")})
add("redact_json_affixed_key", m=M(g0="b", g={"1": "K", "2": "k", "3": ""}),
    io={"json_affixed_key_secret": R(False)})
add("redact_json_affixed_key", m=M(g0="c", g={"1": "K", "2": "api_key", "3": "x"}),
    io={"json_affixed_key_secret": X("ValueError", "bad")})

# ── redact_json_affixed_key_escaped ────────────────────────────────────
add("redact_json_affixed_key_escaped", m=M(g0="\"apiKey\": \\\"v\\\"", g={"1": "\"apiKey\"", "2": "\\\"", "3": "apiKey", "4": "v"}),
    io={"json_affixed_key_secret": R(True), "mask": R("***")})
add("redact_json_affixed_key_escaped", m=M(g0="\"foo\": \\\"v\\\"", g={"1": "\"foo\"", "2": "\\\"", "3": "foo", "4": "v"}),
    io={"json_affixed_key_secret": R(False)})
add("redact_json_affixed_key_escaped", m=M(g0="a", g={"1": "K", "2": "\\\"", "3": "token", "4": "s"}),
    io={"json_affixed_key_secret": R(True), "mask": R("M")})
add("redact_json_affixed_key_escaped", m=M(g0="b", g={"1": "K", "2": "\\\"", "3": "k", "4": ""}),
    io={"json_affixed_key_secret": R(False)})
add("redact_json_affixed_key_escaped", m=M(g0="c", g={"1": "K", "2": "\\\"", "3": "secret", "4": "x"}),
    io={"json_affixed_key_secret": R(True), "mask": R("R")})

# ── redact_assignments ──────────────────────────────────────────────────
add("redact_assignments", text="API_KEY=abc", io={
    "cookie_assignments_sub": R("API_KEY=abc"), "cfg_secret_word_search": [R(False), R(False)],
    "env_assign_sub": R("API_KEY=***"), "env_assign_lower_sub": R("API_KEY=***"),
    "cfg_secret_word_search2": R(False)})
add("redact_assignments", text="http://h/?token=abc", io={
    "cookie_assignments_sub": R("http://h/?token=abc"), "env_assign_sub": R("http://h/?token=abc")})
add("redact_assignments", text='{"api_key": "secret"}', io={
    "secret_header_json_sub": R("X"), "cookie_header_json_sub": R("X"),
    "secret_header_json_unterminated_sub": R("X"), "cookie_header_json_unterminated_sub": R("X"),
    "json_field_sub": R("X"), "json_any_key_sub": R("X"), "yaml_assign_sub": R("X")})
add("redact_assignments", text="{'API_KEY': 'x'}", io={
    "python_repr_fields_sub": R("X"), "yaml_assign_sub": R("X")})
add("redact_assignments", text="TOKEN=abc", mask_nonreusable=True, io={
    "cookie_assignments_sub": R("TOKEN=abc"), "cfg_secret_word_search": [R(True), R(True)],
    "spaced_cfg_sub": [R("TOKEN=abc"), R("TOKEN=abc")],
    "env_assign_sub": R("TOKEN=«redacted:x»"),
    "env_assign_lower_sub": R("TOKEN=«redacted:x»"),
    "cfg_dotted_sub": R("TOKEN=«redacted:x»"),
    "cfg_anchored_sub": R("TOKEN=«redacted:x»")})

# ── redact_userinfo_passwords ───────────────────────────────────────────
add("redact_userinfo_passwords", text="http://u:p@h", io={"url_userinfo_pair_sub": R("http://u:***@h")})
add("redact_userinfo_passwords", text="hello", io={"url_userinfo_pair_sub": R("hello")})
add("redact_userinfo_passwords", text="", io={"url_userinfo_pair_sub": R("")})
add("redact_userinfo_passwords", text="x", io={"url_userinfo_pair_sub": R("x")})
add("redact_userinfo_passwords", text="y", io={"url_userinfo_pair_sub": X("ValueError", "bad")})

# ── redact_credential_store_lines ──────────────────────────────────────
add("redact_credential_store_lines", text="password=abc", mask_nonreusable=True, io={"credential_store_lines_sub": R("password=***")})
add("redact_credential_store_lines", text="hello", io={"credential_store_lines_sub": R("hello")})
add("redact_credential_store_lines", text="", io={"credential_store_lines_sub": R("")})
add("redact_credential_store_lines", text="x", io={"credential_store_lines_sub": R("x")})
add("redact_credential_store_lines", text="y", io={"credential_store_lines_sub": X("ValueError", "bad")})

# ── redact_url_credentials ──────────────────────────────────────────────
add("redact_url_credentials", text="postgres://u:p@h/db", code_file=False, io={
    "db_connstr_sub": R("postgres://u:***@h/db"), "db_url_query_passwords_sub": R("X"),
    "webhook_url_sub": R("X"), "url_bare_token_sub": R("X"), "userinfo_passwords_sub": R("X")})
add("redact_url_credentials", text="hello", code_file=False, io={
    "db_connstr_sub": R("hello"), "db_url_query_passwords_sub": R("hello"),
    "webhook_url_sub": R("hello"), "url_bare_token_sub": R("hello"), "userinfo_passwords_sub": R("hello")})
add("redact_url_credentials", text="", code_file=False, io={
    "db_connstr_sub": R(""), "db_url_query_passwords_sub": R(""),
    "webhook_url_sub": R(""), "url_bare_token_sub": R(""), "userinfo_passwords_sub": R("")})
add("redact_url_credentials", text="x", code_file=True, file_read=True, io={
    "db_connstr_sub": R("x"), "db_url_query_passwords_sub": R("x"),
    "webhook_url_sub": R("x"), "url_bare_token_sub": R("x"), "userinfo_passwords_sub": R("x")})
add("redact_url_credentials", text="y", code_file=False, io={"db_connstr_sub": X("ValueError", "bad")})

# ── mask_network_path_userinfo ──────────────────────────────────────────
add("mask_network_path_userinfo", m=M(g0="//u:p@h", g={"1": "//", "2": "u", "3": "p"}),
    io={"is_userinfo_placeholder": R(False)})
add("mask_network_path_userinfo", m=M(g0="//u:{pw}@h", g={"1": "//", "2": "u", "3": "{pw}"}),
    io={"is_userinfo_placeholder": R(True)})
add("mask_network_path_userinfo", m=M(g0="//u:password@h", g={"1": "//", "2": "u", "3": "password"}),
    io={"is_userinfo_placeholder": R(True)})
add("mask_network_path_userinfo", m=M(g0="//u:x@h", g={"1": "//", "2": "u", "3": "x"}),
    io={"is_userinfo_placeholder": R(False)})
add("mask_network_path_userinfo", m=M(g0="//u:y@h", g={"1": "//", "2": "u", "3": "y"}),
    io={"is_userinfo_placeholder": R(False)})

# ── redact_phone ────────────────────────────────────────────────────────
for ph in ("12345678", "123456789", "1234567890", "123", "1234567890123"):
    add("redact_phone", m=M(g0=ph, g={"1": ph}))

# ── redact_credential_shapes ───────────────────────────────────────────
add("redact_credential_shapes", text="hello", io={
    "private_key_blocks_sub": R("hello"), "has_known_prefix_substring": R(False),
    "redact_assignments_sub": R("hello"), "cli_secrets_sub": R("hello"),
    "auth_gate_search": R(None), "kv_connstr_gate_search": R(None)})
add("redact_credential_shapes", text="a.b", force=True, io={
    "private_key_blocks_sub": R("a.b"), "has_known_prefix_substring": R(False),
    "zhipu_api_key_sub": R("a.b"), "discord_tokens_sub": R("a.b"),
    "redact_assignments_sub": R("a.b"), "cli_secrets_sub": R("a.b"),
    "auth_gate_search": R(None), "kv_connstr_gate_search": R(None)})
add("redact_credential_shapes", text="--password=secret", force=True, code_file=True, io={
    "private_key_blocks_sub": R("--password=secret"), "has_known_prefix_substring": R(False),
    "cli_secrets_sub": R("--password=***"), "basic_pair_assign_sub": R("X"),
    "auth_gate_search": R(None), "kv_connstr_gate_search": R(None)})
add("redact_credential_shapes", text="eyJtoken", io={
    "private_key_blocks_sub": R("eyJtoken"), "has_known_prefix_substring": R(False),
    "redact_assignments_sub": R("eyJtoken"), "cli_secrets_sub": R("eyJtoken"),
    "auth_gate_search": R(None), "kv_connstr_gate_search": R(None), "jwt_sub": R("eyJ***")})
add("redact_credential_shapes", text="user:pass@h", secret_file=True, io={
    "private_key_blocks_sub": R("user:pass@h"), "has_known_prefix_substring": R(False),
    "credential_store_lines_sub": R("user:pass@h"), "aws_credential_process_keys": R(None),
    "redact_assignments_sub": R("X"), "cli_secrets_sub": R("X"),
    "auth_gate_search": R(None), "header_credentials_sub": R("X"), "telegram_sub": R("X"),
    "kv_connstr_gate_search": R(None)})

# ── mask_env_dump_line ──────────────────────────────────────────────────
add("mask_env_dump_line", m=M(g0="DB_PASSWORD=secret", g={"1": "", "2": "DB_PASSWORD", "3": "secret"}),
    io={"should_redact_assignment": R(True), "mask": R("***")})
add("mask_env_dump_line", m=M(g0="NAME=abc", g={"1": "", "2": "NAME", "3": "abc"}),
    io={"should_redact_assignment": R(False), "env_assign_sub": R("NAME=abc")})
add("mask_env_dump_line", m=M(g0="K=\"a b\"", g={"1": "", "2": "K", "3": "\"a b\""}),
    io={"should_redact_assignment": R(True), "mask": R("***")})
add("mask_env_dump_line", m=M(g0="export TOKEN=x", g={"1": "export ", "2": "TOKEN", "3": "x"}),
    io={"should_redact_assignment": R(True), "mask": R("M")})
add("mask_env_dump_line", m=M(g0="K=v", g={"1": "", "2": "K", "3": "v"}),
    io={"should_redact_assignment": X("ValueError", "bad")})

# ── plugin_patterns ─────────────────────────────────────────────────────
add("plugin_patterns", io={"plugin_patterns": L(["a", "b"])})
add("plugin_patterns", io={"plugin_patterns": L([])})
add("plugin_patterns", io={"plugin_patterns": L(["solo"])})
add("plugin_patterns", io={"plugin_patterns": L(["x", "y", "z"])})
add("plugin_patterns", io={"plugin_patterns": L(["ghp_[A-Za-z0-9]+"])})

# ── rebuild_prefix_matcher ──────────────────────────────────────────────
add("rebuild_prefix_matcher", patterns=["ghp_[A-Za-z0-9]+"], io={
    "plugin_patterns": L([]), "compile_prefix_matcher": R(None),
    "compile_prefix_sub_matcher": R(None), "extract_literal_prefix": R("ghp_")})
add("rebuild_prefix_matcher", patterns=["a", "b"], io={
    "plugin_patterns": L(["c"]), "compile_prefix_matcher": R(None),
    "compile_prefix_sub_matcher": R(None),
    "extract_literal_prefix": [R("a"), R("b"), R("c")]})
add("rebuild_prefix_matcher", patterns=[], io={
    "plugin_patterns": L([]), "compile_prefix_matcher": R(None), "compile_prefix_sub_matcher": R(None)})
add("rebuild_prefix_matcher", patterns=[], io={
    "plugin_patterns": L(["x", "y"]), "compile_prefix_matcher": R(None),
    "compile_prefix_sub_matcher": R(None), "extract_literal_prefix": [R("x"), R("y")]})
add("rebuild_prefix_matcher", patterns=["solo"], io={
    "plugin_patterns": L([]), "compile_prefix_matcher": X("ValueError", "bad"),
    "compile_prefix_sub_matcher": R(None), "extract_literal_prefix": R("s")})

# ── register_redaction_patterns ─────────────────────────────────────────
add("register_redaction_patterns", patterns=[], io={})
add("register_redaction_patterns", patterns=["ghp_[A-Za-z0-9]+"], io={
    "compile": R("RX"), "pattern_reject": R(""), "is_duplicate": R(False),
    "registry_lock_enter": R(None), "registry_extend": R(None),
    "rebuild_prefix_matcher": R(None), "registry_lock_exit": R(None), "log_info_registered": R(None)})
add("register_redaction_patterns", patterns=[""], io={"log_warning_empty": R(None)})
add("register_redaction_patterns", patterns=["bad("], io={
    "compile": X("re_error", "bad"), "log_warning_invalid": R(None)})
add("register_redaction_patterns", patterns=["dup", "dup"], io={
    "compile": [R("RX"), R("RX")], "pattern_reject": [R(""), R("")],
    "is_duplicate": [R(False), R(True)], "log_debug_dup": R(None),
    "registry_lock_enter": R(None), "registry_extend": R(None),
    "rebuild_prefix_matcher": R(None), "registry_lock_exit": R(None), "log_info_registered": R(None)})

# ── reset_plugin_redaction_patterns ────────────────────────────────────
for _ in range(5):
    add("reset_plugin_redaction_patterns", io={
        "registry_lock_enter": R(None), "registry_clear": R(None),
        "rebuild_prefix_matcher": R(None), "registry_lock_exit": R(None)})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs; min/func={min(counts.values())}")
    for k in sorted(counts):
        print(f"  {k:42s} {counts[k]}")


if __name__ == "__main__":
    main()
