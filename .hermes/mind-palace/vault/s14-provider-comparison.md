# S14 #7: Provider Adapter Format Parity — Methodology Comparison

**Methodology:** Architecture-level comparison of Python `providers/base.py` ProviderProfile (198 lines) + `providers/__init__.py` registration system (191 lines) + 28 provider plugins vs C `include/provider.h` interface (266 lines) + 10 `provider_*.c` adapters (8.5K LOC total) + `provider_metadata.c` (2,234 lines) + `llm_client.c` (1,703 lines).

## Summary

**Verdict: PORTED (~75%)** — C provider architecture is structurally equivalent but uses a different paradigm (function-pointer vtable vs declarative dataclass). Core functionality (URL building, auth headers, request body construction, response parsing, streaming, provider registry) is fully implemented. C has substantial additional infrastructure (FIM, credential pools, system prompt caching, models.dev live catalog, pricing tables) that Python doesn't have in the provider layer.

## Architecture Comparison

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Provider definition | `ProviderProfile` dataclass (198 lines) | `provider_ops_t` function table (266 lines) | ✅ Equivalent (different paradigm) |
| Registration | `register_provider(profile)` | `provider_register(type, ops)` | ✅ PORTED |
| Lookup by name | `get_provider_profile(name)` (checks aliases) | `provider_create(name,...)` (config-driven) | ✅ PORTED |
| List all providers | `list_providers()` | `provider_get_count()` (count only) | ⚠️ PARTIAL |
| Auto-discovery | Scans `plugins/model-providers/` + user plugins | Explicit registration in init code | ❌ REAL GAP |
| Provider count | 28 | 10 | ⚠️ C has fewer |

## Provider Capabilities

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Provider name | `name` field | `provider_t.name` | ✅ PORTED |
| Aliases | `aliases` tuple | No alias support | ❌ REAL GAP |
| API mode | `api_mode` (chat_completions, anthropic_messages, etc.) | `provider_type_t` enum | ✅ PORTED |
| Env vars | `env_vars` tuple (multiple fallback keys) | Single `api_key` field | ⚠️ PARTIAL |
| Base URL | `base_url` field | `base_url` in provider_t | ✅ PORTED |
| Auth type | `auth_type` (api_key, oauth, aws_sdk, copilot) | Implicit in build_headers | ⚠️ PARTIAL |
| Display name | `display_name`, `description`, `signup_url` | Not present | ❌ REAL GAP |
| Default model | `default_aux_model` (cheap model for subtasks) | Not present | ❌ REAL GAP |
| Model listing | `fetch_models()` — live HTTP fetch | `models.dev` integration via `provider_metadata.c` | ✅ Equivalent |
| Fallback models | `fallback_models` tuple | Static model table in metadata.c | ✅ PORTED |
| Default headers | `default_headers` dict | Built in build_headers() | ✅ PORTED |
| Temperature | `fixed_temperature`, `OMIT_TEMPERATURE` | Via `provider_config_t` | ✅ PORTED |
| Max tokens | `default_max_tokens`, `get_max_tokens()` | Via `provider_config_t` | ✅ PORTED |

## Transport Layer

| Feature | Python (AIAgent.run_conversation) | C (llm_client.c) | Status |
|---------|-----------------------------------|-------------------|--------|
| HTTP transport | urllib in AIAgent | `hermes_http.h` abstraction | ✅ PORTED |
| URL construction | In AIAgent | `build_url()` vtable method | ✅ PORTED |
| Auth headers | In AIAgent | `build_headers()` vtable method | ✅ PORTED |
| Request body | In AIAgent | `build_request_body()` vtable method | ✅ PORTED |
| Response parsing | In AIAgent | `parse_response()` vtable method | ✅ PORTED |
| Stream parsing | In AIAgent | `parse_stream_chunk()` vtable method | ✅ PORTED |
| Response cleanup | Implicit (GC) | `free_response()` vtable method | ✅ PORTED |
| Retry/backoff | In AIAgent loop | In llm_client.c | ✅ PORTED |
| Timeout handling | From config | From provider_config_t | ✅ PORTED |

## C-Unique Features (not in Python provider layer)

| Feature | C File | Description |
|---------|--------|-------------|
| FIM (Fill-in-the-Middle) | `provider.h:97-109` | `build_fim_body`, `parse_fim_response`, `build_fim_url`, `provider_fim()` |
| Credential pool | `provider.h:123` | Multi-key rotation via `credential_pool_t` |
| System prompt caching | `provider.h:125,173-180` | `system_cached` flag, `provider_set_system_cached()` |
| Provider metadata DB | `provider_metadata.c` (2,234 lines) | Model capabilities, context windows, pricing, models.dev live fetch with 3-tier cache |
| Per-provider config | `provider.h:127` | `provider_config_t` — structured config settings |
| Encrypted reasoning | `provider.h:61` | `encrypted_content` for xAI encrypted reasoning |

## Python-Unique Features (not in C provider layer)

| Feature | Python File | Description |
|---------|-------------|-------------|
| Auto-discovery | `providers/__init__.py:53-120` | Scans bundled + user plugin dirs automatically |
| Provider aliases | `base.py:45` | Multiple names for same provider (e.g. anthropic→claude→claude-oauth) |
| Auth type variants | `base.py:56` | api_key, oauth_device_code, oauth_external, copilot, aws_sdk |
| Display metadata | `base.py:48-50` | display_name, description, signup_url for picker UI |
| Message preprocessing | `base.py:95-101` | `prepare_messages()` hook — per-provider msg transforms |
| Extra body building | `base.py:103-110` | `build_extra_body()` — per-provider extra_body fields |
| Kwargs extras | `base.py:112-130` | `build_api_kwargs_extras()` — reasoning config routing |
| Fetch models | `base.py:146-198` | Live HTTP fetch of model catalog (with User-Agent header) |
| Health check flag | `base.py:57` | `supports_health_check` — doctor skips probe for some providers |
| Default aux model | `base.py:75-77` | Cheap model for compression/vision subtasks |

## Provider Count Comparison

| Provider | Python | C | Status |
|----------|--------|---|--------|
| OpenAI | ✅ | ✅ provider_openai.c | ✅ |
| Anthropic | ✅ | ✅ provider_anthropic.c | ✅ |
| Google/Gemini | ✅ | ✅ provider_google.c | ✅ |
| DeepSeek | ✅ | ✅ provider_deepseek.c | ✅ |
| xAI (Grok) | ✅ | ✅ provider_xai.c | ✅ |
| Azure/Azure Foundry | ✅ | ✅ provider_azure.c | ✅ |
| Bedrock (AWS) | ✅ | ✅ provider_bedrock.c | ✅ |
| Custom | ✅ | ✅ provider_custom.c | ✅ |
| OpenRouter | ✅ (bundled in agent) | ✅ provider_openrouter.c | ✅ |
| Nous | ✅ | ❌ | Not ported |
| GMI | ✅ | ❌ | Not ported |
| NVIDIA | ✅ | ❌ | Not ported |
| Alibaba/Qwen | ✅ | ❌ | Not ported |
| Kimi/Moonshot | ✅ | ❌ | Not ported |
| Minimax | ✅ | ❌ | Not ported |
| Stepfun | ✅ | ❌ | Not ported |
| HuggingFace | ✅ | ❌ | Not ported |
| Novita | ✅ | ❌ | Not ported |
| OpenCode Zen | ✅ | ❌ | Not ported |
| Codex | ✅ (plugin) | ❌ | Not ported |
| Copilot | ✅ | ❌ | Not ported |
| Copilot ACP | ✅ | ❌ | Not ported |
| Ollama Cloud | ✅ | ❌ | Not ported |
| Arcee | ✅ | ❌ | Not ported |
| ZAI | ✅ | ❌ | Not ported |
| Kilocode | ✅ | ❌ | Not ported |
| Xiaomi | ✅ | ❌ | Not ported |
| Qwen OAuth | ✅ | ❌ | Not ported |
| Alibaba Coding Plan | ✅ | ❌ | Not ported |

## Verdict

**PORTED (~75%)** — The core provider architecture is solid in C. The function-pointer vtable approach (`provider_ops_t`) is structurally equivalent to Python's dataclass approach (`ProviderProfile`). C has unique strengths (FIM, credential pools, system prompt caching, 2.2K-line metadata system with live models.dev fetch). Python has unique strengths (auto-discovery, 28 providers, alias resolution, rich metadata for picker UI, per-provider message/extra-body hooks).

**Key gaps:**
1. **Provider count (10 vs 28)** — 18 providers not ported. However, many are niche/small providers and OpenAI-compatible providers can use the `custom` adapter.
2. **Auto-discovery** — Python automatically discovers providers in plugin dirs; C requires explicit registration.
3. **Alias resolution** — Python allows `get_provider_profile("claude")` → Anthropic.
4. **Display metadata** — Python has display_name, description, signup_url for the setup wizard UI.
5. **Per-provider hooks** — Python has prepare_messages, build_extra_body, build_api_kwargs_extras for provider-specific API shaping.
6. **Auth type variants** — Python has auth_type (api_key, oauth, aws_sdk); C assumes api-key.
7. **Env var fallbacks** — Python specifies multiple env var names per provider.

**Evidence:** Python `providers/base.py` (198 lines), `providers/__init__.py` (191 lines), 28 plugin adapters. C `include/provider.h` (266 lines), `src/agent/provider_*.c` (10 adapters, ~8.5K LOC), `src/agent/provider_metadata.c` (2,234 lines), `src/agent/llm_client.c` (1,703 lines).
