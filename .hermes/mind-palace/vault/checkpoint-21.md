# Checkpoint 21 — EN05 + TR04 + EN01-EN03 VERIFIED

**Gaps closed: 3** (EN05, TR04, EN01-03 verified)
**Reclassified: 4** (EN01, EN02, EN03 from REAL GAP → PORTED; TR04 from PARTIAL → PORTED)

## EN05: Singularity/Apptainer Container Backend

**What:** Container execution backend using apptainer/singularity CLI. Spawn-per-call model with security hardening.

**Python ref:** `tools/environments/singularity.py` (262 lines)
**C impl:** `src/tools/terminal.c` — `run_command_singularity()` (~100 lines)

Features ported:
- Auto-detection: `apptainer` preferred, fallback to `singularity`
- Security flags: `--containall`, `--no-home`, `--writable-tmpfs` (all configurable)
- Configurable container image (default: `docker://ubuntu:22.04`)
- Resource limits: `--memory`, `--cpus`
- Bind mounts: comma-separated `host:container[:ro]` list from config
- Dispatch via `backend=singularity`

Config keys: `terminal.singularity_exe`, `terminal.singularity_image`, `terminal.singularity_containall`, `terminal.singularity_no_home`, `terminal.singularity_writable_tmpfs`, `terminal.singularity_memory`, `terminal.singularity_cpus`, `terminal.singularity_bind`

## TR04: Codex Responses API Transport — Full Enhancement

**What:** Enhanced `provider_codex_responses.c` from PARTIAL to PORTED. Added all issuer-aware session management and request building from Python `codex.py` transport.

**Python ref:** `agent/transports/codex.py` (359 lines)
**C impl:** `src/agent/provider_codex_responses.c` (~490 lines)

Features added:
- **Issuer detection** from `base_url`: xAI (`x.ai`), GitHub (`github`/`copilot`), Codex (`chatgpt.com`/`backend-api/codex`), OpenAI (default)
- **codex_session_state_t** struct: session_id, issuer, replay_encrypted_reasoning, reasoning_effort, reasoning_enabled
- **Per-issuer reasoning**: xAI (effort + encrypted content include), GitHub (pass-through extras), OpenAI/Codex (effort + summary:auto + encrypted content replay), disabled (empty include)
- **Session ID → prompt_cache_key** for non-xAI/non-GitHub issuers
- **Service tier stripping** for xAI (xAI rejects `service_tier` with HTTP 400)
- **Codex backend extra headers**: `X-Session-Id`, `X-Client-Request-Id`
- **xAI extra headers**: `X-Grok-Conv-Id`
- **xAI extra_body**: `prompt_cache_key` for cache routing
- **Instructions extraction** from system message (first MSG_SYSTEM)
- **tools + tool_choice + parallel_tool_calls** when tools present
- **max_tokens → max_output_tokens** conversion for non-Codex backends
- **Input messages**: system messages skipped (already in instructions)

## EN01-EN03: Verified PORTED

Confirmed these backends already existed in `terminal.c` but were incorrectly marked as REAL GAP in prior battleship versions:
- **EN01** (`docker.py`): `src/tools/terminal.c` — `run_command_docker()` with CWD mount, env forwarding, volume mounts
- **EN02** (`ssh.py`): `src/tools/terminal.c` — `run_command_ssh()` with ControlMaster, key auth, port config
- **EN03** (`modal.py`): `src/tools/terminal.c` — `run_command_modal()` with Python wrapper script

## Metrics

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| PORTED | ~149 | ~155 | +6 |
| PARTIAL | ~13 | ~12 | -1 |
| REAL GAP | ~38 | ~34 | -4 |
| Overall | ~71% | ~74% | +3% |
