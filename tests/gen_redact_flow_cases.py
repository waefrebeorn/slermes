#!/usr/bin/env python3
"""Generate tests/fixtures/redact_flow.jsonl for the redact_flow decision-sequence
oracle. Each case is {"func", "in": {...args, "io": {seam: {"ret": v} | {"raise": ...}
| [queue]}}}. Scripts drive the intended decision branch; the oracle derives the
exact ordered trace + ret + state from the LIVE defs. >=5 cases per ported func.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "redact_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def R(v):
    return {"ret": v}


def raise_(t, m=""):
    return {"raise": {"type": t, "message": m}}


# ── should_redact_assignment ──────────────────────────────────────────────
add("should_redact_assignment", key="API_KEY", value="os.getenv('X')", check_keyword=True,
    io={"env_lookup_match": R(True)})
add("should_redact_assignment", key="API_KEY", value="***", check_keyword=True,
    io={"env_lookup_match": R(False)})
add("should_redact_assignment", key="API_KEY", value="\u00abredacted:ghp_\u2026\u00bb", check_keyword=True,
    io={"env_lookup_match": R(False)})
add("should_redact_assignment", key="API_KEY", value="TRUE", check_keyword=True,
    io={"env_lookup_match": R(False)})
add("should_redact_assignment", key="NAME", value="abcdef", check_keyword=True,
    io={"env_lookup_match": R(False), "key_has_secret_keyword": R(False)})
add("should_redact_assignment", key="DB_PASSWORD", value="hunter2", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(True),
        "path_or_var_match": R(False)})
add("should_redact_assignment", key="DB_PASSWORD", value="$HOME/x", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(True), "path_or_var_match": R(True)})
add("should_redact_assignment", key="SSH_AUTH_SOCK", value="/home/u/.docker", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(True),
        "opaque_path_segment_fullmatch": [R(False), R(False), R(False)]})
add("should_redact_assignment", key="SECRET", value="/wJalrXUtnFEMIK7MDENG/bPxRf", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(True),
        "opaque_path_segment_fullmatch": [R(False), R(True)],
        "has_word_bounded_keyword_strong": R(False), "is_glued_credential_env": R(False),
        "is_qualified_key_name": R(False), "looks_like_opaque_credential": R(True)})
add("should_redact_assignment", key="API_TOKEN", value="short", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(False),
        "has_word_bounded_keyword_strong": R(True)})
add("should_redact_assignment", key="PGPASSWORD", value="x", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(False),
        "has_word_bounded_keyword_strong": R(False), "is_glued_credential_env": R(True)})
add("should_redact_assignment", key="signing_key", value="abc", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(False),
        "has_word_bounded_keyword_strong": R(False), "is_glued_credential_env": R(False),
        "is_qualified_key_name": R(True), "config_placeholder_fullmatch": R(False)})
add("should_redact_assignment", key="signing_key", value="<your key>", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(False),
        "has_word_bounded_keyword_strong": R(False), "is_glued_credential_env": R(False),
        "is_qualified_key_name": R(True), "config_placeholder_fullmatch": R(True),
        "looks_like_opaque_credential": R(False)})
add("should_redact_assignment", key="token", value="abcdef1234567890ABCDEF", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(False),
        "has_word_bounded_keyword_strong": R(False), "is_glued_credential_env": R(False),
        "is_qualified_key_name": R(False), "looks_like_opaque_credential": R(True)})
add("should_redact_assignment", key="token", value="CPU", check_keyword=False,
    io={"env_lookup_match": R(False), "has_word_bounded_keyword_password": R(False),
        "has_password_keyword": R(False), "path_or_var_match": R(False),
        "has_word_bounded_keyword_strong": R(False), "is_glued_credential_env": R(False),
        "is_qualified_key_name": R(False), "looks_like_opaque_credential": R(False)})
add("should_redact_assignment", key="API_KEY", value="x", check_keyword=True,
    io={"env_lookup_match": raise_("ValueError", "boom")})

# ── json_affixed_key_secret ───────────────────────────────────────────────
add("json_affixed_key_secret", key="api_key", value="v",
    io={"json_key_names_full_fullmatch": R(True)})
add("json_affixed_key_secret", key="foo", value="v",
    io={"json_key_names_full_fullmatch": R(False), "has_word_bounded_keyword_yaml": R(False),
        "has_password_keyword": R(False)})
add("json_affixed_key_secret", key="passwordChanged", value="v",
    io={"json_key_names_full_fullmatch": R(False), "has_word_bounded_keyword_yaml": R(True),
        "dsn_pw_placeholder_fullmatch": R(True)})
add("json_affixed_key_secret", key="passwordChanged", value="v",
    io={"json_key_names_full_fullmatch": R(False), "has_word_bounded_keyword_yaml": R(True),
        "dsn_pw_placeholder_fullmatch": R(False), "should_redact_assignment": R(False)})
add("json_affixed_key_secret", key="passwordValue", value="s3cret",
    io={"json_key_names_full_fullmatch": R(False), "has_word_bounded_keyword_yaml": R(False),
        "has_password_keyword": R(True), "dsn_pw_placeholder_fullmatch": R(False),
        "should_redact_assignment": R(True), "json_value_suffix_sub": R("password"),
        "is_python_repr_secret_key": R(True)})
add("json_affixed_key_secret", key="secret_id", value="abc123",
    io={"json_key_names_full_fullmatch": R(False), "has_word_bounded_keyword_yaml": R(True),
        "dsn_pw_placeholder_fullmatch": R(False), "should_redact_assignment": R(True),
        "json_value_suffix_sub": R("secret_id"), "is_python_repr_secret_key": R(False)})

# ── is_sensitive_url_param ────────────────────────────────────────────────
add("is_sensitive_url_param", name="token",
    io={"canonical_url_param_name": R("token")})
add("is_sensitive_url_param", name="accessToken",
    io={"canonical_url_param_name": R("accesstoken"), "decode_url_param_name": R("accessToken"),
        "json_key_names_full_fullmatch": R(True)})
add("is_sensitive_url_param", name="api_key_value",
    io={"canonical_url_param_name": R("api_key_value"), "decode_url_param_name": R("api_key_value"),
        "json_key_names_full_fullmatch": R(False), "json_value_suffix_sub": R("api_key"),
        "is_python_repr_secret_key": R(True)})
add("is_sensitive_url_param", name="page",
    io={"canonical_url_param_name": R("page"), "decode_url_param_name": R("page"),
        "json_key_names_full_fullmatch": R(False), "json_value_suffix_sub": R("page"),
        "is_python_repr_secret_key": R(False)})
add("is_sensitive_url_param", name="x-amz-signature",
    io={"canonical_url_param_name": R("x-amz-signature")})
add("is_sensitive_url_param", name="private_token",
    io={"canonical_url_param_name": R("private_token"), "decode_url_param_name": R("private_token"),
        "json_key_names_full_fullmatch": R(False), "json_value_suffix_sub": R("private_token"),
        "is_python_repr_secret_key": R(True)})

# ── redact_log_urls ───────────────────────────────────────────────────────
add("redact_log_urls", text="")
add("redact_log_urls", text=None)
add("redact_log_urls", text="plain text")
add("redact_log_urls", text="https://x?token=abc", io={"redact_enabled": R(False)})
add("redact_log_urls", text="https://x?token=abc",
    io={"redact_enabled": R(True), "redact_strict_url_credentials": R("https://x?token=***")})
add("redact_log_urls", text="https://x", io={"redact_enabled": raise_("RuntimeError", "x")})

# ── redact_for_egress ─────────────────────────────────────────────────────
add("redact_for_egress", text="hello", io={"redact_sensitive_text": R("hello")})
add("redact_for_egress", text=None, io={"redact_sensitive_text": R("")})
add("redact_for_egress", text="Bearer abc",
    io={"redact_sensitive_text": R("Bearer abc"), "bearer_residue_sub": R("Bearer [redacted]")})
add("redact_for_egress", text="the bearer of news",
    io={"redact_sensitive_text": R("the bearer of news")})
add("redact_for_egress", text="x", urls=True, keep_phone_numbers=True,
    io={"redact_sensitive_text": R("x")})
add("redact_for_egress", text="x", io={"redact_sensitive_text": raise_("ValueError", "boom")})
add("redact_for_egress", text="x", io={"redact_sensitive_text": raise_("SystemExit", "bye")})
add("redact_for_egress", text="x", io={"redact_sensitive_text": raise_("KeyboardInterrupt", "ki")})

# ── redact_code_output ────────────────────────────────────────────────────
add("redact_code_output", output="MAX_TOKENS=100", force=False,
    io={"redact_sensitive_text": R("MAX_TOKENS=100"), "redact_enabled": R(True),
        "env_dump_line_sub": R("MAX_TOKENS=100")})
add("redact_code_output", output="", force=False, io={"redact_sensitive_text": R("")})
add("redact_code_output", output="no equals", force=False,
    io={"redact_sensitive_text": R("no equals")})
add("redact_code_output", output="A=1", force=False,
    io={"redact_sensitive_text": R("A=1"), "redact_enabled": R(False)})
add("redact_code_output", output="DB_PASSWORD=x", force=True,
    io={"redact_sensitive_text": R("DB_PASSWORD=x"), "env_dump_line_sub": R("DB_PASSWORD=***")})
add("redact_code_output", output="A=1", force=False,
    io={"redact_sensitive_text": raise_("ValueError", "boom")})

# ── redact_derived_text ───────────────────────────────────────────────────
add("redact_derived_text", text=None, io={"redact_sensitive_text": R("")})
add("redact_derived_text", text="secret", io={"redact_sensitive_text": R("***")})
add("redact_derived_text", text=123, io={"redact_sensitive_text": R("123")})
add("redact_derived_text", text=True, io={"redact_sensitive_text": R("True")})
add("redact_derived_text", text="x", io={"redact_sensitive_text": raise_("SystemExit", "s")})

# ── redact_enabled ────────────────────────────────────────────────────────
add("redact_enabled", state={"_REDACT_ENABLED": False},
    io={"get_hermes_home_override": R(None)})
add("redact_enabled", state={"_REDACT_ENABLED": True},
    io={"get_hermes_home_override": R(None)})
add("redact_enabled", state={"_REDACT_ENABLED": True},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R({"HERMES_REDACT_SECRETS": "false"})})
add("redact_enabled", state={"_REDACT_ENABLED": True},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R({"HERMES_REDACT_SECRETS": "YES"})})
add("redact_enabled", state={"_ENV_POLICY_BY_HOME": {"h1": "1"}},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R(None)})
add("redact_enabled", state={"_ENV_POLICY_BY_HOME": {"h1": None}, "_CONFIG_POLICY_BY_HOME": {"h1": "off"}},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R(None)})
add("redact_enabled", state={"_ENV_POLICY_BY_HOME": {}},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R(None)})
add("redact_enabled", state={"_ENV_POLICY_BY_HOME": {"h1": None}, "_CONFIG_POLICY_BY_HOME": {}},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R({})})
add("redact_enabled", state={"_REDACT_ENABLED": True},
    io={"get_hermes_home_override": R("/h"), "hermes_home_key": R("h1"),
        "current_secret_scope": R({"OTHER": "x"})})

# ── command_reads_secret_file ─────────────────────────────────────────────
add("command_reads_secret_file", command=None)
add("command_reads_secret_file", command="")
add("command_reads_secret_file", command="cat .env",
    io={"command_segments": R(["cat .env"]), "is_secret_file_arg": R(True)})
add("command_reads_secret_file", command="echo hi",
    io={"command_segments": R(["echo hi"])})
add("command_reads_secret_file", command="grep .bashrc app.py",
    io={"command_segments": R(["grep .bashrc app.py"]), "is_secret_file_arg": [R(False), R(True)]})
add("command_reads_secret_file", command="Get-Content -Path:.netrc",
    io={"command_segments": R(["Get-Content -Path:.netrc"]),
        "ps_colon_param_fullmatch": R(".netrc"), "is_secret_file_arg": R(True)})
add("command_reads_secret_file", command="grep -Path:.env x",
    io={"command_segments": R(["grep -Path:.env x"])})
add("command_reads_secret_file", command="cat a.txt; cat .env",
    io={"command_segments": R(["cat a.txt", "cat .env"]),
        "is_secret_file_arg": [R(False), R(True)]})
add("command_reads_secret_file", command="cat .env",
    io={"command_segments": raise_("ValueError", "seg")})

# ── is_secret_file_arg ────────────────────────────────────────────────────
add("is_secret_file_arg", arg=".env")
add("is_secret_file_arg", arg="'/home/u/.bashrc'")
add("is_secret_file_arg", arg="C:\\Users\\u\\.netrc")
add("is_secret_file_arg", arg="/home/u/.aws/credentials")
add("is_secret_file_arg", arg="/home/u/.aws/config")
add("is_secret_file_arg", arg="main.py")
add("is_secret_file_arg", arg="$HERMES_HOME/config.yaml")
add("is_secret_file_arg", arg="${HOME}/.hermes/config.yaml")
add("is_secret_file_arg", arg="/x/config.yaml", io={"is_under_hermes_home": R(True)})
add("is_secret_file_arg", arg="/x/config.yaml", io={"is_under_hermes_home": R(False)})
add("is_secret_file_arg", arg="$HOME/$VAR/config.yaml")
add("is_secret_file_arg", arg="/x/config.yaml.good.123", io={"is_under_hermes_home": R(True)})
add("is_secret_file_arg", arg="''")

# ── note_env_policy / note_config_policy ──────────────────────────────────
add("note_env_policy", home_key="h1", raw="false", state={"_ENV_POLICY_BY_HOME": {}})
add("note_env_policy", home_key="h1", raw=None, state={"_ENV_POLICY_BY_HOME": {"h1": "x"}})
add("note_env_policy", home_key="h2", raw=True, state={"_ENV_POLICY_BY_HOME": {"h1": "1"}})
add("note_env_policy", home_key="h1", raw=0, state={"_ENV_POLICY_BY_HOME": {}})
add("note_env_policy", home_key="h1", raw="on", state={"_ENV_POLICY_BY_HOME": {"h1": "off"}})
add("note_config_policy", home_key="h1", raw="false", state={"_CONFIG_POLICY_BY_HOME": {}})
add("note_config_policy", home_key="h1", raw=None, state={"_CONFIG_POLICY_BY_HOME": {"h1": "x"}})
add("note_config_policy", home_key="h2", raw=False, state={"_CONFIG_POLICY_BY_HOME": {}})
add("note_config_policy", home_key="h1", raw="yes", state={"_CONFIG_POLICY_BY_HOME": {"h1": "no"}})
add("note_config_policy", home_key="h1", raw=1, state={"_CONFIG_POLICY_BY_HOME": {}})

# ── key_name_words ────────────────────────────────────────────────────────
add("key_name_words", key="awsSecretAccessKey")
add("key_name_words", key="aws_secret_access_key")
add("key_name_words", key="APIToken")
add("key_name_words", key="client-key.data")
add("key_name_words", key="")
add("key_name_words", key="ABC")
add("key_name_words", key="key")

# ── is_qualified_key_name ─────────────────────────────────────────────────
add("is_qualified_key_name", key="aws_secret_access_key")
add("is_qualified_key_name", key="privateKey")
add("is_qualified_key_name", key="public_key")
add("is_qualified_key_name", key="key")
add("is_qualified_key_name", key="hotkey")
add("is_qualified_key_name", key="master_key")
add("is_qualified_key_name", key="key_id")

# ── is_python_repr_secret_key ─────────────────────────────────────────────
add("is_python_repr_secret_key", key="API_KEY")
add("is_python_repr_secret_key", key="UserPassword")
add("is_python_repr_secret_key", key="token_count")
add("is_python_repr_secret_key", key="privateKey")
add("is_python_repr_secret_key", key="password_policy")
add("is_python_repr_secret_key", key="credential")
add("is_python_repr_secret_key", key="SECRET_NAME")

# ── is_secret_field_name ──────────────────────────────────────────────────
add("is_secret_field_name", key="token")
add("is_secret_field_name", key="max_tokens")
add("is_secret_field_name", key=123)
add("is_secret_field_name", key=None)
add("is_secret_field_name", key="signing_key")
add("is_secret_field_name", key=["token"])

# ── decode_url_param_name ─────────────────────────────────────────────────
add("decode_url_param_name", name="token")
add("decode_url_param_name", name="a%2Db")
add("decode_url_param_name", name="a+b")
add("decode_url_param_name", name="%2520")
add("decode_url_param_name", name="caf%C3%A9")
add("decode_url_param_name", name="%ZZ")
add("decode_url_param_name", name="x%2")

# ── canonical_url_param_name ──────────────────────────────────────────────
add("canonical_url_param_name", name="X-Amz-Signature")
add("canonical_url_param_name", name="AccessToken")
add("canonical_url_param_name", name="api-key")
add("canonical_url_param_name", name="TOKEN")
add("canonical_url_param_name", name="x-goog-signature")
add("canonical_url_param_name", name="usertoken")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print(f"wrote {len(CASES)} cases to {OUT}")
for fn, n in sorted(cnt.items()):
    print(f"  {fn}: {n}")
assert all(n >= 5 for n in cnt.values()), "every func needs >=5 cases"
assert len(cnt) == 18, f"expected 18 funcs, got {len(cnt)}"
