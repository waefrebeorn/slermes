# Third-Party Dependencies

Slermes C translation delegates certain Python-specific integrations to external tools and services. These are not ported to C because they depend on Python-specific packages, external vendor APIs, or proprietary Nous infrastructure.

## Python Package Dependencies

### NeuTTS (TD22 — neutts_synth.py)
- **Python file:** `tools/neutts_synth.py` (104 lines)
- **Dependency:** `neutts` Python package (`pip install -U neutts[all]`)
- **System dep:** `espeak-ng` (apt/brew)
- **Purpose:** Neural TTS synthesis via subprocess. Keeps the ~500MB TTS model in a separate process.
- **C equivalent:** `src/tools/tts.c` provides native TTS. For NeuTTS specifically, invoke the Python script as a subprocess:
  ```bash
  python -m tools.neutts_synth --text "Hello" --out output.wav --ref-audio samples/jo.wav --ref-text samples/jo.txt
  ```
- **Status:** Third-party Python package. Not portable to C. Use subprocess invocation if needed.

## Vendor Gateway Dependencies

### Managed Tool Gateway (TD21 — managed_tool_gateway.py)
- **Python file:** `tools/managed_tool_gateway.py` (192 lines)
- **Dependencies:** `hermes_cli.auth`, `tools.tool_backend_helpers`, Nous auth store (`auth.json`)
- **Purpose:** Routes tool calls through Nous-hosted vendor passthrough gateways (Browserbase, etc.). Reads OAuth tokens from auth store, builds vendor gateway URLs.
- **C equivalent:** C code makes direct API calls to providers. No vendor gateway routing needed.
- **Status:** Proprietary Nous infrastructure. Not portable. Direct API calls used instead.

## Python Runtime Dependencies

### Jiter Preload (MS81 — jiter_preload.py)
- **Python file:** `agent/jiter_preload.py` (39 lines)
- **Dependency:** `jiter` native extension (OpenAI SDK dependency)
- **Purpose:** Preloads jiter native extension during agent package import to avoid import-order failures in threaded streaming.
- **C equivalent:** None. C has no Python native extension import system.
- **Status:** Python import system specific. Not applicable to C.

### Async Utils (MS80 — async_utils.py)
- **Python file:** `agent/async_utils.py` (68 lines)
- **Dependency:** Python `asyncio`
- **Purpose:** `safe_schedule_threadsafe()` wraps `asyncio.run_coroutine_threadsafe` to prevent coroutine leaks on scheduling failure.
- **C equivalent:** C uses pthreads with mutex/cond. Thread lifecycle management is handled differently (no coroutine concept).
- **Status:** Python asyncio specific. Not applicable to C.

### Credential Pool (MS84 — credential_pool.py)
- **Python file:** `agent/credential_pool.py` (2183 lines)
- **Dependencies:** `hermes_cli.auth`, `hermes_cli.config`, `agent.credential_persistence`
- **Purpose:** Multi-credential pool with OAuth, failover, cooldowns, persistence. Supports multiple credentials per provider with rotation strategies (fill-first, round-robin, random, least-used).
- **C equivalent:** C code has single-provider credential handling. Multi-credential pool with OAuth rotation is not yet implemented.
- **Status:** **Deferred — should be implemented in C.** This is a large but valuable feature for production use with multiple API keys per provider.

## Non-Existent Files

These files were listed in the battleship but do not exist in the Python codebase:
- TD25 `texteditor_tool.py`
- TD26 `thinking_tool.py`
- TD27 `webhook_tool.py`
- TD28 `whatsapp_tool.py`
- MS82 `agent/compression.py`
- MS83 `agent/caching.py`

## Test Fixtures (Not Portable)

- PL17 `example_dashboard` — Python test fixture only. No real plugin to port.
