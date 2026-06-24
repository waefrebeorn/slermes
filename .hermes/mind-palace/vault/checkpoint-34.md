# Checkpoint 34 — AG20 + AG05 + AG22 + AG26

## What happened
Closed 4 AG-sector REAL GAPs: auto-title session, memory context fencing,
stream diagnostics, and chat completion helpers.

## Gaps Closed

### AG20: auto_title_session — `agent/title_generator.py:auto_title_session()`
- **File:** `src/agent/title.c:77-185`
- **What:** Generate and set session title on first exchange if none exists.
  Checks DB for existing title, generates from first user message, saves via
  pthread background thread (fire-and-forget, daemon).
- **Functions:** `auto_title_session()`, `maybe_auto_title()`
- **Classification:** REAL GAP → PORTED

### AG05: build_memory_context_block — `agent/memory_manager.py:build_memory_context_block()`
- **File:** `src/tools/memory.c:2373-2523`
- **What:** Wrap prefetched memory in `<memory-context>` fenced block with
  system note. Sanitizes provider output (strips existing fence tags,
  system notes). Returns NULL on empty input.
- **Functions:** `sanitize_context()`, `build_memory_context_block()`
- **Classification:** REAL GAP → PORTED

### AG22: stream diagnostics — `agent/stream_diag.py` (5 functions)
- **File:** `src/agent/stream_diag.c` (new)
- **What:** Per-attempt stream diagnostic counters, error chain formatting,
  structured retry logging, user-visible drop status emission.
- **Functions:** `stream_diag_reset()`, `flatten_error_chain()`,
  `log_stream_retry()`, `emit_stream_drop()`
- **Note:** `stream_diag_init()` already existed via `calloc` zero-init in llm_client.c
- **Classification:** REAL GAP → PORTED

### AG26: chat completion helpers — `agent/chat_completion_helpers.py` (7 of 11 functions)
- **File:** `src/agent/chat_completion_helpers.c` (new)
- **What:** Token estimation for API payloads, API kwargs builder, assistant
  message constructor, max-iterations handler, fallback activator, resource cleanup.
- **Functions:** `estimate_request_context_tokens()`, `build_api_kwargs()`,
  `build_assistant_message()`, `handle_max_iterations()`,
  `try_activate_fallback()`, `cleanup_task_resources()`
- **Note:** `interruptible_api_call()` and `interruptible_streaming_api_call()`
  already implemented in agent_loop.c and llm_client.c. `_ra()` is Python-specific N/A.
  `_is_openai_codex_backend()` and `_env_float()` are simple enough to inline at call sites.
- **Classification:** REAL GAP → PORTED (7/11 functions; 2 already in C code, 2 N/A/trivial)

## Files Modified
- `src/agent/title.c` — added auto_title_session + maybe_auto_title
- `src/agent/title.h` — added declarations
- `src/tools/memory.c` — added sanitize_context + build_memory_context_block
- `src/agent/stream_diag.c` — new file (stream diagnostics)
- `src/agent/chat_completion_helpers.c` — new file (chat completion helpers)
- `include/hermes_agent.h` — added declarations for all new functions
- `include/hermes_memory.h` — added sanitize_context + build_memory_context_block declarations
- `Makefile` — added stream_diag.o and chat_completion_helpers.o to AGENT_OBJ

## Build Status
- Clean compile, 0 errors
- 4/4 tests passing
