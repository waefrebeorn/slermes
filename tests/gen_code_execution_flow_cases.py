#!/usr/bin/env python3
"""Generate tests/fixtures/code_execution_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII text inputs (one UTF-8 base64 content case locks byte handling);
no integral-float JSON literals in str/repr/json.dumps float positions
(duration_seconds kept NON-integral); no non-finite floats. Set returns are
scripted as lists; tuple returns (_clean_output / truncate_stdout_text) as a
2-element list nested one level deep (a bare array is a call queue).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "code_execution_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


RAISE_EXC = {"type": "RuntimeError", "message": "boom"}
RAISE_VAL = {"type": "ValueError", "message": "bad value"}
RAISE_KBI = {"type": "KeyboardInterrupt", "message": "interrupted"}
RAISE_SYS = {"type": "SystemExit", "message": "exit"}

ALL_TOOLS = ["web_search", "web_extract", "read_file", "write_file", "search_files", "patch", "terminal"]

# ── is_secret_env_name ─────────────────────────────────────────────────────
for nm in ["MY_TOKEN", "PATH", "OPENAI_API_KEY", "HOME", "my_password", "BYPASS_CACHE",
           "HERMES_WEBHOOK_URL", "", "AUTH_HEADER", "COMPASS_DIR", "AWS_SECRET_ACCESS_KEY",
           "LANG", "GITHUB_CREDS", "DSN_URL"]:
    add("is_secret_env_name", name=nm)

# ── _cache_probe_result ────────────────────────────────────────────────────
add("_cache_probe_result", state={"cache": {}}, key="p1", value=True)
add("_cache_probe_result", state={"cache": {f"k{i}": i for i in range(31)}}, key="new", value=False)
add("_cache_probe_result", state={"cache": {f"k{i}": i for i in range(32)}}, key="new", value="v")
add("_cache_probe_result", state={"cache": {f"k{i}": i for i in range(32)}}, key="k0", value="updated")
add("_cache_probe_result", state={"cache": {"a": 1}}, key="b", value=None)
add("_cache_probe_result", state={"cache": {f"k{i}": i for i in range(33)}}, key="new", value=1)
add("_cache_probe_result", state={}, key="solo", value={"x": 1})

# ── _sandbox_tools_for ─────────────────────────────────────────────────────
add("_sandbox_tools_for", enabled_tools=None)
add("_sandbox_tools_for", enabled_tools=[])
add("_sandbox_tools_for", enabled_tools=["read_file", "terminal"])
add("_sandbox_tools_for", enabled_tools=["read_file", "bogus"])
add("_sandbox_tools_for", enabled_tools=ALL_TOOLS)
add("_sandbox_tools_for", enabled_tools=["bogus"])
add("_sandbox_tools_for", enabled_tools=["read_file", "read_file", "patch"])

# ── _missing_hermes_tools_import_hint ──────────────────────────────────────
add("_missing_hermes_tools_import_hint", m={"group1": "json_parse"}, enabled_tools=None)
add("_missing_hermes_tools_import_hint", m={"group1": "shell_quote"}, enabled_tools=[])
add("_missing_hermes_tools_import_hint", m={"group1": "retry"}, enabled_tools=["read_file"])
add("_missing_hermes_tools_import_hint", m={"group1": "bogus"}, enabled_tools=None,
    io={"sandbox_tools_for": [ALL_TOOLS]})
add("_missing_hermes_tools_import_hint", m={"group1": "bogus"}, enabled_tools=[],
    io={"sandbox_tools_for": [[]]})
add("_missing_hermes_tools_import_hint", m={"group1": "read_file"}, enabled_tools=["read_file", "terminal"],
    io={"sandbox_tools_for": [["terminal", "read_file"]]})
add("_missing_hermes_tools_import_hint", m={"group1": "bogus"}, enabled_tools=None,
    io={"sandbox_tools_for": script(raise_spec=RAISE_EXC)})
add("_missing_hermes_tools_import_hint", m={"group1": "x"}, enabled_tools=None, io={})

# ── json_parse ─────────────────────────────────────────────────────────────
add("json_parse", text='{"a":1}', io={"json_loads": {"a": 1}})
add("json_parse", text='\ufeff{"a":1}', io={"json_loads": {"a": 1}})
add("json_parse", text=None, io={"json_loads": None})
add("json_parse", text=123, io={"json_loads": {"x": 1}})
add("json_parse", text="bad", io={"json_loads": script(raise_spec=RAISE_VAL)})
add("json_parse", text="x", io={"json_loads": script(raise_spec=RAISE_KBI)})
add("json_parse", text="x", io={})
add("json_parse", text='\ufeff', io={"json_loads": ""})

# ── shell_quote ────────────────────────────────────────────────────────────
for s in ["", "abc", "a b", "a'b", "/path/to", "a$b", 'say "hi"', "a=b", "a\tb", "ABC_123", "a|b"]:
    add("shell_quote", s=s)

# ── retry ──────────────────────────────────────────────────────────────────
add("retry", max_attempts=3, delay=2, io={"fn": [42]})
add("retry", max_attempts=3, delay=2,
    io={"fn": [script(raise_spec=RAISE_VAL), 42], "time.sleep": [None]})
add("retry", max_attempts=3, delay=2,
    io={"fn": [script(raise_spec=RAISE_VAL), script(raise_spec=RAISE_VAL), script(raise_spec=RAISE_VAL)],
        "time.sleep": [None, None]})
add("retry", max_attempts=3, delay=2, io={"fn": [script(raise_spec=RAISE_KBI)]})
add("retry", max_attempts=3, delay=2, io={"fn": [script(raise_spec=RAISE_SYS)]})
add("retry", max_attempts=1, delay=2, io={"fn": [script(raise_spec=RAISE_VAL)]})
add("retry", max_attempts=0, delay=2, io={})
add("retry", max_attempts=3, delay=0.5,
    io={"fn": [script(raise_spec=RAISE_VAL), "ok"], "time.sleep": [None]})
add("retry", max_attempts=3, delay=2,
    io={"fn": [script(raise_spec=RAISE_VAL), script(raise_spec=RAISE_EXC)], "time.sleep": [None]})
add("retry", max_attempts=3, delay=2, io={})
add("retry", max_attempts=2, delay=1,
    io={"fn": [script(raise_spec=RAISE_VAL), script(raise_spec=RAISE_VAL)], "time.sleep": [None]})

# ── _sanitize_output_text ──────────────────────────────────────────────────
add("_sanitize_output_text", text="x", io={"strip_ansi": "s", "redact_code_output": "r"})
add("_sanitize_output_text", text="", io={"strip_ansi": "", "redact_code_output": ""})
add("_sanitize_output_text", text="x", io={"strip_ansi": script(raise_spec=RAISE_EXC)})
add("_sanitize_output_text", text="x", io={"strip_ansi": "s", "redact_code_output": script(raise_spec=RAISE_EXC)})
add("_sanitize_output_text", text="x", io={"strip_ansi": script(raise_spec=RAISE_KBI)})
add("_sanitize_output_text", text="x", io={})
add("_sanitize_output_text", text="abc", io={"strip_ansi": "ABC", "redact_code_output": "REDACTED"})

# ── _clean_output ──────────────────────────────────────────────────────────
add("_clean_output", stdout_text="raw", env=None,
    io={"sanitize_output_text": "san", "truncate_stdout_text": [["clean", {"stdout_truncated": False}]]})
add("_clean_output", stdout_text="raw", env={"k": 1},
    io={"sanitize_output_text": "san", "truncate_stdout_text": [["clean", {"stdout_truncated": True, "warning": "w"}]]})
add("_clean_output", stdout_text="raw", env=None, io={"sanitize_output_text": script(raise_spec=RAISE_EXC)})
add("_clean_output", stdout_text="raw", env=None,
    io={"sanitize_output_text": "san", "truncate_stdout_text": script(raise_spec=RAISE_EXC)})
add("_clean_output", stdout_text="", env=None,
    io={"sanitize_output_text": "", "truncate_stdout_text": [["", {"stdout_bytes_total": 0}]]})
add("_clean_output", stdout_text="raw", env=None, io={})
add("_clean_output", stdout_text="raw", env=None,
    io={"sanitize_output_text": "san", "truncate_stdout_text": script(raise_spec=RAISE_KBI)})

# ── _with_timeout_notice ───────────────────────────────────────────────────
add("_with_timeout_notice", stdout_text="out", timeout_msg="T")
add("_with_timeout_notice", stdout_text="", timeout_msg="T")
add("_with_timeout_notice", stdout_text="line1\nline2", timeout_msg="msg")
add("_with_timeout_notice", stdout_text="out", timeout_msg="")
add("_with_timeout_notice", stdout_text="", timeout_msg="")
add("_with_timeout_notice", stdout_text="x", timeout_msg="timed out after 300s")

# ── _error_result ──────────────────────────────────────────────────────────
add("_error_result", error="boom", tool_calls_made=0, duration=1.25)
add("_error_result", error="x", tool_calls_made=5, duration=0.1, user_summary="s")
add("_error_result", error="x", tool_calls_made=2, duration=2.5, user_summary=None)
add("_error_result", error="x", tool_calls_made=0, duration=0.33, user_summary="")
add("_error_result", error='q"uote', tool_calls_made=1, duration=1.5)
add("_error_result", error="x", tool_calls_made=0, duration=0.1, user_summary="sum")
add("_error_result", error="back\\slash", tool_calls_made=3, duration=10.25)

# ── _remote_failure ────────────────────────────────────────────────────────
add("_remote_failure", exc={"__raise__": "RuntimeError", "message": "boom"}, exec_start=0, tool_calls_made=0,
    io={"time.monotonic": 1.25, "logger.error": None, "error_result": "ERRSTR"})
add("_remote_failure", exc={"__raise__": "ValueError", "message": "bad"}, exec_start=1.0, tool_calls_made=3,
    io={"time.monotonic": 3.5, "logger.error": None, "error_result": "ERR2"})
add("_remote_failure", exc={"__raise__": "RuntimeError", "message": "boom"}, exec_start=0, tool_calls_made=0,
    io={"time.monotonic": script(raise_spec=RAISE_EXC)})
add("_remote_failure", exc={"__raise__": "RuntimeError", "message": "boom"}, exec_start=0, tool_calls_made=0,
    io={"time.monotonic": 1.25, "logger.error": script(raise_spec=RAISE_EXC)})
add("_remote_failure", exc={"__raise__": "RuntimeError", "message": "boom"}, exec_start=0, tool_calls_made=0,
    io={"time.monotonic": 1.25, "logger.error": None, "error_result": script(raise_spec=RAISE_EXC)})
add("_remote_failure", exc={"__raise__": "KeyboardInterrupt", "message": "ki"}, exec_start=0, tool_calls_made=2,
    io={"time.monotonic": 2.5, "logger.error": None, "error_result": "ERRKI"})
add("_remote_failure", exc={"__raise__": "RuntimeError", "message": "boom"}, exec_start=0, tool_calls_made=0, io={})

# ── _remote_result ─────────────────────────────────────────────────────────
add("_remote_result", status="success", raw_stdout="out", exec_start=0, fields={"exit_code": 0}, kernel=None, env=None,
    io={"clean_output": [["clean", {"stdout_truncated": False}]], "time.monotonic": 1.25})
add("_remote_result", status="success", raw_stdout="out", exec_start=0, fields={"exit_code": 0}, kernel={"remote": True}, env=None,
    io={"clean_output": [["clean", {"stdout_truncated": False}]], "time.monotonic": 2.5})
add("_remote_result", status="error", raw_stdout="out", exec_start=0, fields={"tool_calls_made": 2, "exit_code": 1}, kernel=None, env=None,
    io={"clean_output": [["clean", {"stdout_truncated": True, "warning": "w"}]], "time.monotonic": 0.5})
add("_remote_result", status="success", raw_stdout="out", exec_start=0, fields={}, kernel=None, env={"k": 1},
    io={"clean_output": [["clean", {}]], "time.monotonic": 1.25})
add("_remote_result", status="success", raw_stdout="out", exec_start=0, fields={}, kernel=None, env=None,
    io={"clean_output": script(raise_spec=RAISE_EXC)})
add("_remote_result", status="success", raw_stdout="out", exec_start=0, fields={}, kernel=None, env=None,
    io={"clean_output": [["clean", {}]], "time.monotonic": script(raise_spec=RAISE_EXC)})
add("_remote_result", status="timeout", raw_stdout="partial", exec_start=1.0, fields={"exit_code": 124}, kernel=None, env=None,
    io={"clean_output": [["partial", {"stdout_truncated": False}]], "time.monotonic": 301.25})

# ── _apply_timeout ─────────────────────────────────────────────────────────
add("_apply_timeout", state={"result": {"output": "out", "status": "timeout"}}, timeout_msg="T",
    io={"with_timeout_notice": "out\n\n\u23f0 T"})
add("_apply_timeout", state={"result": {"output": ""}}, timeout_msg="T",
    io={"with_timeout_notice": "\u23f0 T"})
add("_apply_timeout", state={"result": {"output": "out", "status": "timeout"}}, timeout_msg="T",
    io={"with_timeout_notice": script(raise_spec=RAISE_EXC)})
add("_apply_timeout", state={"result": {"output": None}}, timeout_msg="T",
    io={"with_timeout_notice": "\u23f0 T"})
add("_apply_timeout", state={"result": {"status": "x"}}, timeout_msg="T", io={})
add("_apply_timeout", state={"result": {"output": "out"}}, timeout_msg="T", io={})

# ── _private_dirs_cmd ──────────────────────────────────────────────────────
add("_private_dirs_cmd", root="/tmp/x", subdirs=[])
add("_private_dirs_cmd", root="/tmp/x", subdirs=["/tmp/x/rpc"])
add("_private_dirs_cmd", root="/tmp/a b", subdirs=[])
add("_private_dirs_cmd", root="/tmp/x", subdirs=["/tmp/x/a", "/tmp/x/b"])
add("_private_dirs_cmd", root="rel", subdirs=["rel/sub"])
add("_private_dirs_cmd", root="/tmp/x", subdirs=["/tmp/x/rpc", "/tmp/x/out"])

# ── _execute_checked ───────────────────────────────────────────────────────
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={"env.execute": {"returncode": 0, "output": "ok"}})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={"env.execute": {"returncode": 1, "output": "err"}})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={"env.execute": {"output": "no rc"}})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={"env.execute": {"returncode": 0, "output": None}})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={"env.execute": script(raise_spec=RAISE_EXC)})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={"stdin_data": "x"},
    io={"env.execute": {"returncode": 0, "output": "ok"}})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={"env.execute": {"returncode": 2, "output": "bad"}})
add("_execute_checked", cmd="ls", what="setup", timeout=10, kwargs={}, io={})

# ── _remote_write ──────────────────────────────────────────────────────────
add("_remote_write", remote_path="/tmp/f", content="hi", atomic=False, timeout=30, check=False,
    io={"env.execute": {"returncode": 0}})
add("_remote_write", remote_path="/tmp/f", content="hi", atomic=True, timeout=30, check=False,
    io={"env.execute": {"returncode": 0}})
add("_remote_write", remote_path="/tmp/f", content="hi", atomic=False, timeout=30, check=True,
    io={"execute_checked": {"returncode": 0}})
add("_remote_write", remote_path="/tmp/f", content="hi", atomic=True, timeout=60, check=True,
    io={"execute_checked": script(raise_spec=RAISE_EXC)})
add("_remote_write", remote_path="/tmp/f", content="hi", atomic=False, timeout=30, check=False,
    io={"env.execute": script(raise_spec=RAISE_EXC)})
add("_remote_write", remote_path="/tmp/f", content="", atomic=False, timeout=30, check=False,
    io={"env.execute": {"returncode": 0}})
add("_remote_write", remote_path="/tmp/a b", content="x", atomic=False, timeout=30, check=False,
    io={"env.execute": {"returncode": 0}})
add("_remote_write", remote_path="/tmp/f", content="caf\u00e9", atomic=False, timeout=30, check=False,
    io={"env.execute": {"returncode": 0}})
add("_remote_write", remote_path="/tmp/f", content="hi", atomic=False, timeout=30, check=True, io={})

# ── _rpc_token_ok ──────────────────────────────────────────────────────────
add("_rpc_token_ok", request={"token": "abc"}, rpc_token="abc")
add("_rpc_token_ok", request={"token": "abc"}, rpc_token="xyz")
add("_rpc_token_ok", request={"token": "abc"}, rpc_token="")
add("_rpc_token_ok", request={}, rpc_token="abc")
add("_rpc_token_ok", request={"token": None}, rpc_token="abc")
add("_rpc_token_ok", request={"token": ""}, rpc_token="abc")
add("_rpc_token_ok", request={"token": 123}, rpc_token="123")
add("_rpc_token_ok", request={"token": "abc"}, rpc_token="abc")

# ── _result_error ──────────────────────────────────────────────────────────
add("_result_error", result='{"error":"boom"}', io={"json_loads": {"error": "boom"}})
add("_result_error", result='{"error":""}', io={"json_loads": {"error": ""}})
add("_result_error", result="not json", io={})
add("_result_error", result='{"x":1}', io={})
add("_result_error", result=123, io={})
add("_result_error", result='{"error":"boom"}', io={"json_loads": script(raise_spec=RAISE_VAL)})
add("_result_error", result='{"error":"boom"}', io={"json_loads": script(raise_spec=RAISE_EXC)})
add("_result_error", result='{"error":"boom"}', io={"json_loads": [1, 2]})
add("_result_error", result='{"error":{"a":1}}', io={"json_loads": {"error": {"a": 1}}})
add("_result_error", result='{"error":"' + "z" * 400 + '"}', io={"json_loads": {"error": "z" * 400}})
add("_result_error", result='{"error":"x"}', io={})
add("_result_error", result=None, io={})

# ── tool_errors_since ──────────────────────────────────────────────────────
add("tool_errors_since", tool_call_log=[{"tool": "a", "error": "e1"}, {"tool": "b"}], start=0)
add("tool_errors_since", tool_call_log=[{"tool": "a", "error": "e1"}, {"tool": "b", "error": "e2"}], start=1)
add("tool_errors_since", tool_call_log=[], start=0)
add("tool_errors_since", tool_call_log=[{"tool": f"t{i}", "error": f"e{i}"} for i in range(6)], start=0)
add("tool_errors_since", tool_call_log=[{"tool": "a", "error": ""}], start=0)
add("tool_errors_since", tool_call_log=[{"tool": "a"}], start=0)
add("tool_errors_since", tool_call_log=[{"tool": "a", "error": "e"}], start=2)
add("tool_errors_since", tool_call_log=[{"tool": "a", "error": "e"}, {"tool": "b", "error": ""}, {"tool": "c", "error": "e3"}], start=0)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in cnt.items() if v < 5}
    print(f"Wrote {len(CASES)} cases across {len(cnt)} funcs to {OUT}")
    if short:
        print("WARNING funcs with <5 cases:", short)
    else:
        print("All funcs have >=5 cases.")


if __name__ == "__main__":
    main()