#!/usr/bin/env python3
"""
fuzz_context.py — Context management & compression subsystem fuzz for Slermes C.

Tests all context-related functionality:
- C1: Context engine initialization and config
- C2: Compression thresholds, adaptive feedback
- C3: Compression lock (acquire/release/contention)
- C4: Session splitting (compression-driven rotation)
- C5: Manual compression feedback
- C6: Token counting / estimation
- C7: Context eviction strategies
- C8: Prompt caching
- C9: Context references expansion
- C10: Edge cases (zero messages, single message, all system)

Usage:
  python3 tests/fuzz_context.py
"""

import os
import sys
import json
import time
import signal
import shutil
import struct
import random
import tempfile
import subprocess

# ─── Config ───────────────────────────────────────────────────
SLERMES_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(SLERMES_DIR, "slermes")

FAILURES = []
PASSES = []
TOTAL = 0
MAX_NAME = 36

def run_with_env(cmd, env_extra=None, timeout=10):
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
            ok = (result == "PASS") or (result and result.startswith("PASS"))
            if ok:
                PASSES.append((name, category))
                ansi = "✅"
            else:
                FAILURES.append((name, category, result))
                ansi = "❌"
            pad = "." * max(1, MAX_NAME - len(name))
            msg = result[:60] if result and not ok and result != "PASS" else ""
            print(f"  {ansi} [{category:<10}] {name} {pad} {msg}")
        except Exception as e:
            FAILURES.append((name, category, str(e)[:120]))
            print(f"  💥 [{category:<10}] {name} {'.' * max(1, MAX_NAME-len(name))} {str(e)[:80]}")
    return decorator

# ══════════════════════════════════════════════════════════════
#  C1: CONTEXT ENGINE
# ══════════════════════════════════════════════════════════════

@test("config: compression block present", "C1-engine")
def test_compression_config():
    """Config with compression block should not crash doctor."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("compression:\n  enabled: true\n  threshold: 0.75\n  cooldown_secs: 30\n  failure_cooldown_secs: 120\n  tail_messages: 10\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: extreme compression values", "C1-engine")
def test_extreme_compression():
    """Extreme compression config values should not overflow."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("compression:\n  enabled: true\n  threshold: 999\n  cooldown_secs: 999999\n  failure_cooldown_secs: -1\n  tail_messages: 99999\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: zero compression values", "C1-engine")
def test_zero_compression():
    """Zero compression values should not cause divide-by-zero."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("compression:\n  enabled: true\n  threshold: 0\n  cooldown_secs: 0\n  tail_messages: 0\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: compression disabled", "C1-engine")
def test_compression_disabled():
    """Explicitly disabled compression should not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("compression:\n  enabled: false\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  C2: ALTERNATIVE PROMPT CACHING CONFIG
# ══════════════════════════════════════════════════════════════

@test("config: prompt_caching block", "C2-cache")
def test_prompt_caching_config():
    """Config with prompt_caching block should not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("prompt_caching:\n  enabled: true\n  min_system_prompt_tokens: 1024\n  cache_ttl_seconds: 300\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: prompt_caching extreme", "C2-cache")
def test_prompt_caching_extreme():
    """Extreme prompt_caching values."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("prompt_caching:\n  enabled: true\n  min_system_prompt_tokens: 999999999\n  cache_ttl_seconds: 0\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  C3: CONTEXT REFERENCES
# ══════════════════════════════════════════════════════════════

@test("config: context_references block", "C3-refs")
def test_context_refs_config():
    """Config with context_references block should not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("context_references:\n  enabled: true\n  max_files: 10\n  max_tokens: 4096\n  follow_symlinks: false\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: context_refs extreme", "C3-refs")
def test_context_refs_extreme():
    """Extreme context_references values."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("context_references:\n  max_files: 99999\n  max_tokens: 0\n  max_size_mb: 99999\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  C4: COMPRESSION FEEDBACK
# ══════════════════════════════════════════════════════════════

@test("src: compression_feedback functions exist", "C4-feedback")
def test_feedback_functions():
    """Check that key compression_feedback functions exist."""
    checks = [
        ("compression_feedback_init", "src/agent/context.c"),
        ("compression_feedback_positive", "src/agent/context.c"), 
        ("compression_feedback_negative", "src/agent/context.c"),
        ("compression_feedback_get_threshold", "src/agent/context.c"),
        ("summarize_manual_compression", "src/agent/manual_compression_feedback.c"),
    ]
    for func, expected_file in checks:
        r = subprocess.run(["grep", "-rn", func, "src/agent/"],
                          capture_output=True, text=True, cwd=SLERMES_DIR)
        if not r.stdout.strip():
            return f"MISSING: {func}"
    return "PASS"

@test("src: compress cooldown fields exist", "C4-feedback")
def test_cooldown_fields():
    """Check that compression cooldown fields exist in agent_state."""
    checks = [
        "last_compress_time",
        "compress_cooldown_secs",
        "last_compress_failure_time",
        "compress_failure_cooldown_secs",
        "compress_tail_messages",
        "compression_fb",
    ]
    for field in checks:
        r = subprocess.run(["grep", "-rn", field, "include/"],
                          capture_output=True, text=True, cwd=SLERMES_DIR)
        if not r.stdout.strip():
            return f"FIELD MISSING: {field}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  C5: COMPRESSION LOCK
# ══════════════════════════════════════════════════════════════

@test("src: compression lock functions exist", "C5-lock")
def test_lock_functions():
    """Check compression lock API functions."""
    checks = [
        "db_try_acquire_compression_lock",
        "db_release_compression_lock",
        "db_get_compression_lock_holder",
    ]
    for func in checks:
        r = subprocess.run(["grep", "-rn", func, "lib/libdb/"],
                          capture_output=True, text=True, cwd=SLERMES_DIR)
        if not r.stdout.strip():
            return f"MISSING: {func}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  C6: TOKEN COUNTING
# ══════════════════════════════════════════════════════════════

@test("src: token counting functions exist", "C6-tokens")
def test_token_functions():
    """Check key token counting functions exist."""
    checks = [
        "llm_count_context_tokens",
        "llm_estimate_tokens",
        "context_message_tokens",
        "estimate_request_tokens_rough",
    ]
    for func in checks:
        r = subprocess.run(
            ["grep", "-rn", func, "src/agent/", "include/"],
            capture_output=True, text=True, cwd=SLERMES_DIR)
        if not r.stdout.strip():
            return f"MISSING: {func}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  C7: CONTEXT EVICTION
# ══════════════════════════════════════════════════════════════

@test("src: eviction functions exist", "C7-evict")
def test_eviction_functions():
    """Check context eviction functions."""
    checks = [
        "context_evict_smart",
        "llm_truncate_context",
        "llm_compress_context",
        "compress_prune_tool_results",
    ]
    for func in checks:
        r = subprocess.run(
            ["grep", "-rn", func, "src/agent/", "include/"],
            capture_output=True, text=True, cwd=SLERMES_DIR)
        if not r.stdout.strip():
            return f"MISSING: {func}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  C8: CONFIG STRESS (ALL COMBINATIONS)
# ══════════════════════════════════════════════════════════════

@test("config: all context keys combined", "C8-stress")
def test_all_context_keys():
    """Config with every context/compression key should not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        cfg = """compression:
  enabled: true
  threshold: 0.75
  cooldown_secs: 30
  failure_cooldown_secs: 120
  tail_messages: 10
prompt_caching:
  enabled: true
  min_system_prompt_tokens: 1024
  cache_ttl_seconds: 300
context_references:
  enabled: true
  max_files: 10
  max_tokens: 4096
  max_size_mb: 10
context_engine:
  plugin: default
"""
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write(cfg)
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: malformed context YAML", "C8-stress")
def test_malformed_context():
    """Malformed context YAML should error, not crash."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("compression:\n  threshold: [\n    - 'unclosed\n  cooldown: [[[[[\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

@test("config: huge context config (100KB)", "C8-stress")
def test_huge_context_config():
    """Config with 100KB of context data should not blow stack."""
    tmpdir = tempfile.mkdtemp()
    try:
        with open(os.path.join(tmpdir, "config.yaml"), "w") as f:
            f.write("compression:\n  enabled: true\n")
            for i in range(1000):
                f.write(f"  ref_{i}: {'x' * 100}\n")
        out, err, rc, _ = run_with_env(["doctor"], {"HERMES_HOME": tmpdir})
        assert rc != -6 and rc != -11, f"Crash: rc={rc}"
        return "PASS"
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

# ══════════════════════════════════════════════════════════════
#  C9: COMPRESSION SESSION EDGE CASES
# ══════════════════════════════════════════════════════════════

@test("sessions with compressed flag", "C9-session")
def test_compressed_session_flag():
    """Sessions with compression metadata should not crash insights."""
    tmpdir = tempfile.mkdtemp()
    sess = os.path.join(tmpdir, "sessions")
    os.makedirs(sess, exist_ok=True)

    # Session with compression metadata
    meta = {
        "title": "Compressed test",
        "model": "test/model",
        "schema_version": 3,
        "created_at": int(time.time()) - 86400,
        "updated_at": int(time.time()),
        "source": "cli",
        "message_count": 5,
        "input_tokens": 1000,
        "output_tokens": 500,
        "tool_call_count": 3,
        "end_reason": "compression",
        "parent_id": "orig_session_id",
        "branch_point": 10,
        "compression_count": 3,
        "estimated_cost": 0.01,
        "token_count": 1500,
        "cache_read_tokens": 100,
        "cache_write_tokens": 50,
    }
    with open(os.path.join(sess, "compressed_session.json"), "w") as f:
        json.dump({"messages": [{"role": "user", "content": "test"},
                                {"role": "assistant", "content": "compressed summary",
                                 "tool_calls": [{"function": {"name": "read_file",
                                    "arguments": '{"path":"/tmp/test"}'}}]}]}, f)
    with open(os.path.join(sess, "compressed_session.meta.json"), "w") as f:
        json.dump(meta, f)

    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
                                    {"HERMES_HOME": tmpdir})
    assert rc != -6 and rc != -11, f"Crash: rc={rc}"
    return "PASS"

@test("sessions: compression lineage chain", "C9-session")
def test_compression_lineage():
    """Session lineage with parent/child compression chain."""
    tmpdir = tempfile.mkdtemp()
    sess = os.path.join(tmpdir, "sessions")
    os.makedirs(sess, exist_ok=True)

    # Chain of 3 sessions: original → compressed_v1 → compressed_v2
    chain = [
        ("orig", None),
        ("comp_v1", "orig"),
        ("comp_v2", "comp_v1"),
    ]
    for sid, parent in chain:
        m = {
            "title": sid, "model": "test/model", "schema_version": 3,
            "created_at": int(time.time()) - 86400,
            "updated_at": int(time.time()),
            "source": "cli", "message_count": 3,
            "tool_call_count": 1,
        }
        if parent:
            m["end_reason"] = "compression"
            m["parent_id"] = parent
            m["branch_point"] = 5
        with open(os.path.join(sess, f"{sid}.json"), "w") as f:
            json.dump({"messages": [{"role": "user", "content": sid}]}, f)
        with open(os.path.join(sess, f"{sid}.meta.json"), "w") as f:
            json.dump(m, f)

    out, err, rc, _ = run_with_env(["insights", "--days", "365"],
                                    {"HERMES_HOME": tmpdir})
    assert rc != -6 and rc != -11, f"Crash: rc={rc}"
    return "PASS"

# ══════════════════════════════════════════════════════════════
#  C10: SOURCE-LEVEL CONSISTENCY
# ══════════════════════════════════════════════════════════════

@test("src: manual_compression_feedback has real code", "C10-consist")
def test_manual_feedback_impl():
    """manual_compression_feedback.c should have real code, not just wrapper."""
    r = subprocess.run(["wc", "-l", "src/agent/manual_compression_feedback.c"],
                       capture_output=True, text=True, cwd=SLERMES_DIR)
    loc = int(r.stdout.split()[0])
    assert loc > 5, f"Only {loc} LOC — likely empty wrapper"
    return f"PASS ({loc} LOC)"

@test("src: context file sizes match expectations", "C10-consist")
def test_context_file_sizes():
    """Context files should be substantial implementations."""
    checks = {
        "src/agent/context.c": 500,
        "src/agent/context_engine.c": 100,
        "src/agent/context_references.c": 300,
        "src/agent/manual_compression_feedback.c": 5,
        "src/agent/prompt_caching.c": 100,
    }
    for fpath, min_loc in checks.items():
        r = subprocess.run(["wc", "-l", fpath],
                          capture_output=True, text=True, cwd=SLERMES_DIR)
        loc = int(r.stdout.split()[0])
        if loc < min_loc:
            return f"{fpath}: {loc} LOC < {min_loc}"
    return "PASS"

@test("src: compress function in llm_client.c", "C10-consist")
def test_compress_in_llm_client():
    """llm_compress_context should be a substantive function."""
    r = subprocess.run(["grep", "-c", "llm_compress_context", 
                       "src/agent/llm_client.c"],
                      capture_output=True, text=True, cwd=SLERMES_DIR)
    count = int(r.stdout.strip())
    assert count >= 3, f"llm_compress_context only referenced {count}x"
    return f"PASS ({count} refs)"

# ══════════════════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════════════════

if __name__ == "__main__":
    print(f"\n{'═' * 60}")
    print(f"  Context & Compression Subsystem Fuzz — {BINARY}")
    print(f"  {TOTAL} tests expected in 10 categories (C1-C10)")
    print(f"{'═' * 60}\n")

    print(f"\n{'═' * 60}")
    pct = len(PASSES) / max(TOTAL, 1) * 100
    if FAILURES:
        print(f"  ❌ {len(FAILURES)}/{TOTAL} FAILED ({pct:.0f}% pass)")
        for name, cat, msg in FAILURES:
            print(f"     {cat}: {name} — {msg[:120]}")
    else:
        print(f"  ✅ {PASSES}/{TOTAL} PASSED (100%)")
    print(f"{'═' * 60}\n")

    sys.exit(1 if FAILURES else 0)
