#!/usr/bin/env python3
"""Generate tests/fixtures/file_operations_flow.jsonl — scripted-seam cases for
the file_operations_flow port (>=5 per function). Each case is
{"func":..., "in":{...args..., "seams":{name: script}}}. A script that is a JSON
array is a SEQUENCE (one result per call, last repeats); a bare value is reused.
List/tuple RESULTS are wrapped one level deeper (e.g. split_segments: [[...]]).
bytes travel as lowercase hex; _reject_unencodable content as a codepoint array.
"""
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "file_operations_flow.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def rs(kind, msg):
    return {"__raise__": kind, "msg": msg}


def sha(b):
    return hashlib.sha256(b).hexdigest()


AMB = {"sys_executable": "/usr/bin/python3",
       "linter_exts": [".py", ".json", ".yaml", ".yml", ".toml"],
       "fail_closed_exts": [".json", ".yaml", ".yml", ".toml"]}


# ── init ──
case("init", env={"cwd": "/env"}, cwd="/explicit")
case("init", env={"cwd": "/env"}, cwd=None)
case("init", env={"cwd": None, "config": {"cwd": "/cfg"}}, cwd=None)
case("init", env={}, cwd=None)
case("init", env={"cwd": "", "config": {"cwd": ""}}, cwd="")
case("init", env={"cwd": None, "config": None}, cwd=None)
case("init", env={"config": {"cwd": "/cfg"}}, cwd=None)

# ── _cat ──
case("_cat", path="/f", seams={"escape_shell_arg": "'p'", "exec": {"stdout": "data", "exit_code": 0}})
case("_cat", path="/f", seams={"escape_shell_arg": "'p'", "exec": {"stdout": "err", "exit_code": 1}})
case("_cat", path="/f", seams={"escape_shell_arg": "'p'", "exec": {"stdout": "x", "exit_code": 126, "cwd_error": "working directory '/nope' does not exist"}})
case("_cat", path="/f", seams={"escape_shell_arg": rs("RuntimeError", "boom")})
case("_cat", path="/f", seams={"escape_shell_arg": "'p'", "exec": rs("ValueError", "bad")})
case("_cat", path="/f", seams={"escape_shell_arg": "'p'", "exec": rs("SystemExit", "bye")})
case("_cat", path="/f", seams={})

# ── _head ──
case("_head", path="/f", nbytes=1000, seams={"escape_shell_arg": "'p'", "exec": {"stdout": "abc", "exit_code": 0}})
case("_head", path="/f", nbytes=3, seams={"escape_shell_arg": "'p'", "exec": {"stdout": "ab", "exit_code": 0}})
case("_head", path="/f", nbytes=0, seams={"escape_shell_arg": "'p'", "exec": {"stdout": "", "exit_code": 0}})
case("_head", path="/f", nbytes=5, seams={"escape_shell_arg": rs("RuntimeError", "x")})
case("_head", path="/f", nbytes=5, seams={"escape_shell_arg": "'p'", "exec": rs("KeyboardInterrupt", "")})
case("_head", path="/f", nbytes=5, seams={"escape_shell_arg": "'p'"})

# ── _bounded ──
case("_bounded", encoder="base64", arg="'p'", max_bytes=None)
case("_bounded", encoder="base64", arg="'p'", max_bytes=100)
case("_bounded", encoder="od -An -v -tx1", arg="'p'", max_bytes=0)
case("_bounded", encoder="od -An -v -tx1", arg="'p'", max_bytes=None)
case("_bounded", encoder="base64", arg="'a b'", max_bytes=50)
case("_bounded", encoder="base64", arg="'p'", max_bytes=1)

# ── _failed_read ──
case("_failed_read", path="/f", payload="  diagnostic  ", read_rc=1, seams={"strip_fence": "  diagnostic  "})
case("_failed_read", path="/f", payload="", read_rc=2, seams={"strip_fence": ""})
case("_failed_read", path="/f", payload="   ", read_rc=3, seams={"strip_fence": "   "})
case("_failed_read", path="/f", payload="x", read_rc=1, seams={"strip_fence": rs("RuntimeError", "x")})
case("_failed_read", path="/f", payload="x", read_rc=1, seams={"strip_fence": rs("ValueError", "x")})
case("_failed_read", path="/f", payload="noise__HERMES_FENCE_X__diag", read_rc=1, seams={"strip_fence": "diag"})

# ── _matches_size ──
case("_matches_size", data="6162", size_segment="2", seams={"strip_fence": "2"})
case("_matches_size", data="6162", size_segment="3", seams={"strip_fence": "3"})
case("_matches_size", data="6162", size_segment="2 extra", seams={"strip_fence": "2 extra"})
case("_matches_size", data="6162", size_segment="", seams={"strip_fence": ""})
case("_matches_size", data="6162", size_segment="x", seams={"strip_fence": "x"})
case("_matches_size", data="", size_segment="0", seams={"strip_fence": "0"})
case("_matches_size", data="6162", size_segment="2", seams={"strip_fence": rs("RuntimeError", "x")})

# ── _fenced_read ──
SENT = "__HERMES_RB_abc__"
def fenced(segments, more=None, strip=None, exec_=None, sentinel=SENT):
    s = {"new_sentinel": sentinel,
         "exec": exec_ if exec_ is not None else {"stdout": "raw", "exit_code": 0},
         "split_segments": [segments]}
    if strip is not None:
        s["strip_fence"] = strip
    return s
case("_fenced_read", body="base64 < 'p'", more=["wc -c < 'p'"],
     seams=fenced(["pre", "payload", "size", "0\n"], strip="0\n"))
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams=fenced(["pre", "payload", "size"], strip="0"))
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams=fenced(["pre", "p", "s", "x"], strip="x"))
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams=fenced(["pre", "p", "s", ""], strip=""))
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams={"new_sentinel": SENT, "exec": rs("RuntimeError", "x")})
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams={"new_sentinel": SENT, "exec": {"stdout": "raw", "exit_code": 0}, "split_segments": rs("RuntimeError", "x")})
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams=fenced(["pre", "p", "s", "0"], strip=rs("ValueError", "x")))
case("_fenced_read", body="b", more=["wc -c < 'p'"],
     seams=fenced(["pre", "p", "s", "0"], strip=rs("RuntimeError", "x")))
case("_fenced_read", body="b", more=[],
     seams=fenced(["pre", "p", "0"], strip="0"))
case("_fenced_read", body="b", more=["c1", "c2"],
     seams=fenced(["pre", "p1", "p2", "s", "7"], strip="7"))

# ── _probe_regular_file ──
def probe(exec_, strip, escape="'p'"):
    return {"escape_shell_arg": escape, "exec": exec_, "strip_fence": strip}
case("_probe_regular_file", path="/f", seams=probe({"stdout": "123", "exit_code": 0}, "123"))
case("_probe_regular_file", path="/f", seams=probe({"stdout": "__hermes_missing__", "exit_code": 0}, "__hermes_missing__"))
case("_probe_regular_file", path="/f", seams=probe({"stdout": "__hermes_not_regular__", "exit_code": 0}, "__hermes_not_regular__"))
case("_probe_regular_file", path="/f", seams=probe({"stdout": "", "exit_code": 1}, ""))
case("_probe_regular_file", path="/f", seams=probe({"stdout": "", "exit_code": 126, "cwd_error": "working directory '/nope' does not exist"}, ""))
case("_probe_regular_file", path="/f", seams=probe({"stdout": "abc", "exit_code": 0}, "abc"))
case("_probe_regular_file", path="/f", seams={"escape_shell_arg": rs("RuntimeError", "x")})
case("_probe_regular_file", path="/f", seams=probe({"stdout": "5", "exit_code": 0}, rs("RuntimeError", "x")))
case("_probe_regular_file", path="/f", seams=probe({"stdout": "  42  ", "exit_code": 0}, "  42  "))

# ── _env_unavailable_error ──
case("_env_unavailable_error", path="/f", status="env_unavailable")
case("_env_unavailable_error", path="/f", status="missing")
case("_env_unavailable_error", path="/f", status="not_regular")
case("_env_unavailable_error", path="/f", status="bad_size")
case("_env_unavailable_error", path="/f", status="working directory '/nope' does not exist")

# ── _detect_binary ──
case("_detect_binary", path="/f", seams={"sample_file_bytes": "6162", "has_binary_extension": False, "is_likely_binary_bytes": False})
case("_detect_binary", path="/f.png", seams={"sample_file_bytes": "6162", "has_binary_extension": True})
case("_detect_binary", path="/f", seams={"sample_file_bytes": None, "escape_shell_arg": "'p'", "exec": {"stdout": "sample", "exit_code": 0}, "strip_fence": "sample", "is_likely_binary": False})
case("_detect_binary", path="/f", seams={"sample_file_bytes": None, "escape_shell_arg": "'p'", "exec": {"stdout": "bin", "exit_code": 0}, "strip_fence": "bin", "is_likely_binary": True})
case("_detect_binary", path="/f", seams={"sample_file_bytes": rs("RuntimeError", "x")})
case("_detect_binary", path="/f", seams={"sample_file_bytes": "6162", "has_binary_extension": rs("ValueError", "x")})
case("_detect_binary", path="/f", seams={"sample_file_bytes": "", "has_binary_extension": False, "is_likely_binary_bytes": False})
case("_detect_binary", path="/f", seams={"sample_file_bytes": None, "escape_shell_arg": "'p'", "exec": rs("RuntimeError", "x")})

# ── _native_path ──
case("_native_path", path="/a/b", env={"cwd": "/env"}, self={"cwd": "/work"})
case("_native_path", path="f", env={"cwd": "/env"}, self={"cwd": "/work"})
case("_native_path", path="f", env={"cwd": None}, self={"cwd": "/work"})
case("_native_path", path="f")
case("_native_path", path="f", env={"cwd": ""}, self={"cwd": "/work"})
case("_native_path", path="sub/f", env={"cwd": "/env/"}, self={"cwd": "/work"})

# ── _python_interpreter_cmd ──
case("_python_interpreter_cmd", amb=AMB, seams={"lsp_local_only": True, "escape_shell_arg": "'/usr/bin/python3'"})
case("_python_interpreter_cmd", amb=AMB, seams={"lsp_local_only": False})
case("_python_interpreter_cmd", amb=AMB, seams={"lsp_local_only": rs("RuntimeError", "x")})
case("_python_interpreter_cmd", amb=AMB, seams={"lsp_local_only": True, "escape_shell_arg": rs("ValueError", "x")})
case("_python_interpreter_cmd", amb={**AMB, "sys_executable": "/opt/py"}, seams={"lsp_local_only": True, "escape_shell_arg": "'/opt/py'"})

# ── _exec_python_snippet ──
SNIP = "print('hi')"
ENC = "cHJpbnQoJ2hpJyk="
case("_exec_python_snippet", snippet=SNIP, py="python3", seams={"exec": {"stdout": "hi", "exit_code": 0}})
case("_exec_python_snippet", snippet=SNIP, py=None, amb=AMB, seams={"lsp_local_only": False, "exec": {"stdout": "hi", "exit_code": 0}})
case("_exec_python_snippet", snippet=SNIP, py=None, amb=AMB, seams={"lsp_local_only": True, "escape_shell_arg": "'/usr/bin/python3'", "exec": {"stdout": "hi", "exit_code": 0}})
case("_exec_python_snippet", snippet=SNIP, py="python3", seams={"exec": rs("RuntimeError", "x")})
case("_exec_python_snippet", snippet=SNIP, py="", seams={"exec": {"stdout": "", "exit_code": 0}})
case("_exec_python_snippet", snippet=SNIP, py=None, amb=AMB, seams={"lsp_local_only": rs("SystemExit", "x")})

# ── read_file_bytes ──
def rfb(probe_exec, probe_strip, b64_exec=None, b64_strip=None, escape="'p'", expand="/f", extra=None):
    s = {"expand_path": expand,
         "escape_shell_arg": [escape, escape],
         "exec": [probe_exec] + ([b64_exec] if b64_exec is not None else []),
         "strip_fence": [probe_strip] + ([b64_strip] if b64_strip is not None else [])}
    if extra:
        s.update(extra)
    return s
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "3", "exit_code": 0}, "3", {"stdout": "QUJD", "exit_code": 0}, "QUJD"))
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "__hermes_missing__", "exit_code": 0}, "__hermes_missing__"))
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "__hermes_not_regular__", "exit_code": 0}, "__hermes_not_regular__",
               extra={"not_regular_error": {"error": "Cannot read '/f': not a regular file (directory, dangling symlink, FIFO, socket, or device). Reading it could block indefinitely."}}))
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "", "exit_code": 1}, ""))
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "abc", "exit_code": 0}, "abc"))
case("read_file_bytes", path="/f", max_bytes=2,
     seams=rfb({"stdout": "3", "exit_code": 0}, "3"))
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "3", "exit_code": 0}, "3", {"stdout": "err", "exit_code": 1}, "err"))
case("read_file_bytes", path="/f", max_bytes=None,
     seams=rfb({"stdout": "3", "exit_code": 0}, "3", {"stdout": "QQ", "exit_code": 0}, "QQ"))
case("read_file_bytes", path="/f", max_bytes=None, seams={"expand_path": rs("RuntimeError", "x")})

# ── _reject_unencodable ──
case("_reject_unencodable", path="/f", content=[104, 105])
case("_reject_unencodable", path="/f", content=[0xDC80])
case("_reject_unencodable", path="/f", content=[0xD800])
case("_reject_unencodable", path="/f", content=[0xDD00])
case("_reject_unencodable", path="/f", content=[104, 0xDC7F, 105])
case("_reject_unencodable", path="/f", content=[])
case("_reject_unencodable", path="/f", content=[0xDCFF, 0xDD00])

# ── _json_nonstandard_constant ──
def jnc(text, strip=None):
    s = {}
    if strip is not None:
        s["strip_bom"] = [strip]
    return s
case("_json_nonstandard_constant", text="plain text", seams={})
case("_json_nonstandard_constant", text='{"a": NaN}', seams=jnc('{"a": NaN}', ['{"a": NaN}', False]))
case("_json_nonstandard_constant", text="[Infinity, NaN]", seams=jnc("[Infinity, NaN]", ["[Infinity, NaN]", False]))
case("_json_nonstandard_constant", text="-Infinity", seams=jnc("-Infinity", ["-Infinity", False]))
case("_json_nonstandard_constant", text='"NaN"', seams=jnc('"NaN"', ['"NaN"', False]))
case("_json_nonstandard_constant", text="NaN!", seams=jnc("NaN!", ["NaN!", False]))
case("_json_nonstandard_constant", text='{"a": NaN}', seams={"strip_bom": rs("ValueError", "x")})
case("_json_nonstandard_constant", text='{"a": NaN}', seams={"strip_bom": rs("RuntimeError", "x")})
case("_json_nonstandard_constant", text="01", seams={})
case("_json_nonstandard_constant", text='\ufeff{"a": Infinity}', seams=jnc('\ufeff{"a": Infinity}', ['{"a": Infinity}', True]))

# ── _refuse_introduced_json_constant ──
case("_refuse_introduced_json_constant", path="/f.json", content="{}", pre_content=None, seams={})
case("_refuse_introduced_json_constant", path="/f.json", content='{"a": NaN}', pre_content=None,
     seams={"strip_bom": [['{"a": NaN}', False]]})
case("_refuse_introduced_json_constant", path="/f.json", content='{"a": NaN}', pre_content="[Infinity]",
     seams={"strip_bom": [['{"a": NaN}', False], ["[Infinity]", False]]})
case("_refuse_introduced_json_constant", path="/f.json", content='{"a": NaN}', pre_content="{}",
     seams={"strip_bom": [['{"a": NaN}', False], ["{}", False]]})
case("_refuse_introduced_json_constant", path="/f.json", content='{"a": NaN}', pre_content=None,
     seams={"strip_bom": rs("RuntimeError", "x")})
case("_refuse_introduced_json_constant", path="/f.json", content="{}", pre_content="[NaN]", seams={})

# ── _fail_closed_syntax_error ──
case("_fail_closed_syntax_error", path="/f.txt", ext=".txt", content="x", amb=AMB, seams={})
case("_fail_closed_syntax_error", path="/f.json", ext=".json", content="{}", amb=AMB, seams={"linter": [[True, ""]]})
case("_fail_closed_syntax_error", path="/f.json", ext=".json", content="{bad", amb=AMB, seams={"linter": [[False, "boom"]]})
case("_fail_closed_syntax_error", path="/f.yaml", ext=".yaml", content="x", amb=AMB, seams={"linter": [[False, "__SKIP__"]]})
case("_fail_closed_syntax_error", path="/f.py", ext=".py", content="x", amb=AMB, seams={})
case("_fail_closed_syntax_error", path="/f.json", ext=".json", content="x", amb=AMB, seams={"linter": rs("RuntimeError", "x")})
case("_fail_closed_syntax_error", path="/f.json", ext=".json", content="x",
     amb={**AMB, "linter_exts": [".py"]}, seams={})

# ── _verify_written_hash ──
AB_SHA = sha(b"ab")
def vwh(native, native_sha=None, exec_=None, escape="'/f'"):
    s = {"native_read_enabled": native}
    if native_sha is not None:
        s["native_sha256"] = native_sha
    if exec_ is not None:
        s["escape_shell_arg"] = escape
        s["exec"] = exec_
    return s
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(False, exec_={"stdout": AB_SHA + "  /f\n", "exit_code": 0}))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(False, exec_={"stdout": "0" * 64 + "  /f\n", "exit_code": 0}))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(False, exec_={"stdout": "", "exit_code": 1}))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(True, native_sha=AB_SHA))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(True, native_sha=None, exec_={"stdout": AB_SHA + "  /f", "exit_code": 0}))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(True, native_sha=rs("RuntimeError", "x")))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(False, exec_=rs("SystemExit", "x")))
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams={"native_read_enabled": rs("ValueError", "x")})
case("_verify_written_hash", path="/f", content_bytes="6162",
     seams=vwh(False, exec_={"stdout": "   ", "exit_code": 0}))

# ── write_file ──
def wf(content, path="/f.txt", expand=None, denied=None, linter=None, lsp_ext=False,
       probe=None, normalize=None, has_bom=None, atomic=None, native=False,
       native_sha=None, hash_exec=None, lint=None, lsp=None, monotonic=1000.0,
       strip_bom=None, extra=None):
    exp = expand if expand is not None else path
    s = {"monotonic": monotonic, "expand_path": exp, "get_write_denied_error": denied}
    if linter is not None:
        s["linter"] = linter
    s["lsp_handles_extension"] = lsp_ext
    s["probe_write_target"] = [probe if probe is not None else [False, None, None]]
    if normalize is not None:
        s["normalize_line_endings"] = normalize
    if has_bom is not None:
        s["has_bom"] = has_bom
    s["atomic_write"] = atomic if atomic is not None else {"stdout": "", "exit_code": 0}
    s["native_read_enabled"] = native
    if native_sha is not None:
        s["native_sha256"] = native_sha
    if hash_exec is not None:
        s["escape_shell_arg"] = "'/f'"
        s["exec"] = hash_exec
    if lint is not None:
        s["check_lint_delta"] = lint
    if lsp is not None:
        s["maybe_lsp_diagnostics"] = lsp
    if strip_bom is not None:
        s["strip_bom"] = [strip_bom]
    if extra:
        s.update(extra)
    return s
HELLO_SHA = sha(b"hello")
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", hash_exec={"stdout": HELLO_SHA + "  /f.txt", "exit_code": 0},
              lint={"success": True, "skipped": False, "output": "", "message": ""}, lsp=""))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", denied="denied by policy"))
case("write_file", path="/f.json", content="{bad", pre_content=None, amb=AMB,
     seams=wf("{bad", path="/f.json", linter=[[False, "boom"]]))
case("write_file", path="/f.json", content='{"a": NaN}', pre_content=None, amb=AMB,
     seams=wf('{"a": NaN}', path="/f.json", linter=[[True, ""]],
              strip_bom=['{"a": NaN}', False]))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", atomic={"stdout": "disk full", "exit_code": 1}))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", hash_exec={"stdout": "0" * 64 + "  /f.txt", "exit_code": 0}))
case("write_file", path="/f.txt", content="a\nb", pre_content=None, amb=AMB,
     seams=wf("a\nb", probe=[True, "pre\r\n", "\r\n"], normalize="a\r\nb", has_bom=False,
              hash_exec={"stdout": sha(b"\xef\xbb\xbfa\r\nb") + "  /f.txt", "exit_code": 0},
              lint={"success": True, "skipped": False, "output": "", "message": ""}, lsp=""))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", hash_exec={"stdout": HELLO_SHA + "  /f.txt", "exit_code": 0},
              extra={"check_lint_delta": None}))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", monotonic=rs("SystemExit", "x")))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", hash_exec={"stdout": HELLO_SHA + "  /f.txt", "exit_code": 0},
              lint={"success": True, "skipped": False, "output": "", "message": ""},
              lsp="semantic warning"))
case("write_file", path="/f.txt", content="hello", pre_content=None, amb=AMB,
     seams=wf("hello", native=True, native_sha=HELLO_SHA,
              lint={"success": False, "skipped": False, "output": "E1", "message": "lint error"}))

# ── _no_match_result ──
case("_no_match_result", path="/f", content="new", old_string="old", new_string="new", match_count=0, error=None,
     seams={"is_already_applied": True})
case("_no_match_result", path="/f", content="c", old_string="old", new_string="new", match_count=0, error=None,
     seams={"is_already_applied": False, "format_no_match_hint": " hint"})
case("_no_match_result", path="/f", content="c", old_string="old", new_string="new", match_count=2, error="custom",
     seams={"is_already_applied": False, "format_no_match_hint": " hint"})
case("_no_match_result", path="/f", content="c", old_string="old", new_string="new", match_count=0, error=None,
     seams={"is_already_applied": False, "format_no_match_hint": rs("ValueError", "x")})
case("_no_match_result", path="/f", content="c", old_string="old", new_string="new", match_count=0, error=None,
     seams={"is_already_applied": False, "format_no_match_hint": rs("SystemExit", "x")})
case("_no_match_result", path="/f", content="c", old_string="old", new_string="new", match_count=0, error=None,
     seams={"is_already_applied": rs("RuntimeError", "x")})
case("_no_match_result", path="/f", content="c", old_string="old", new_string="new", match_count=0, error=None,
     seams={"is_already_applied": False, "format_no_match_hint": None})

# ── _verify_patch_persisted ──
def vpp(data_hex, strip_bom=None, native=False, rc="0", segments=None, decode=None,
        size=None, escape="'p'", exec_=None, new_sentinel=SENT, split=None, strip_rc=None,
        matches_strip=None, extra=None):
    # shell-path _read_exact_bytes scripts
    if segments is None:
        segments = ["pre", data_hex if data_hex is not None else "", size if size is not None else str(len(bytes.fromhex(data_hex)) if data_hex else 0), rc]
    s = {"native_read_enabled": native, "escape_shell_arg": escape,
         "new_sentinel": new_sentinel,
         "exec": exec_ if exec_ is not None else {"stdout": "raw", "exit_code": 0},
         "split_segments": [segments],
         "strip_fence": [strip_rc if strip_rc is not None else rc,
                         matches_strip if matches_strip is not None else (size if size is not None else str(len(bytes.fromhex(data_hex)) if data_hex else 0))],
         "decode_base64_sample": decode if decode is not None else data_hex}
    if strip_bom is not None:
        s["strip_bom"] = [strip_bom]
    if extra:
        s.update(extra)
    return s
case("_verify_patch_persisted", path="/f", new_content="hello",
     seams=vpp("68656c6c6f", strip_bom=["hello", False]))
case("_verify_patch_persisted", path="/f", new_content="hello",
     seams=vpp("68656c6c58", strip_bom=["hellX", False]))
case("_verify_patch_persisted", path="/f", new_content="hello",
     seams=vpp(None, decode=None, segments=["pre", "", "0", "1"], rc="1", strip_rc="1"))
case("_verify_patch_persisted", path="/f", new_content="a\nb",
     seams=vpp("610d0a62", strip_bom=["a\r\nb", False]))
case("_verify_patch_persisted", path="/f", new_content="hello",
     seams=vpp("efbbbf68656c6c6f", strip_bom=["hello", True]))
case("_verify_patch_persisted", path="/f", new_content="hello",
     seams={"native_read_enabled": rs("RuntimeError", "x")})
case("_verify_patch_persisted", path="/f", new_content="hello",
     seams=vpp("68656c6c6f", strip_bom=rs("RuntimeError", "x")))


# ── _read_exact_bytes_hex ──
def hexsegs(payload, size, rc, count=4):
    segs = ["pre", payload, size, rc]
    if count != 4:
        segs = segs[:count]
    return segs
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("61 62", "2", "0")],
            "strip_fence": ["0", "61 62", "2"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("", "0", "127")],
            "strip_fence": ["127"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("6162", "2", "0", count=3)],
            "strip_fence": ["0"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "backend down", "exit_code": 1},
            "split_segments": [hexsegs("6162", "2", "0", count=3)],
            "strip_fence": ["0"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("6162", "2", "x")],
            "strip_fence": ["x"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("No such file", "2", "1")],
            "strip_fence": ["1", "No such file"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("zz", "1", "0")],
            "strip_fence": ["0", "zz", "1"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=1,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("6162", "2", "0")],
            "strip_fence": ["0", "6162", "2"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": {"stdout": "raw", "exit_code": 0},
            "split_segments": [hexsegs("6162", "9", "0")],
            "strip_fence": ["0", "6162", "9"]})
case("_read_exact_bytes_hex", path="/f", max_bytes=None,
     seams={"escape_shell_arg": rs("RuntimeError", "x")})

# ── _read_exact_bytes ──
def rex_shell(payload_b64, data_hex, size, rc, decode=None, count=4, exec_=None):
    segs = ["pre", payload_b64, size, rc]
    if count != 4:
        segs = segs[:count]
    s = {"native_read_enabled": False, "escape_shell_arg": "'p'", "new_sentinel": SENT,
         "exec": exec_ if exec_ is not None else {"stdout": "raw", "exit_code": 0},
         "split_segments": [segs],
         "strip_fence": [rc, size],
         "decode_base64_sample": decode if decode is not None else data_hex}
    return s
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams=rex_shell("YWJj", "616263", "3", "0"))
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={"native_read_enabled": False, "escape_shell_arg": "'p'", "new_sentinel": SENT,
            "exec": [{"stdout": "raw", "exit_code": 0}, {"stdout": "raw", "exit_code": 0}],
            "split_segments": [["pre", "", "0", "127"], ["pre", "6162", "2", "0"]],
            "strip_fence": ["127", "0", "6162", "2"]})
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={"native_read_enabled": True, "os_open": 3, "os_fstat": {"st_mode": 0o100644},
            "fh_open": {"__fh__": True}, "fh_read": "6162", "os_close": None})
case("_read_exact_bytes", path="/f", max_bytes=1,
     seams={"native_read_enabled": True, "os_open": 3, "os_fstat": {"st_mode": 0o100644},
            "fh_open": {"__fh__": True}, "fh_read": "6162", "os_close": None})
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={**rex_shell("YWJj", "616263", "3", "0"),
            "native_read_enabled": True, "os_open": 3, "os_fstat": {"st_mode": 0o40755},
            "os_close": None})
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={**rex_shell("YWJj", "616263", "3", "0"),
            "native_read_enabled": True, "os_open": rs("FileNotFoundError", "nope")})
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={"native_read_enabled": True, "os_open": rs("RuntimeError", "x")})
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams=rex_shell("", None, "0", "0", decode=None, count=3))
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams=rex_shell("YWJj", None, "3", "0", decode=None))
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams=rex_shell("YWJj", "616263", "9", "0"))
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={"native_read_enabled": rs("RuntimeError", "x")})
case("_read_exact_bytes", path="/f", max_bytes=None,
     seams={**rex_shell("YWJj", "616263", "3", "0"),
            "native_read_enabled": True, "os_open": 3, "os_fstat": {"st_mode": 0o100644},
            "fh_open": {"__fh__": True}, "fh_read": rs("OSError", "read fail"), "os_close": None})
case("_read_exact_bytes", path="/f", max_bytes=2,
     seams=rex_shell("YWJjZA==", "61626364", "4", "0"))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        print("WARNING funcs with <5 cases: %s" % short)
    else:
        print("every func has >=5 cases")


if __name__ == "__main__":
    main()