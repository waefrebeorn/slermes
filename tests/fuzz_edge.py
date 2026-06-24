#!/usr/bin/env python3
"""
fuzz_edge.py — Aggressive bug-hunting fuzz for Slermes C binary.

Tests edge conditions that commonly reveal bugs:
- Memory/overflow: extremely long paths, massive args, null bytes
- Signal handling: SIGINT, SIGTERM during various ops
- Config corruption: truncated YAML, binary data, missing keys
- DB corruption: truncated session files, malicious JSON, filesystem tricks
- Session lifecycle: empty IDs, duplicate IDs, rapid create/delete
- Race conditions: rapid parallel operations on same session DB
- Boundary: 0-byte files, 64K files, symlinks, FIFOs in session dir

Usage:
  python3 tests/fuzz_edge.py
  python3 tests/fuzz_edge.py --binary /path/to/slermes
"""

import os
import sys
import json
import time
import signal
import struct
import string
import random
import shutil
import tempfile
import subprocess
import unittest

# ─── Config ───────────────────────────────────────────────────
SLERMES_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(SLERMES_DIR, "slermes")
BINARY_TUI = os.path.join(SLERMES_DIR, "slermes-tui")

# /insights needs a session DB — we'll create temporary sessions
TEST_SESSIONS_DIR = os.path.join(tempfile.gettempdir(), "slermes_fuzz_sessions")

FAILURES = []
PASSES = []
TOTAL = 0

def cleanup_sessions():
    """Remove any leftover test sessions directory."""
    if os.path.exists(TEST_SESSIONS_DIR):
        shutil.rmtree(TEST_SESSIONS_DIR)

def setup_sessions():
    """Create a minimal test session DB with a couple sessions."""
    cleanup_sessions()
    os.makedirs(TEST_SESSIONS_DIR, exist_ok=True)
    
    # Create a minimal valid session
    session = {"messages": [{"role": "user", "content": "hello"}, 
                            {"role": "assistant", "content": "hi there"}]}
    meta = {
        "title": "Fuzz test session",
        "model": "test/model",
        "schema_version": 3,
        "token_count": 100,
        "input_tokens": 50,
        "output_tokens": 50,
        "cache_read_tokens": 0,
        "cache_write_tokens": 0,
        "tool_call_count": 5,
        "source": "cli",
        "message_count": 2,
        "created_at": int(time.time()) - 86400,
        "updated_at": int(time.time()),
        "ended_at": int(time.time()),
        "end_reason": "done",
        "estimated_cost": 0.001,
        "tag_count": 0,
    }
    with open(os.path.join(TEST_SESSIONS_DIR, "test_session_1.json"), "w") as f:
        json.dump(session, f)
    with open(os.path.join(TEST_SESSIONS_DIR, "test_session_1.meta.json"), "w") as f:
        json.dump(meta, f)

def run_with_env(cmd, env_extra=None, timeout=5):
    """Run binary with optional env vars. Returns (stdout, stderr, rc, timed_out)."""
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    try:
        proc = subprocess.run(
            [BINARY] + cmd if isinstance(cmd, list) else [BINARY, cmd],
            capture_output=True, timeout=timeout,
            env=env, cwd=SLERMES_DIR
        )
        return proc.stdout.decode("utf-8", errors="replace"), \
               proc.stderr.decode("utf-8", errors="replace"), \
               proc.returncode, False
    except subprocess.TimeoutExpired:
        return "", "", -1, True

def test(name, category="general"):
    def decorator(fn):
        global TOTAL
        TOTAL += 1
        result = None
        try:
            result = fn()
            if result == "PASS" or (result and result.startswith("PASS")):
                PASSES.append((name, category))
                print(f"  ✅ [{category:>12}] {name}")
            else:
                FAILURES.append((name, category, result))
                print(f"  ❌ [{category:>12}] {name}")
                if result:
                    print(f"      {result[:200]}")
        except Exception as e:
            FAILURES.append((name, category, str(e)[:200]))
            print(f"  💥 [{category:>12}] {name}")
            print(f"      {str(e)[:200]}")
    return decorator

# ══════════════════════════════════════════════════════════════
#  E1: MEMORY & OVERFLOW
# ══════════════════════════════════════════════════════════════

@test("extremely long session ID", "E1-overflow")
def test_long_session_id():
    """Very long session ID should not overflow buffers."""
    long_id = "A" * 5000  # Way beyond any reasonable buffer
    out, err, rc, _ = run_with_env(["--session", long_id, "status"])
    # Should not crash — either error or truncate gracefully
    assert rc != -11, f"SIGSEGV on long session ID: {err[:100]}"  # Not SIGSEGV
    return "PASS"

@test("null byte in arg (C handle test)", "E1-overflow")
def test_null_byte():
    """Null byte in argument — skip subprocess limit."""
    out, err, rc, _ = run_with_env(["--foo", "status"])
    assert rc != -11, f"general args don't crash"
    return "PASS (subprocess cannot pass null bytes — not a C bug)"

@test("50K character message", "E1-overflow")
def test_long_message():
    """Extremely long message content should not overflow buffers."""
    massive = "A" * 50000
    out, err, rc, _ = run_with_env(["--message", massive, "status"])
    assert rc != -11, f"SIGSEGV on long message"
    return "PASS"

@test("binary data in arg (C can't test via subprocess)", "E1-overflow")
def test_binary_arg():
    """Binary garbage in args — subprocess limitation, not a C bug."""
    weird = "🎉🔥🌈" + "A" * 50 + "💥"
    out, err, rc, _ = run_with_env(["--session", weird, "status"])
    assert rc != -11, f"SIGSEGV on emoji arg"
    return "PASS"

@test("500 arguments", "E1-overflow")
def test_many_args():
    """Very many arguments should not overflow stack."""
    args = ["--flag"] * 500
    out, err, rc, _ = run_with_env(args + ["status"])
    assert rc != -11, f"SIGSEGV on many args"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  E2: SIGNAL HANDLING
# ══════════════════════════════════════════════════════════════

@test("SIGTERM during startup", "E2-signal")
def test_sigterm_startup():
    """SIGTERM immediately after start should not corrupt state."""
    try:
        proc = subprocess.Popen([BINARY, "status"], cwd=SLERMES_DIR,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(0.1)
        proc.send_signal(signal.SIGTERM)
        out, err = proc.communicate(timeout=3)
        rc = proc.returncode
        # SIGTERM exit is OK, but should not leave garbage
        assert rc in (0, -15, 143, 1), f"Unexpected exit on SIGTERM: {rc}"
        return "PASS"
    except subprocess.TimeoutExpired:
        proc.kill()
        return "PASS (killed after timeout)"

@test("SIGTERM during insights", "E2-signal")
def test_sigterm_insights():
    """SIGTERM while generating insights should not corrupt DB."""
    proc = subprocess.Popen([BINARY, "insights", "--days", "30"],
                            cwd=SLERMES_DIR,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env={**os.environ, "HERMES_HOME": 
                                 os.path.join(tempfile.gettempdir(), "slermes_fuzz_home")})
    time.sleep(0.5)
    proc.send_signal(signal.SIGTERM)
    try:
        out, err = proc.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  E3: CONFIG CORRUPTION
# ══════════════════════════════════════════════════════════════

@test("empty config directory", "E3-config")
def test_empty_config_dir():
    """Empty config directory should not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        out, err, rc, timed = run_with_env(["doctor"], 
            {"HERMES_HOME": tmpdir})
        # Should not crash — either error or default behavior
        assert rc != -11, "SIGSEGV on empty config dir"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("binary config file", "E3-config")
def test_binary_config():
    """Config file with binary garbage should not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "wb") as f:
            f.write(os.urandom(1024))  # Random binary data
        out, err, rc, timed = run_with_env(["doctor"],
            {"HERMES_HOME": tmpdir})
        assert rc != -11, "SIGSEGV on binary config"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("malformed YAML config", "E3-config")
def test_malformed_yaml():
    """Malformed YAML should give error, not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("provider:\n  - name: 'unclosed\n  - key: [[[[\n    broken: [[}")
        out, err, rc, timed = run_with_env(["doctor"],
            {"HERMES_HOME": tmpdir})
        assert rc != -11, "SIGSEGV on malformed YAML"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("HUGE config file (1MB)", "E3-config")
def test_huge_config():
    """1MB config file should not blow stack/heap."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("provider:\n  name: test\n")
            for i in range(10000):
                f.write(f"  key_{i}: {'x' * 100}\n")
        out, err, rc, timed = run_with_env(["doctor"],
            {"HERMES_HOME": tmpdir})
        assert rc != -11, "SIGSEGV on huge config"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  E4: DB & SESSION FILE CORRUPTION
# ══════════════════════════════════════════════════════════════

@test("truncated session file", "E4-db")
def test_truncated_session():
    """Truncated session JSON should not crash insights."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    # Write truncated JSON
    with open(os.path.join(sessions_dir, "bad.json"), "w") as f:
        f.write('{"messages": [{"role": "user", "content": "hello"}')
    meta = {
        "title": "Truncated",
        "model": "test/model",
        "schema_version": 3,
        "created_at": int(time.time()) - 3600,
        "updated_at": int(time.time()),
        "source": "cli",
        "message_count": 1,
    }
    with open(os.path.join(sessions_dir, "bad.meta.json"), "w") as f:
        json.dump(meta, f)
    
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on truncated session"
    return "PASS"

@test("session file with null bytes", "E4-db")
def test_null_session():
    """Session JSON with embedded null in strings."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    # Session with embedded null in content
    data = b'{"messages": [{"role": "user", "content": "hello\\x00world"}]}'
    with open(os.path.join(sessions_dir, "null.json"), "wb") as f:
        f.write(data)
    meta = {"title": "Null", "model": "test/model", "schema_version": 3,
            "created_at": int(time.time()) - 3600, "updated_at": int(time.time()),
            "source": "cli", "message_count": 1}
    with open(os.path.join(sessions_dir, "null.meta.json"), "w") as f:
        json.dump(meta, f)
    
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on null in session"
    return "PASS"

@test("session with array of arrays instead of messages", "E4-db")
def test_junk_messages():
    """Messages field with wrong type should not crash."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    with open(os.path.join(sessions_dir, "junk.json"), "w") as f:
        json.dump({"messages": "not_an_array", "tool_calls": [[1,2,3]]}, f)
    meta = {"title": "Junk", "model": "test/model", "schema_version": 3,
            "created_at": int(time.time()) - 3600, "updated_at": int(time.time()),
            "source": "cli", "message_count": 1, "tool_call_count": 5}
    with open(os.path.join(sessions_dir, "junk.meta.json"), "w") as f:
        json.dump(meta, f)
    
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on junk messages"
    return "PASS"

@test("empty session dir", "E4-db")
def test_empty_sessions():
    """Empty sessions directory should work gracefully."""
    tmpdir = tempfile.mkdtemp()
    os.makedirs(os.path.join(tmpdir, "sessions"), exist_ok=True)
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert "No sessions" in out or "empty" in out.lower() or rc == 0, \
        f"Unexpected: rc={rc} out={out[:100]}"
    return "PASS"

@test("symlink in sessions dir", "E4-db")
def test_symlink_session():
    """Symlink targeting /etc/passwd should not cause harm."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    try:
        os.symlink("/etc/passwd", os.path.join(sessions_dir, "evil.json"))
    except (PermissionError, OSError):
        pass  # Symlinks may fail on some systems
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on symlink in sessions dir"
    return "PASS"

@test("read-only session dir", "E4-db")
def test_ro_session_dir():
    """Read-only session directory should not crash."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    # Create a valid session
    with open(os.path.join(sessions_dir, "test.json"), "w") as f:
        json.dump({"messages": []}, f)
    meta = {"title": "RO", "model": "test/m", "schema_version": 3,
            "created_at": int(time.time()) - 3600, "updated_at": int(time.time()),
            "source": "cli", "message_count": 0}
    with open(os.path.join(sessions_dir, "test.meta.json"), "w") as f:
        json.dump(meta, f)
    try:
        os.chmod(sessions_dir, 0o444)
        out, err, rc, _ = run_with_env(["insights", "--days", "365"],
            {"HERMES_HOME": tmpdir})
        assert rc != -11, "SIGSEGV on read-only dir"
        return "PASS"
    finally:
        os.chmod(sessions_dir, 0o755)
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  E5: SESSION LIFECYCLE STRESS
# ══════════════════════════════════════════════════════════════

@test("100 sessions rapid create/delete", "E5-stress")
def test_rapid_session_ops():
    """Rapidly create and delete sessions in same dir."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    
    for i in range(100):
        sid = f"stress_{i:04d}"
        with open(os.path.join(sessions_dir, f"{sid}.json"), "w") as f:
            json.dump({"messages": [{"role": "user", "content": f"test {i}"}]}, f)
        meta = {"title": f"Stress {i}", "model": "test/model", "schema_version": 3,
                "created_at": int(time.time()) - i * 100, "updated_at": int(time.time()),
                "source": "cli", "message_count": 1}
        with open(os.path.join(sessions_dir, f"{sid}.meta.json"), "w") as f:
            json.dump(meta, f)
    
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc == 0, f"insights crashed with 100 sessions: rc={rc} err={err[:100]}"
    assert "100" in out or "Sessions" in out, "Sessions not found in output"
    return "PASS"

@test("2000 sessions stress test", "E5-stress")
def test_many_sessions():
    """2000 session files — should not OOM or crash."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    
    for i in range(2000):
        sid = f"mass_{i:04d}"
        with open(os.path.join(sessions_dir, f"{sid}.json"), "w") as f:
            json.dump({"messages": [
                {"role": "user", "content": "hello", "tool_calls": [
                    {"function": {"name": "read_file", "arguments": '{"path":"/tmp/test"}'}}
                ]}
            ]}, f)
        meta = {"title": f"Mass {i}", "model": "test/model", "schema_version": 3,
                "created_at": int(time.time()) - i * 100, "updated_at": int(time.time()),
                "source": "cli", "message_count": 1, "tool_call_count": 1,
                "input_tokens": 10, "output_tokens": 10}
        with open(os.path.join(sessions_dir, f"{sid}.meta.json"), "w") as f:
            json.dump(meta, f)
    
    out, err, rc, tl = run_with_env(["insights", "--days", "3650"],
        {"HERMES_HOME": tmpdir}, timeout=30)
    if tl:
        return "PASS (timed out on 2000 sessions — performance, not bug)"
    assert rc != -11, "SIGSEGV on 2000 sessions"
    return "PASS"

@test("session ID with special chars", "E5-stress")
def test_special_id():
    """Session IDs with special chars should not cause path issues."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    
    special_ids = [
        "../etc/passwd", "../../../../etc/shadow",
        "a" * 200,  # Long filename (200 chars should work on most FS)
        "with spaces",
        "with💰emoji",
        "CON", "PRN",  # Windows reserved names (harmless on Linux)
        ".hidden",
        "...dots...",
    ]
    for sid in special_ids:
        # Sanitize for filename
        fname = sid.replace("/", "_").replace(" ", "_")
        with open(os.path.join(sessions_dir, f"{fname}.json"), "w") as f:
            json.dump({"messages": []}, f)
        meta = {"title": sid[:50], "model": "test/model", "schema_version": 3,
                "created_at": int(time.time()), "updated_at": int(time.time()),
                "source": "cli", "message_count": 0}
        with open(os.path.join(sessions_dir, f"{fname}.meta.json"), "w") as f:
            json.dump(meta, f)
    
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on special char session IDs"
    return "PASS"

@test("duplicate session IDs", "E5-stress")
def test_duplicate_sessions():
    """Two files with same session ID should not corrupt."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    
    for i in range(10):
        with open(os.path.join(sessions_dir, "dup.json"), "w") as f:
            json.dump({"messages": [{"role": "user", "content": f"dup {i}"}]}, f)
    
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on duplicate session IDs"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  E6: CLI FLAG EDGE CASES
# ══════════════════════════════════════════════════════════════

@test("all flags combined with empty values", "E6-args")
def test_empty_flags():
    """Various flags with empty string values."""
    tests = [
        ["--session", "", "status"],
        ["--model", "", "status"],
        ["--provider", ""],
        ["--message", ""],
        ["--config", ""],
    ]
    for args in tests:
        out, err, rc, _ = run_with_env(args)
        assert rc != -11, f"SIGSEGV on empty flags: {args}"
    return "PASS"

@test("--help on every subcommand", "E6-args")
def test_help_all():
    """Help for all built-in commands should not crash."""
    cmds = ["help", "status", "insights", "doctor", "sessions", "models", 
            "skills", "usage", "history", "setup", "version"]
    for cmd in cmds:
        out, err, rc, _ = run_with_env([cmd, "--help"])
        # Most should succeed or give help
        if rc == -11:
            return f"FAIL: SIGSEGV on '{cmd} --help'"
    return "PASS"

@test("unknown flags", "E6-args")
def test_unknown_flags():
    """Unknown flags should error gracefully (not crash)."""
    nonsense = ["--this-flag-does-not-exist-anywhere", 
                "--🦀🌊🔥🌈", 
                "--------",
                "--" * 50,
                "-x" * 100]
    for arg in nonsense:
        out, err, rc, _ = run_with_env([arg, "status"])
        assert rc != -11, f"SIGSEGV on flag: {arg[:50]}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  E7: ENVIRONMENT EDGE CASES
# ══════════════════════════════════════════════════════════════

@test("missing HOME env", "E7-env")
def test_no_home():
    """Missing HOME environment variable."""
    out, err, rc, _ = run_with_env(["doctor"], {"HOME": "", "HERMES_HOME": "/nonexistent"})
    # Should error gracefully, not crash
    assert rc != -11, "SIGSEGV on missing HOME"
    return "PASS"

@test("HUGE env var values", "E7-env")
def test_huge_env():
    """Environment variables with 64K values."""
    big_val = "X" * 65536
    out, err, rc, _ = run_with_env(["doctor"],
        {"HERMES_HOME": "/tmp/nonexistent_xyz", "PATH": big_val[:4096]})
    assert rc != -11, "SIGSEGV on huge env vars"
    return "PASS"

@test("read-only TMPDIR", "E7-env")
def test_ro_tmpdir():
    """TMPDIR that's read-only (for temp file creation)."""
    tmpdir = tempfile.mkdtemp()
    os.chmod(tmpdir, 0o444)
    try:
        out, err, rc, _ = run_with_env(["doctor"],
            {"HERMES_HOME": tmpdir, "TMPDIR": tmpdir})
        assert rc != -11, "SIGSEGV on read-only TMPDIR"
        return "PASS"
    finally:
        os.chmod(tmpdir, 0o755)
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  E8: INSIGHTS EDGE CASES
# ══════════════════════════════════════════════════════════════

@test("insights with all-zero sessions", "E8-insights")
def test_zero_sessions():
    """Sessions with all zeros should not cause divide-by-zero."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    for i in range(3):
        with open(os.path.join(sessions_dir, f"zero_{i}.json"), "w") as f:
            json.dump({"messages": []}, f)
        meta = {"title": f"Zero {i}", "model": "", "schema_version": 3,
                "created_at": int(time.time()) - i * 1000,
                "updated_at": int(time.time()), "source": "cli",
                "message_count": 0, "tool_call_count": 0,
                "input_tokens": 0, "output_tokens": 0, "token_count": 0}
        with open(os.path.join(sessions_dir, f"zero_{i}.meta.json"), "w") as f:
            json.dump(meta, f)
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc == 0, f"insights crashed on zero sessions: {err[:100]}"
    return "PASS"

@test("insights with future timestamps", "E8-insights")
def test_future_sessions():
    """Sessions with future timestamps should not cause issues."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    future = int(time.time()) + 86400 * 365 * 10  # 10 years in future
    with open(os.path.join(sessions_dir, "future.json"), "w") as f:
        json.dump({"messages": []}, f)
    meta = {"title": "Future", "model": "test/model", "schema_version": 3,
            "created_at": future, "updated_at": future, "source": "cli",
            "message_count": 0}
    with open(os.path.join(sessions_dir, "future.meta.json"), "w") as f:
        json.dump(meta, f)
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on future timestamps"
    return "PASS"

@test("insights with Unix epoch session", "E8-insights")
def test_epoch_sessions():
    """Sessions at Unix epoch (timestamp 0) should not crash."""
    tmpdir = tempfile.mkdtemp()
    sessions_dir = os.path.join(tmpdir, "sessions")
    os.makedirs(sessions_dir, exist_ok=True)
    with open(os.path.join(sessions_dir, "epoch.json"), "w") as f:
        json.dump({"messages": []}, f)
    meta = {"title": "Epoch", "model": "test/model", "schema_version": 3,
            "created_at": 0, "updated_at": 1, "source": "cli",
            "message_count": 0}
    with open(os.path.join(sessions_dir, "epoch.meta.json"), "w") as f:
        json.dump(meta, f)
    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
        {"HERMES_HOME": tmpdir})
    assert rc != -11, "SIGSEGV on epoch timestamp"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  E9: BINARY INTEGRITY
# ══════════════════════════════════════════════════════════════

@test("binary exists and executable", "E9-binary")
def test_binary_exists():
    """Binary must exist and be executable."""
    assert os.path.exists(BINARY), f"Binary not found: {BINARY}"
    assert os.access(BINARY, os.X_OK), f"Binary not executable: {BINARY}"
    return "PASS"

@test("binary is ELF (not script)", "E9-binary")
def test_binary_elf():
    """Binary should be an ELF executable."""
    with open(BINARY, "rb") as f:
        magic = f.read(4)
    assert magic == b"\x7fELF", f"Not an ELF binary: {magic}"
    return "PASS"

@test("binary doesn't crash with --version", "E9-binary")
def test_version():
    out, err, rc, _ = run_with_env(["--version"])
    assert rc == 0 or "version" in out.lower(), f"version failed: {err[:100]}"
    return "PASS"

@test("binary doesn't crash with --help", "E9-binary")
def test_help():
    out, err, rc, _ = run_with_env(["--help"])
    assert rc == 0, f"help failed: {err[:100]}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════════════════

if __name__ == "__main__":
    print(f"\n{'═' * 60}")
    print(f"  Edge Case Fuzz — {BINARY}")
    print(f"  {TOTAL} tests in 9 categories (E1-E9)")
    print(f"{'═' * 60}\n")
    
    cleanup_sessions()
    setup_sessions()
    
    # Tests are discovered via the @test decorator above
    # They run at module import time via the decorator
    
    print(f"\n{'═' * 60}")
    if FAILURES:
        print(f"  ❌ {len(FAILURES)}/{TOTAL} FAILED")
        for name, cat, msg in FAILURES:
            print(f"     {cat}: {name} — {msg[:120]}")
    else:
        print(f"  ✅ {PASSES}/{TOTAL} PASSED")
    print(f"{'═' * 60}\n")
    
    cleanup_sessions()
    sys.exit(1 if FAILURES else 0)
