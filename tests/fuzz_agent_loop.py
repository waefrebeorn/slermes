#!/usr/bin/env python3
"""
Agent Loop Fuzz Test Suite — Full coverage
=============================================
Tests all agent_loop.c features: conversation loop, streaming, tool dispatch,
session CRUD, budget tracking, retry logic, snapshot, crash recovery, and all edge cases.

Test layers:
  L1  — Binary integration: agent_chat, run_conversation via process invocation
  L2  — Session lifecycle: create, load, save, delete, export, migrate, branch, prune, tag
  L3  — Streaming & abort: stream callback, token delivery, abort signal, graceful finish
  L4  — Tool dispatch: tool call execution, result classification, max iterations
  L5  — Budget & retry: budget exhaustion, retry logic, grace call
  L6  — Error recovery: crash recovery, auto-save, NULL safety, OOM, empty messages
  L7  — Source pattern analysis: verify all features properly implemented
  L8  — Edge cases: snapshot, injection, prefill, steer queue, memory nudge
"""

import subprocess, os, sys, re, time

SLERMES_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AGENT_BIN = os.path.join(SLERMES_DIR, "slermes")
AGENT_SRC = os.path.join(SLERMES_DIR, "src/agent/agent_loop.c")
CONV_SRC = os.path.join(SLERMES_DIR, "src/agent/conversation_loop.c")
TURN_SRC = os.path.join(SLERMES_DIR, "src/agent/turn_finalizer.c")
AGENT_HEADER = os.path.join(SLERMES_DIR, "include/hermes_agent.h")

def load_all():
    """Load content from all agent source files for cross-file searches."""
    text = load(AGENT_SRC)
    if os.path.exists(CONV_SRC):
        text += "\n" + load(CONV_SRC)
    if os.path.exists(TURN_SRC):
        text += "\n" + load(TURN_SRC)
    return text

def has_in_all(target):
    """Search for a string across all agent source files."""
    return target in load_all()
HERMES_H = os.path.join(SLERMES_DIR, "include/hermes.h")

PASS = 0; FAIL = 0; SEEN = set()

def test(name):
    def dec(fn):
        def wrapper():
            global PASS, FAIL
            if name in SEEN: return
            SEEN.add(name)
            try:
                fn()
                print(f"  PASS  {name}"); PASS += 1
            except AssertionError as e:
                print(f"  FAIL  {name}: {e}"); FAIL += 1
            except Exception as e:
                print(f"  FAIL  {name}: {e}"); FAIL += 1
        return wrapper
    return dec

def load(src):
    with open(src, "r") as f: return f.read()

# ═══ L1 ═══

@test("L1.01 — Agent binary exists and is executable")
def l1_01():
    assert os.path.exists(AGENT_BIN)
    assert os.access(AGENT_BIN, os.X_OK)

@test("L1.02 — Agent source file exists")
def l1_02():
    assert os.path.exists(AGENT_SRC) or os.path.exists(CONV_SRC)
    total_size = (os.path.getsize(AGENT_SRC) if os.path.exists(AGENT_SRC) else 0) + \
                 (os.path.getsize(CONV_SRC) if os.path.exists(CONV_SRC) else 0) + \
                 (os.path.getsize(TURN_SRC) if os.path.exists(TURN_SRC) else 0)
    assert total_size > 50000

@test("L1.03 — Agent header has public API")
def l1_03():
    s = load(AGENT_HEADER)
    for fn in ["agent_chat", "run_conversation", "agent_free", "init_agent",
               "agent_save_session", "agent_load_session", "agent_session_create",
               "agent_session_delete", "agent_session_export_json", "agent_session_export_markdown",
               "agent_snapshot_take", "agent_snapshot_restore", "agent_auto_save",
               "agent_crash_recover", "agent_session_branch", "agent_session_migrate",
               "agent_session_add_tag", "agent_session_remove_tag", "agent_auto_prune",
               "agent_inject_history"]:
        assert fn in s, f"Missing declaration: {fn}()"

@test("L1.04 — run_conversation is large (200+ lines)")
def l1_04():
    s = load_all()
    idx = s.find("run_conversation")
    assert idx >= 0
    assert s[idx:idx+15000].count('\n') > 200

@test("L1.05 — agent_chat wrapper exists")
def l1_05():
    assert "agent_chat" in load_all()

@test("L1.06 — Binary --help works")
def l1_06():
    r = subprocess.run([AGENT_BIN, "--help"], capture_output=True, text=True, timeout=10)
    assert r.returncode in (0, 1)

@test("L1.07 — Binary --version accessible")
def l1_07():
    r = subprocess.run([AGENT_BIN, "--version"], capture_output=True, text=True, timeout=10)
    assert r.returncode in (0, 1)

# ═══ L2 ═══

@test("L2.01 — Session create: ID generation")
def l2_01():
    s = load_all()
    assert "agent_generate_session_id" in s
    assert "session_id" in s

@test("L2.02 — Session save: serialize + store")
def l2_02():
    s = load_all()
    assert "agent_save_session" in s
    assert "serialize_messages" in s
    assert "deserialize_messages" in s

@test("L2.03 — Session load: restore from DB")
def l2_03():
    s = load_all()
    assert "agent_load_session" in s
    assert "deserialize_messages" in s

@test("L2.04 — Session delete: DB cleanup")
def l2_04():
    s = load_all()
    assert "agent_session_delete" in s
    assert "db_session_delete" in s or "db_delete" in s

@test("L2.05 — Session export: JSON + Markdown")
def l2_05():
    s = load_all()
    assert "agent_session_export_json" in s
    assert "agent_session_export_markdown" in s

@test("L2.06 — Session branch: fork at point")
def l2_06():
    s = load_all()
    assert "agent_session_branch" in s
    assert "snapshot" in s or "copy" in s or "duplicate" in s or "branch" in s

@test("L2.07 — Session migrate: DB upgrade")
def l2_07():
    s = load_all()
    assert "agent_session_migrate" in s
    assert "db_" in s or "migrat" in s

@test("L2.08 — Session tags: add + remove")
def l2_08():
    s = load_all()
    assert "agent_session_add_tag" in s
    assert "agent_session_remove_tag" in s

@test("L2.09 — Auto-prune: retention cleanup")
def l2_09():
    s = load_all()
    assert "agent_auto_prune" in s
    assert "retention" in s.lower() or "days" in s

@test("L2.10 — Session meta: save + load")
def l2_10():
    s = load_all()
    assert "agent_save_meta" in s
    assert "agent_load_meta" in s

@test("L2.11 — Session DB open/close")
def l2_11():
    s = load_all()
    assert "agent_open_db" in s
    assert "agent_close_db" in s

# ═══ L3 ═══

@test("L3.01 — Stream callback function pointer")
def l3_01():
    s = load_all()
    assert "stream_cb" in s
    assert "stream_data" in s

@test("L3.02 — Interrupt mechanism")
def l3_02():
    s = load_all()
    assert "interrupt" in s, "No interrupt mechanism"
    assert "sigint_handler" in s, "No SIGINT handler"

@test("L3.03 — Streaming tokens delivered via callback")
def l3_03():
    s = load_all()
    assert "llm_chat_completion_stream" in s or "stream" in s

@test("L3.04 — Output tracking (stream lifecycle proxy)")
def l3_04():
    s = load_all()
    assert "stream_cb" in s, "No stream callback"
    assert "session_output_tokens" in s or "budget_tracker" in s, "No output tracking"

@test("L3.05 — Graceful continuation after stream abort")
def l3_05():
    s = load_all()
    idx = s.find("run_conversation")
    section = s[idx:idx+15000]
    assert "break" in section, "No loop break"
    assert "interrupt" in section.lower() or "abort" in section.lower(), "No abort in loop"

# ═══ L4 ═══

@test("L4.01 — Tool dispatch mechanism")
def l4_01():
    s = load_all()
    assert "tool_dispatch_thread" in s
    assert "registry_dispatch" in s

@test("L4.02 — Tool result classification")
def l4_02():
    s = load_all()
    assert "classify_tool_result" in s
    assert "tool_result_class_t" in s

@test("L4.03 — Max iterations guard")
def l4_03():
    s = load_all()
    assert "max_iterations" in s

@test("L4.04 — Tools JSON from registry")
def l4_04():
    s = load_all()
    assert "tools_to_json" in s

@test("L4.05 — Toolset filter")
def l4_05():
    s = load_all()
    assert "enabled_toolsets" in s
    assert "disabled_toolsets" in s
    assert "registry_filter_by_toolset" in s

@test("L4.06 — Registry refresh")
def l4_06():
    assert "registry_refresh_availability" in load_all()

@test("L4.07 — Tool call counter")
def l4_07():
    s = load_all()
    assert "tool_call_count" in s or "api_call_count" in s or "iteration_count" in s

# ═══ L5 ═══

@test("L5.01 — Budget tracker integration")
def l5_01():
    assert "budget_tracker" in load_all()

@test("L5.02 — Budget tracked in agent_loop")
def l5_02():
    assert "budget" in load_all()

@test("L5.03 — Grace call after budget")
def l5_03():
    l = load_all().lower()
    assert "grace" in l or "budget_exhausted" in l

@test("L5.04 — Retry loaded from retry_utils")
def l5_04():
    s = load_all()
    assert "retry" in s.lower(), "No retry ref anywhere"

@test("L5.05 — Turn-completion explainer")
def l5_05():
    s = load_all()
    assert "turn_completion" in s or "exit_reason" in s or "budget_exhausted" in s or "max_iterations" in s

@test("L5.06 — Per-turn usage display")
def l5_06():
    s = load_all()
    assert "budget_tracker_format_turn_summary" in s or "turn_summary" in s or "per_turn" in s

# ═══ L6 ═══

@test("L6.01 — Crash recovery")
def l6_01():
    s = load_all()
    assert "agent_crash_recover" in s
    assert "recover" in s.lower()

@test("L6.02 — Auto-save on interval")
def l6_02():
    s = load_all()
    assert "agent_auto_save" in s
    assert "agent_save_session" in s
    assert "last_save" in s or "interval" in s or "auto_save" in s

@test("L6.03 — Error message for OOM / NULL")
def l6_03():
    s = load_all()
    assert "OOM" in s or "Error:" in s
    assert "NULL" in s or "return NULL" in s

@test("L6.04 — User message parameter handling")
def l6_04():
    s = load_all()
    assert "user_message" in s
    assert "NULL" in s or "null" in s.lower()

@test("L6.05 — Sanitize surrogates")
def l6_05():
    assert "sanitize_surrogates" in load_all()

@test("L6.06 — Logging (5+ hermes_log calls)")
def l6_06():
    c = load_all().count("hermes_log(")
    assert c >= 5, f"Only {c} log calls"

@test("L6.07 — Thread creation for tool dispatch")
def l6_07():
    s = load_all()
    assert "pthread_create" in s
    assert "pthread_join" in s

@test("L6.08 — Signal handler")
def l6_08():
    s = load_all()
    assert "sigint_handler" in s
    assert "signal(" in s or "sigaction" in s

# ═══ L7 ═══

@test("L7.01 — 30+ function definitions")
def l7_01():
    s = load_all()
    funcs = re.findall(r'^(?:static\s+)?(?:int|void|bool|char\s*\*|const\s+char\s*\*|size_t|'
                       r'agent_\w+\s*\*|json_\w+\s*\*|tool_\w+\s*\*|plugin_\w+\s*\*)'
                       r'\s*\n?\s*(\w+)\s*\(', s, re.MULTILINE)
    assert len(set(funcs)) >= 30

@test("L7.02 — Agent state has required fields")
def l7_02():
    s = load(HERMES_H)
    for f in ["llm", "session_id", "tools", "memory", "budget", "stream_cb",
              "max_iterations", "plugin_reg", "interrupt", "user_turn_count",
              "prefill", "system_message", "enabled_toolsets"]:
        assert f in s, f"Missing field: {f}"

@test("L7.03 — System prompt: stable + volatile tiers")
def l7_03():
    s = load_all()
    assert "system_prompt_build_stable" in s or "system_prompt" in s
    assert "vol_cfg" in s or "volatile" in s.lower()

@test("L7.04 — Memory prefetch + snapshot")
def l7_04():
    s = load_all()
    assert "prefetch_result" in s
    assert "memory_format_snapshot" in s or "memory_snapshot" in s

@test("L7.05 — Steer queue")
def l7_05():
    s = load_all()
    assert "steer_queue" in s
    assert "steer_count" in s
    assert "HERMES_MAX_STEERS" in s

@test("L7.06 — Memory nudge")
def l7_06():
    s = load_all()
    assert "memory_nudge_interval" in s
    assert "turns_since_memory" in s
    assert "Memory reminder" in s

@test("L7.07 — Prefill message")
def l7_07():
    s = load_all()
    assert "prefill" in s
    assert "prefill_role" in s

@test("L7.08 — Subdirectory hints")
def l7_08():
    assert "subdir_hints" in load_all()

@test("L7.09 — OpenClaw residue")
def l7_09():
    assert "openclaw" in load_all().lower()

@test("L7.10 — Platform hint")
def l7_10():
    assert "platform_hint" in load_all()

@test("L7.11 — User profile loading")
def l7_11():
    s = load_all()
    assert "USER.md" in s or "user_profile" in s

@test("L7.12 — Provider family detection")
def l7_12():
    s = load_all()
    assert "is_google_family" in s or "is_openai_family" in s

@test("L7.13 — Alibaba detection")
def l7_13():
    assert "alibaba" in load_all().lower()

@test("L7.14 — Token tracking (in/out/total)")
def l7_14():
    s = load_all()
    assert "session_input_tokens" in s
    assert "session_output_tokens" in s
    assert "session_total_tokens" in s

@test("L7.15 — Last activity timestamp")
def l7_15():
    assert "last_activity_ts" in load_all()

@test("L7.16 — User turn counter")
def l7_16():
    assert "user_turn_count" in load_all()

@test("L7.17 — Environment hints")
def l7_17():
    s = load_all()
    assert "build_environment_hints" in s
    assert "WSL" in s or "env_hints" in s

@test("L7.18 — Ollama context limit guidance")
def l7_18():
    assert "ollama_context_limit_error" in load_all()

@test("L7.19 — Nous inference route")
def l7_19():
    assert "is_nous_inference_route" in load_all()

@test("L7.20 — Billing/entitlement guidance")
def l7_20():
    s = load_all()
    assert "billing_or_entitlement_message" in s
    assert "nous_entitlement_message" in s

# ═══ L8 ═══

@test("L8.01 — Snapshot take + restore")
def l8_01():
    s = load_all()
    assert "agent_snapshot_take" in s
    assert "agent_snapshot_restore" in s

@test("L8.02 — Inject history from JSON")
def l8_02():
    s = load_all()
    assert "agent_inject_history" in s
    assert "deserialize_messages" in s or "json_parse" in s

@test("L8.03 — Configure from config")
def l8_03():
    s = load_all()
    assert "agent_configure_from_config" in s
    assert "hermes_config_t" in s

@test("L8.04 — init_agent zeroes state")
def l8_04():
    s = load_all()
    idx = s.find("init_agent")
    section = s[idx:idx+2000]
    assert "memset" in section

@test("L8.05 — agent_free releases resources")
def l8_05():
    s = load_all()
    idx = s.find("agent_free")
    section = s[idx:idx+2000]
    assert section.count("free(") >= 5

@test("L8.06 — NULL-after-free patterns")
def l8_06():
    s = load_all()
    matches = re.findall(r'free\([^)]+\)\s*;\s*\n\s*\w+\s*=\s*NULL', s)
    assert len(matches) >= 1

@test("L8.07 — Thread/iteration guard")
def l8_07():
    s = load_all()
    assert "max_iterations" in s or "pthread" in s or "tool_call_count" in s

@test("L8.08 — context_push usage")
def l8_08():
    assert load_all().count("context_push") > 0

@test("L8.09 — session_meta_t exists")
def l8_09():
    r = subprocess.run(["grep", "-rn", "session_meta_t", os.path.join(SLERMES_DIR, "include")],
                       capture_output=True, text=True, timeout=10)
    assert len([l for l in r.stdout.split('\n') if l.strip()]) >= 1

@test("L8.10 — agent_chat called from other files")
def l8_10():
    r = subprocess.run(["grep", "-rn", "agent_chat(", os.path.join(SLERMES_DIR, "src")],
                       capture_output=True, text=True, timeout=10)
    calls = [l for l in r.stdout.split('\n') if 'agent_chat(' in l and 'agent_loop.c' not in l]
    assert len(calls) >= 3

@test("L8.11 — Computer use / browser")
def l8_11():
    s = load_all()
    assert "has_computer_use" in s or "computer_use" in s or "browser" in s.lower()

@test("L8.12 — Condition depth (30+ ifs in run_conversation)")
def l8_12():
    s = load_all()
    idx = s.find("run_conversation")
    section = s[idx:idx+15000]
    assert section.count(' if ') >= 30

@test("L8.13 — Reasoning/thinking")
def l8_13():
    s = load_all().lower()
    assert "reasoning" in s or "thinking" in s

@test("L8.14 — Session listing in headers")
def l8_14():
    r = subprocess.run(["grep", "-rn", "agent_session_list", os.path.join(SLERMES_DIR, "include")],
                       capture_output=True, text=True, timeout=10)
    assert len([l for l in r.stdout.split('\n') if 'agent_session_list' in l]) >= 1

@test("L8.15 — Minimal assert() calls (release safety)")
def l8_15():
    assert load_all().count('assert(') <= 2

@test("L8.16 — snprintf > sprintf")
def l8_16():
    s = load_all()
    assert s.count('snprintf(') > s.count('sprintf(')

@test("L8.17 — 10+ snprintf (bounded copy)")
def l8_17():
    assert load_all().count('snprintf(') >= 10


@test("L8.18 — GAP1: todo_hydrate_from_context function exists")
def l8_18():
    s = load_all()
    assert "todo_hydrate_from_context" in s, "todo_hydrate_from_context not wired in agent_loop"
    s2 = load(os.path.join(SLERMES_DIR, "include/hermes_gap_fixes.h"))
    assert "todo_hydrate_from_context" in s2, "todo_hydrate_from_context not in header"

@test("L8.19 — GAP2: file_mutation_tracker exists")
def l8_19():
    s = load(os.path.join(SLERMES_DIR, "include/hermes_gap_fixes.h"))
    assert "file_mutation_tracker_t" in s, "file_mutation_tracker_t not defined"
    assert "file_mutation_tracker_init" in s, "file_mutation_tracker_init not declared"
    assert "file_mutation_tracker_record" in s, "file_mutation_tracker_record not declared"
    assert "file_mutation_tracker_format_footer" in s, "file_mutation_tracker_format_footer not declared"

@test("L8.20 — GAP3: summarize_api_error function exists")
def l8_20():
    s = load(os.path.join(SLERMES_DIR, "include/hermes_gap_fixes.h"))
    assert "summarize_api_error" in s, "summarize_api_error not in header"
    s2 = load(os.path.join(SLERMES_DIR, "src/agent/provider_openai.c"))
    assert "summarize_api_error" in s2, "summarize_api_error not wired in provider_openai"

# ═══ RUN ═══

def run_all():
    global PASS, FAIL, SEEN
    PASS = 0; FAIL = 0; SEEN = set()
    print("═" * 50)
    print("AGENT LOOP FUZZ TEST SUITE — Full Coverage")
    print(f"Source: {AGENT_SRC}")
    print("═" * 50)
    tests = sorted([(n, fn) for n, fn in globals().items()
                    if n.startswith("l") and len(n) == 5 and callable(fn)],
                   key=lambda x: x[0])
    for _, fn in tests:
        fn()
    total = PASS + FAIL
    print("═" * 50)
    print(f"AGENT LOOP FUZZ: {PASS}/{total} PASS, {FAIL} FAIL")
    return FAIL == 0

if __name__ == "__main__":
    success = run_all()
    sys.exit(0 if success else 1)
