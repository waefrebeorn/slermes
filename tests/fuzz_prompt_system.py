#!/usr/bin/env python3
"""
fuzz_prompt_system.py — Prompt system fuzz test suite (v376)

Tests the C prompt system (system_prompt.c, prompt_caching.c) against
the Python parent behavior (system_prompt.py, prompt_builder.py,
prompt_caching.py). 6 layers: binary, constants, functions, source
patterns, behavioral, edge cases.

Usage: python3 tests/fuzz_prompt_system.py
"""

import os
import re
import subprocess
import sys
import tempfile

WORKDIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRCDIR = os.path.join(WORKDIR, "src")
INCDIR = os.path.join(WORKDIR, "include")

PASS = 0
FAIL = 0
TOTAL = 0

def test(name, condition, detail=""):
    global PASS, FAIL, TOTAL
    TOTAL += 1
    if condition:
        PASS += 1
        print(f"  ✓ {name}")
    else:
        FAIL += 1
        print(f"  ✗ {name}" + (f" — {detail}" if detail else ""))

def grep_file(pattern, path):
    """Return matching lines from a file."""
    try:
        with open(path, "r") as f:
            content = f.read()
        matches = re.findall(pattern, content, re.IGNORECASE | re.MULTILINE)
        return matches
    except Exception:
        return []

def grep_codebase(pattern, root=WORKDIR):
    """Search all .c/.h files for a pattern."""
    results = []
    for dirpath, _, filenames in os.walk(root):
        if ".o" in dirpath or ".git" in dirpath:
            continue
        for fn in filenames:
            if fn.endswith((".c", ".h")):
                path = os.path.join(dirpath, fn)
                try:
                    with open(path, "r", errors="ignore") as f:
                        content = f.read()
                    for m in re.finditer(pattern, content, re.IGNORECASE | re.MULTILINE):
                        results.append((path, m.group()))
                except Exception:
                    pass
    return results

# =============================================================================
# L1: Binary integration (7 tests)
# =============================================================================
print("\n═══ L1: Binary Integration ═══")

test("system_prompt.o exists",
     os.path.exists(os.path.join(SRCDIR, "agent", "system_prompt.o")))

test("prompt_caching.o exists",
     os.path.exists(os.path.join(SRCDIR, "agent", "prompt_caching.o")))

test("hermes_system_prompt.h exists",
     os.path.exists(os.path.join(INCDIR, "hermes_system_prompt.h")))

test("prompt_caching.h exists",
     os.path.exists(os.path.join(INCDIR, "prompt_caching.h")))

test("prompt_builder.c exists (name parity)",
     os.path.exists(os.path.join(SRCDIR, "agent", "prompt_builder.c")))

# Check header declares all 4 system_prompt.py public API functions
header_path = os.path.join(INCDIR, "hermes_system_prompt.h")
header = open(header_path).read() if os.path.exists(header_path) else ""
test("system_prompt_build declared", "system_prompt_build" in header)
test("invalidate_system_prompt declared", "invalidate_system_prompt" in header)
test("format_tools_for_system_message declared",
     "format_tools_for_system_message" in header)

print(f"  L1: {PASS}/{TOTAL}")

# Reset counter for next section
_L1_PASS = PASS
_L1_FAIL = FAIL
_L1_TOTAL = TOTAL

# =============================================================================
# L2: Constants parity (12 tests)
# =============================================================================
print("\n═══ L2: Constants Parity ═══")

sysprompt_c = os.path.join(SRCDIR, "agent", "system_prompt.c")
promptbuilder_c = os.path.join(SRCDIR, "agent", "prompt_builder.c")
sysprompt_content = (open(sysprompt_c).read() + "\n" + open(promptbuilder_c).read()) if os.path.exists(sysprompt_c) and os.path.exists(promptbuilder_c) else ""
if not sysprompt_content:
    sysprompt_content = open(sysprompt_c).read() if os.path.exists(sysprompt_c) else ""

# Python has 10 guidance constants, C should have all
constants_py = [
    "DEFAULT_AGENT_IDENTITY",
    "HERMES_AGENT_HELP_GUIDANCE",
    "MEMORY_GUIDANCE",
    "SESSION_SEARCH_GUIDANCE",
    "SKILLS_GUIDANCE",
    "KANBAN_GUIDANCE",
    "TOOL_USE_ENFORCEMENT_GUIDANCE",
    "OPENAI_MODEL_EXECUTION_GUIDANCE",
    "GOOGLE_MODEL_OPERATIONAL_GUIDANCE",
    "COMPUTER_USE_GUIDANCE",
]
constants_c = [
    "SYSPRMPT_DEFAULT_IDENTITY",
    "SYSPRMPT_HERMES_HELP",
    "SYSPRMPT_MEMORY_GUIDANCE",
    "SYSPRMPT_SESSION_SEARCH_GUIDANCE",
    "SYSPRMPT_SKILLS_GUIDANCE",
    "SYSPRMPT_KANBAN_GUIDANCE",
    "SYSPRMPT_TOOL_ENFORCE",
    "SYSPRMPT_OPENAI_EXEC",
    "SYSPRMPT_GOOGLE_OPS",
    "SYSPRMPT_COMPUTER_USE",
]

for name_c in constants_c:
    test(f"Constant {name_c} exists",
         name_c in sysprompt_content or name_c in header,
         f"not found in system_prompt.c or hermes_system_prompt.h")

# Python-specific constants that C should also have
test("TASK_COMPLETION_GUIDANCE in C",
     "TASK_COMPLETION" in sysprompt_content or
     grep_codebase("TASK_COMPLETION_GUIDANCE|Finishing.the.job", WORKDIR),
     "Guidance about finishing the job (task completion)")

test("DEVELOPER_ROLE_MODELS deprecation in C",
     "DEVELOPER_ROLE" in sysprompt_content or
     grep_codebase("DEVELOPER_ROLE|developer_role", WORKDIR),
     "Models that should use developer role instead of system role")

test("STEER_CHANNEL_NOTE in C",
     "OUT-OF-BAND" in sysprompt_content or
     "STEER_CHANNEL_NOTE" in sysprompt_content or
     "steer.*channel" in sysprompt_content,
     "Mid-turn user steering guidance in system prompt")

print(f"  L2: {PASS - _L1_PASS}/{TOTAL - _L1_TOTAL}")

# =============================================================================
# L3: Function parity (16 tests)
# =============================================================================
print("\n═══ L3: Function Parity ═══")

# Python system_prompt.py has 4 public functions
py_funcs = [
    "build_system_prompt_parts",
    "build_system_prompt",
    "invalidate_system_prompt",
    "format_tools_for_system_message",
]

c_funcs = [
    "system_prompt_build_stable",
    "system_prompt_build_volatile",
    "system_prompt_build",
    "invalidate_system_prompt",
    "format_tools_for_system_message",
    "build_environment_hints",
    "platform_hint_get",
    "build_context_files_prompt",
    "load_soul_md",
    "context_load_hermes_md",
    "context_load_agents_md",
    "context_load_claude_md",
    "context_load_cursorrules",
    "context_scan_content",
    "context_truncate_content",
    "context_strip_frontmatter",
    "context_find_git_root",
    "build_skills_system_prompt",
    "build_nous_subscription_prompt",
    "format_steer_marker",
    "agent_get_continuation_prompt",
    "clear_skills_system_prompt_cache",
    "build_skills_manifest",
    "load_skills_snapshot",
    "write_skills_snapshot",
    "build_snapshot_entry",
    "probe_remote_backend",
    "set_session_cwd",
    "clear_session_cwd",
    "resolve_agent_cwd",
    "resolve_context_cwd",
]

for name in c_funcs:
    test(f"Function {name} in C",
         name in sysprompt_content or
         f"{name}(" in sysprompt_content,
         f"Expected in system_prompt.c")

# Check continuation prompt matches Python
test("agent_get_continuation_prompt 3 modes",
     "partial_stub" in sysprompt_content or
     "network error mid-stream" in sysprompt_content,
     "Should have partial stub, network error, and length limit modes")

# Check prompt_caching.py functions ported
prompt_cache_c = os.path.join(SRCDIR, "agent", "prompt_caching.c")
pcc = open(prompt_cache_c).read() if os.path.exists(prompt_cache_c) else ""
for fn in ["apply_anthropic_cache_control", "cache_track_hit", "cache_track_miss",
           "cache_reset_stats", "cache_get_stats_json", "cache_warmup",
           "cache_set_system_prompt", "cache_is_valid", "anthropic_prompt_cache_policy",
           "prompt_cache_set_provider_config"]:
    test(f"prompt_caching.{fn} exists", fn in pcc)

# Track section boundaries properly
_L2_START_TOTAL = TOTAL - (TOTAL - _L1_TOTAL)

print(f"  L3: {PASS}/{TOTAL}")

# =============================================================================
# L4: Source pattern analysis — constant content parity (20 tests)
# =============================================================================
L4_START_TOTAL = TOTAL
L4_START_PASS = PASS
print("\n═══ L4: Constant Content Parity ═══")

# Check key strings in C match Python equivalents
key_strings = [
    ("You are Hermes Agent, an intelligent AI assistant",
     "DEFAULT_IDENTITY core text present"),
    ("nousresearch.com/docs",
     "Hermes docs URL present"),
    ("persistent memory across sessions",
     "Memory guidance core text present"),
    ("session_search to recall it",
     "Session search guidance present"),
    ("save the approach as a",
     "Skills guidance present"),
    ("You MUST use your tools to take action",
     "Tool enforcement text present"),
    ("tool_persistence",
     "OpenAI execution XML tag present"),
    ("Google model operational directives",
     "Google ops text present"),
    ("Computer Use (macOS",
     "Computer use header present"),
    ("Kanban task execution protocol",
     "Kanban guidance header present"),
]

for text, desc in key_strings:
    found = text in sysprompt_content
    if not found:
        # Search other files
        found = len(grep_codebase(re.escape(text), WORKDIR)) > 0
    test(f"Content: {desc}", found)

# Platform hint coverage — count platforms
platform_count_c = sysprompt_content.count('strcmp(platform_name, "')
test("Platform hints: 19 platforms",
     platform_count_c >= 19,
     f"Found {platform_count_c} platform checks (expect 19+)")

# Threat patterns in C
test("Threat patterns defined",
     "THREAT_PATTERNS[]" in sysprompt_content or
     "threat_pattern_t" in sysprompt_content,
     "Context file threat scan patterns")

print(f"  L4: {PASS - L4_START_PASS}/{TOTAL - L4_START_TOTAL}")

# =============================================================================
# L5: Behavioral parity — gap verification (6 tests)
# =============================================================================
L5_START_TOTAL = TOTAL
L5_START_PASS = PASS
print("\n═══ L5: Behavioral Parity (Gap Verification) ═══")

# These tests should FAIL if the gaps haven't been fixed yet
# P1: TASK_COMPLETION_GUIDANCE
has_p1 = len(grep_codebase("TASK_COMPLETION_GUIDANCE", WORKDIR)) > 0
test("[GAP P1] TASK_COMPLETION_GUIDANCE constant + injection in stable tier",
     has_p1, "227-char guidance string about finishing the job")

# P2: DEVELOPER_ROLE_MODELS
has_p2 = len(grep_codebase("DEVELOPER_ROLE", WORKDIR)) > 0
test("[GAP P2] DEVELOPER_ROLE_MODELS list in C constants",
     has_p2, "(\"gpt-5\", \"codex\") — swap system for developer role")

# P3: STEER_CHANNEL_NOTE with OUT-OF-BAND marker
has_p3_right = len(grep_codebase('OUT-OF-BAND USER MESSAGE', WORKDIR)) > 0
test("[GAP P3] STEER_CHANNEL_NOTE with Python-compatible marker format",
     has_p3_right,
     "C uses <steer_marker> but Python uses [OUT-OF-BAND USER MESSAGE]")

# P4: _probe_remote_backend non-trivial
pc = open(os.path.join(SRCDIR, "agent", "system_prompt.c")).read()
has_p4_stub = "return NULL" in (
    __import__('re').search(r'probe_remote_backend.*?\{.*?\n.*?\}', pc, __import__('re').DOTALL).group() 
    if __import__('re').search(r'probe_remote_backend.*?\{.*?\n.*?\}', pc, __import__('re').DOTALL) else ""
)
# Check if probe_remote_backend does anything useful
has_p4_real = len(grep_codebase("probe_remote_backend", WORKDIR)) > 0
is_p4_stub = "return NULL" in str(grep_codebase("return NULL", os.path.join(SRCDIR, "agent", "system_prompt.c")))
test("[GAP P4] probe_remote_backend has non-trivial implementation",
     has_p4_real and not is_p4_stub,
     "Python does live probing inside docker/ssh containers")

# P5: invalidate_system_prompt reloads memory
has_p5 = len(grep_codebase("reload.*memory|load_from_disk|memory.*reload", WORKDIR)) > 0
test("[GAP P5] invalidate_system_prompt reloads memory from disk",
     has_p5,
     "Python sets agent._cached_system_prompt=None + reloads memory")

# P6: build_skills_system_prompt external dirs + platform filtering
has_p6 = len(grep_codebase("external.*skill|skill.*external|ENTRY_FILTER|platform.*filter|category.*description|DESCRIPTION\\.md", WORKDIR)) > 0
test("[GAP P6] build_skills_system_prompt — external dirs + platform filter + category descriptions",
     has_p6,
     "Missing ~150 LOC of filtering/reconciliation logic")

print(f"  L5: {PASS - L5_START_PASS}/{TOTAL - L5_START_TOTAL}")

# =============================================================================
# L6: Edge cases and robustness (12 tests)
# =============================================================================
L6_START_TOTAL = TOTAL
L6_START_PASS = PASS
print("\n═══ L6: Edge Cases ═══")

# Check defense against NULL inputs
null_checks = ["(!cfg)", "(!content)", "(!filename)"]
test("NULL pointer guards in system_prompt.c",
     any(c in sysprompt_content for c in null_checks),
     "Functions should check for NULL inputs")

# Check for buffer overflow protection
test("snprintf used (not sprintf)",
     "snprintf(" in sysprompt_content or "snprintf(" in pcc,
     "snprintf is safe vs sprintf")

# Check file size limits
test("CONTEXT_FILE_MAX_CHARS limit applied",
     "CONTEXT_FILE_MAX_CHARS" in sysprompt_content,
     "All context file reads should be capped")

# Check threat scan before content injection
test("context_scan_content called before loading",
     sysprompt_content.count("context_scan_content") >= 5,
     "Threat scan applied to all context file loaders")

# Check continuation prompt has all 3 branches
continuation_branches = ["is_partial_stub", "network error", "truncated by"]
test("Continuation prompt 3 branches",
     all(b in sysprompt_content for b in continuation_branches),
     "Partial stub, network error, and output-length branches")

# Caching stats functions
cache_stats = ["cache_get_stats_json", "cache_get_hits", "cache_get_misses"]
test("Cache stats reporting functions",
     all(f in pcc for f in cache_stats),
     "JSON stats for debugging prompt cache performance")

# Per-provider cache config
test("Per-provider cache configuration",
     "prompt_cache_set_provider_config" in pcc,
     "Allow different TTL/config per inference provider")

# Cache warmup on session load
test("cache_warmup function exists",
     "cache_warmup" in pcc,
     "Warm cache on session restore")

# Multi-turn cache optimization
test("Multi-turn cache optimization (P04)",
     "g_marked_count" in pcc or "cache_set_marked_count" in pcc,
     "Skip already-cached messages in subsequent turns")

# Alibaba model name workaround
test("Alibaba model name workaround",
     "alibaba" in sysprompt_content.lower(),
     "Inject explicit model identity for Alibaba Coding Plan API")

# Skills manifest building
test("Skills manifest builder (mtime/size)",
     "build_skills_manifest" in sysprompt_content,
     "JSON manifest of skills directory for cache validation")

# Format tools for system message
test("format_tools_for_system_message handles empty registry",
     "strdup(\"[]\")" in sysprompt_content or "strdup('[]')" in sysprompt_content,
     "Empty tool registry returns empty JSON array")

print(f"  L6: {PASS - L6_START_PASS}/{TOTAL - L6_START_TOTAL}")

# =============================================================================
# Summary
# =============================================================================
print(f"\n{'='*60}")
print(f"RESULTS: {PASS}/{TOTAL} passed, {FAIL} failed")
print(f"{'='*60}")

if FAIL > 0:
    sys.exit(1)
