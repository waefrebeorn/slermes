#!/usr/bin/env python3
"""
fuzz_all_modules.py — Comprehensive single-pass fuzz for all remaining
Python→C module pairs. v376.

Tests behavioral presence of every Python module in the corresponding
C implementation file(s). Quick triage: finds what's MISSING.

Usage: python3 tests/fuzz_all_modules.py
"""
import os, re, subprocess, sys

WORKDIR = "/home/wubu/hermes-agent-dev/slermes"
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
        detail_str = f" — {detail}" if detail else ""
        print(f"  ✗ {name}{detail_str}")

def c_grep(pattern, *dirs):
    """Grep .c/.h files in dirs for pattern. Returns list of (file, line)."""
    results = []
    for d in dirs:
        if os.path.exists(d):
            r = subprocess.run(
                ['grep', '-rn', '--include=*.c', '--include=*.h', '-E', pattern, d],
                capture_output=True, text=True, timeout=10
            )
            for line in r.stdout.strip().split('\n'):
                if line:
                    results.append(line)
    return results

def file_loc(path):
    return len(open(path).read().splitlines()) if os.path.exists(path) else 0

# ============================================================
# MODULE 1: agent_init.py (1741 LOC) → agent_init.c (68 LOC)
# ============================================================
print("\n═══ M1: agent_init ═══")
ai_c = os.path.join(SRCDIR, "agent", "agent_init.c")

test("agent_init.c exists", os.path.exists(ai_c))
test("init_agent function exists in C",
     len(c_grep('init_agent\s*\(', SRCDIR + '/agent/agent_init.c')) > 0)
test("_build_codex_gpt55_autoraise_notice in C",
     len(c_grep('codex.*gpt55|autoraise.*notice|codex_autoraise', SRCDIR + '/agent')) > 0)
test("_normalized_custom_base_url in C",
     len(c_grep('normalized.*base.*url|normalize.*base', SRCDIR + '/agent')) > 0)
test("_custom_provider_model_matches in C",
     len(c_grep('custom.*provider.*model.*match|model.*match.*custom', SRCDIR + '/agent')) > 0)
test("_merge_custom_provider_extra_body in C",
     len(c_grep('merge.*extra.*body|extra_body.*merge', SRCDIR + '/agent')) > 0)

# ============================================================
# MODULE 2: agent_runtime_helpers.py (2503 LOC → 151 LOC C)
# ============================================================
print("\n═══ M2: agent_runtime_helpers ═══")
arh_c = os.path.join(SRCDIR, "agent", "agent_runtime_helpers.c")

test("agent_runtime_helpers.c exists", os.path.exists(arh_c))
test("sanitize_tool_call_arguments in C",
     len(c_grep('sanitize.*tool.*arg|sanitize_tool', SRCDIR + '/agent')) > 0)
test("strip_think_blocks in C",
     len(c_grep('strip_think|think_scrub', SRCDIR + '/agent')) > 0)
test("drop_thinking_only_and_merge_users in C",
     len(c_grep('drop_thinking|merge.*user|thinking_only', SRCDIR + '/agent')) > 0)
test("restore_primary_runtime in C",
     len(c_grep('restore.*primary.*runtime|restore_runtime|primary_runtime', SRCDIR + '/agent')) > 0)
test("extract_reasoning in C",
     len(c_grep('extract_reasoning|reasoning.*extract', SRCDIR + '/agent')) > 0)
test("switch_model in C",
     len(c_grep('switch_model|switch.*model', SRCDIR + '/agent')) > 0)
test("create_openai_client in C",
     len(c_grep('create.*openai.*client|openai.*init|provider.*create', SRCDIR + '/agent')) > 0)
test("agent_runtime_owns_post_tool_hook in C",
     len(c_grep('post_tool_hook|owns.*hook|hook.*owner', SRCDIR)) > 0,
     "Hook lifecycle ownership")
test("convert_to_trajectory_format in C",
     len(c_grep('trajectory.*format|format.*trajectory|convert.*trajectory', SRCDIR)) > 0,
     "Trajectory format conversion")
test("recover_with_credential_pool in C",
     len(c_grep('credential.*pool.*recover|recover.*credential|pool.*fallback|credential.*fallback', SRCDIR)) > 0,
     "Credential pool recovery after transport failure")

# ============================================================
# MODULE 3: chat_completion_helpers.py (2592 LOC → 315 LOC C)
# ============================================================
print("\n═══ M3: chat_completion_helpers ═══")
cch_c = os.path.join(SRCDIR, "agent", "chat_completion_helpers.c")

test("chat_completion_helpers.c exists", os.path.exists(cch_c))
test("build_api_kwargs in C",
     len(c_grep('build_api_kwargs|api_kwargs|build_kwargs', SRCDIR + '/agent')) > 0)
test("format_messages_for_api in C",
     len(c_grep('format.*message.*api|messages.*for.*api|serialize.*message', SRCDIR + '/agent')) > 0)
test("get_effective_model in C",
     len(c_grep('effective_model|get_effective|resolve_model', SRCDIR + '/agent')) > 0)
test("tool_schema_building in C",
     len(c_grep('tool.*schema|schema.*tool|build.*tool.*schema|format.*tool', SRCDIR + '/agent')) > 0)
test("streaming_response_handler in C",
     len(c_grep('stream.*handler|handle.*stream|stream.*process|streaming_cb', SRCDIR + '/agent')) > 0)
test("build_api_system_message in C",
     len(c_grep('build.*system.*message|system.*message.*build|format.*system', SRCDIR + '/agent')) > 0)

# ============================================================
# MODULE 4: tool_executor.py (1409 LOC → 133 LOC C)
# ============================================================
print("\n═══ M4: tool_executor ═══")
te_c = os.path.join(SRCDIR, "agent", "tool_executor.c")

test("tool_executor.c exists", os.path.exists(te_c))
test("tool_execute in C",
     len(c_grep('tool_execut|execute_tool|execut_tool', SRCDIR + '/agent')) > 0)
test("tool result formatting in C",
     len(c_grep('tool.*result.*format|result.*formatt|format_tool_result', SRCDIR + '/agent')) > 0)
test("error classification per tool call in C",
     len(c_grep('error.*classif|classify.*error', SRCDIR + '/agent') + 
         c_grep('error_classifier', SRCDIR + '/..')) > 0)
test("confirmation/approval prompt integration in C",
     len(c_grep('confirm.*tool|tool.*confirm|approval.*tool|tool.*approval', SRCDIR)) > 0)

# ============================================================
# MODULE 5: context_compressor.py (2182 LOC → context.c)
# ============================================================
print("\n═══ M5: context_compressor / context ═══")
ctx_c = os.path.join(SRCDIR, "agent", "context.c")

test("context.c exists with compression", os.path.exists(ctx_c))
if os.path.exists(ctx_c):
    ctx_loc = file_loc(ctx_c)
    test(f"context.c size ({ctx_loc} LOC) ≥ 50% of Python (2182 LOC)",
         ctx_loc >= 1091, f"C has {ctx_loc} LOC, Python has 2182")
test("compress_messages in C",
     len(c_grep('compress.*message|message.*compress|compress_messages', SRCDIR + '/agent')) > 0)
test("should_compress_context in C",
     len(c_grep('should_compress|compress.*threshold|compress.*trigger', SRCDIR + '/agent')) > 0)
test("get_compression_ratio in C",
     len(c_grep('compression.*ratio|compress.*ratio|get_compress', SRCDIR + '/agent')) > 0)
test("compress_conversation_turn in C",
     len(c_grep('compress.*turn|turn.*compress', SRCDIR + '/agent')) > 0)

# ============================================================
# MODULE 6: insights.py (921 LOC → provider_metadata.c)
# ============================================================
print("\n═══ M6: insights ═══")
pm_c = os.path.join(SRCDIR, "agent", "provider_metadata.c")

test("provider_metadata.c exists", os.path.exists(pm_c))
test("get_model_insights in C",
     len(c_grep('model.*insights|insights.*model|get.*insights', SRCDIR + '/agent/provider_metadata.c')) > 0)
test("collect_usage_stats in C",
     len(c_grep('usage.*stats|collect.*usage|stats.*collect', SRCDIR + '/agent/provider_metadata.c')) > 0)
test("format_insights_output in C",
     len(c_grep('insights.*output|format.*insights|output.*insights', SRCDIR + '/agent/provider_metadata.c')) > 0)
test("aggregate_model_usage in C",
     len(c_grep('aggregate.*usage|model.*usage.*aggregate|usage.*aggregate', SRCDIR + '/agent/provider_metadata.c')) > 0)
test("display_bar_chart in C",
     len(c_grep('bar_chart|display.*bar|draw.*bar', SRCDIR + '/../src')) > 0)

# ============================================================
# MODULE 7: memory_manager.py (857 LOC → memory_provider.c)
# ============================================================
print("\n═══ M7: memory_manager ═══")
mp_c = os.path.join(SRCDIR, "agent", "memory_provider.c")

test("memory_provider.c exists", os.path.exists(mp_c))
test("memory_manager_init in C",
     len(c_grep('memory.*init|memory_manager_init', SRCDIR + '/agent/memory_provider.c')) > 0)
test("memory_manager_load in C",
     len(c_grep('memory.*load|load.*memory', SRCDIR + '/agent/memory_provider.c')) > 0)
test("memory_manager_save in C",
     len(c_grep('memory.*save|save.*memory', SRCDIR + '/agent/memory_provider.c')) > 0)
test("memory_manager_search in C",
     len(c_grep('memory.*search|search.*memory', SRCDIR + '/agent/memory_provider.c')) > 0)
test("memory_manager_delete in C",
     len(c_grep('memory.*delete|delete.*memory', SRCDIR + '/agent/memory_provider.c')) > 0)
test("memory_manager_list in C",
     len(c_grep('memory.*list|list.*memory', SRCDIR + '/agent/memory_provider.c')) > 0)
test("format_for_system_prompt in C",
     len(c_grep('format.*system.*prompt|system_prompt.*memory|memory.*prompt.*format', SRCDIR + '/agent')) > 0)

# ============================================================
# MODULE 8: file_safety.py (640 LOC → file_safety.c has 760 LOC)
# ============================================================
print("\n═══ M8: file_safety ═══")
fs_c = os.path.join(SRCDIR, "agent", "file_safety.c")
test("file_safety.c exists and has content",
     os.path.exists(fs_c) and file_loc(fs_c) >= 400,
     f"C has {file_loc(fs_c) if os.path.exists(fs_c) else 0} LOC")
test("file write validation in C",
     len(c_grep('file.*safety|file.*valid|path.*check|allowed.*path|write.*guard', SRCDIR + '/agent/file_safety.c')) > 0)
test("execution path security in C",
     len(c_grep('path.*security|security.*path|exec.*path|allowed.*exec', SRCDIR)) > 0)

# ============================================================
# MODULE 9: curator.py (1848 LOC → curator.c 873 LOC)
# ============================================================
print("\n═══ M9: curator ═══")
cur_c = os.path.join(SRCDIR, "agent", "curator.c")
test("curator.c exists with content",
     os.path.exists(cur_c) and file_loc(cur_c) >= 500,
     f"C has {file_loc(cur_c) if os.path.exists(cur_c) else 0} LOC")
test("skill pinning/unpinning in C",
     len(c_grep('pin|unpin|curator.*pin', SRCDIR + '/agent/curator.c')) > 0)
test("skill audit in C",
     len(c_grep('audit|skill.*check', SRCDIR + '/agent/curator.c')) > 0)

# ============================================================
# MODULE 10: sanction/security modules
# ============================================================
print("\n═══ M10: security/sanction ═══")
test("sanitize.c exists with content",
     os.path.exists(os.path.join(SRCDIR, "agent", "sanitize.c")) and 
     file_loc(os.path.join(SRCDIR, "agent", "sanitize.c")) >= 200)
test("redact.c exists with content",
     os.path.exists(os.path.join(SRCDIR, "agent", "redact.c")) and
     file_loc(os.path.join(SRCDIR, "agent", "redact.c")) >= 200)
test("think_scrubber.c exists",
     os.path.exists(os.path.join(SRCDIR, "agent", "think_scrubber.c")))
test("tool_guardrails.c exists",
     os.path.exists(os.path.join(SRCDIR, "agent", "tool_guardrails.c")))

print(f"\n{'='*60}")
print(f"RESULTS: {PASS}/{TOTAL} passed, {FAIL} failed")
print(f"{'='*60}")

if FAIL > 0:
    sys.exit(1)
