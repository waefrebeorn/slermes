# Python → C Naming Strategy

**Purpose:** Establish consistent naming conventions for Python→C function translation.
**Source:** Analysis of 50+ "Port of Python" comments across slermes/src/ and slermes/lib/.

---

## Core Principle

**Match Python function names exactly whenever possible.** The C codebase should use the same function names as the Python reference, dropping Python-specific prefixes (like `_` for private) only when C conventions require it.

---

## Naming Patterns (from actual codebase analysis)

### Pattern 1: EXACT MATCH (preferred)
Python name = C name. No transformation.

```
Python: estimate_tokens_rough                    → C: estimate_tokens_rough
Python: is_local_endpoint                        → C: is_local_endpoint
Python: parse_context_limit_from_error           → C: parse_context_limit_from_error
Python: get_next_probe_tier                      → C: get_next_probe_tier
Python: query_ollama_num_ctx                     → C: query_ollama_num_ctx
Python: get_context_length_from_provider_error   → C: get_context_length_from_provider_error
Python: estimate_messages_tokens_rough           → C: estimate_messages_tokens_rough
Python: estimate_request_tokens_rough            → C: estimate_request_tokens_rough
Python: build_context_files_prompt               → C: build_context_files_prompt
Python: build_environment_hints                  → C: build_environment_hints
Python: load_soul_md                             → C: load_soul_md
Python: clear_skills_system_prompt_cache         → C: clear_skills_system_prompt_cache
```

**Rule:** Always try exact match first. This is the most common pattern (~40%).

### Pattern 2: DROP LEADING `_` (private functions)
Python private functions (`_func_name`) drop the underscore in C.

```
Python: _estimate_message_chars                  → C: estimate_message_chars
Python: _looks_like_env_assignment               → C: looks_like_env_assignment
Python: _strip_yaml_frontmatter                  → C: context_strip_frontmatter (also semantic)
Python: _find_git_root                           → C: context_find_git_root (also prefix)
Python: _find_hermes_md                          → C: context_load_hermes_md (also prefix + semantic)
Python: _truncate_content                        → C: context_truncate_content (also prefix)
Python: _scan_context_content                    → C: context_scan_content (also prefix)
Python: _load_agents_md                          → C: context_load_agents_md (also prefix)
Python: _load_claude_md                          → C: context_load_claude_md (also prefix)
Python: _load_cursorrules                        → C: context_load_cursorrules (also prefix)
```

**Rule:** Drop leading `_` from Python private functions. Add `context_` prefix when the function is part of the context file loading subsystem.

### Pattern 3: ADD MODULE PREFIX
C adds a module/domain prefix to avoid namespace collisions.

```
Python: _count_image_tokens                      → C: estimate_count_image_tokens
Python: _is_openrouter_base_url                  → C: provider_is_openrouter_base_url
Python: _extract_first_int                       → C: provider_extract_first_int
Python: _resolve_requests_verify                 → C: provider_resolve_requests_verify
Python: _extract_context_length                  → C: provider_extract_context_length
Python: _extract_max_completion_tokens           → C: provider_extract_max_completion_tokens
Python: _query_ollama_api_show                   → C: provider_query_ollama_api_show
Python: _query_local_context_length              → C: provider_query_local_context_length
Python: _sudo_nopasswd_works                     → C: terminal_sudo_nopasswd_works
Python: _model_supports_tool_use                 → C: bedrock_model_supports_tool_use
Python: _normalize_base_url                      → C: provider_normalize_base_url
Python: _infer_provider_from_url                 → C: provider_infer_from_url
Python: _is_custom_endpoint                      → C: provider_is_custom_endpoint
Python: _is_known_provider_base_url              → C: provider_is_known_base_url
Python: _coerce_reasonable_int                   → C: provider_coerce_reasonable_int
Python: _auth_headers                            → C: provider_auth_headers
Python: _get_context_cache_path                  → C: provider_context_cache_path
Python: _load_context_cache                      → C: provider_context_cache_load
Python: save_context_length                      → C: provider_context_cache_save
Python: get_cached_context_length                → C: provider_context_cache_get
Python: _invalidate_cached_context_length        → C: provider_context_cache_invalidate
Python: _add_model_aliases                       → C: provider_add_model_aliases
Python: _normalize_fallback_ips                  → C: telegram_normalize_fallback_ips
Python: _is_telegram_thread_not_found            → C: telegram_is_thread_not_found
Python: _query_doh_provider                      → C: telegram_query_doh (or telegram_parse_doh_response)
Python: _resolve_system_dns                      → C: telegram_resolve_system_dns
Python: _sanitize_tool_error                     → C: tool_sanitize_error (or tool_error_tags)
Python: grok_supports_reasoning_effort           → C: model_grok_supports_reasoning_effort
```

**Rule:** Add prefix when the function is specific to a module/domain. Most common prefixes:
- `provider_` — Model provider logic (most common, ~15 functions)
- `context_` — Context file loading
- `terminal_` — Terminal/shell tools
- `telegram_` — Telegram gateway
- `bedrock_` — AWS Bedrock adapter
- `model_` — Model metadata
- `tool_` — Tool utilities
- `estimate_` — Token estimation

### Pattern 4: WORD REORDER
Words are reordered to put the domain prefix first.

```
Python: has_aws_credentials                      → C: bedrock_has_credentials
Python: _is_telegram_thread_not_found            → C: telegram_is_thread_not_found
```

**Rule:** Reorder to `module_verb_object` when the Python name starts with a verb.

### Pattern 5: SEMANTIC RENAME (rare)
The C name differs because the implementation approach differs.

```
Python: _build_event                             → C: wecom_callback_user_app_key (different scope)
```

**Rule:** Only when the Python function's purpose doesn't map cleanly to C.

---

## Decision Tree

```
1. Does the Python name work as-is in C?
   YES → Use exact match (Pattern 1)
   NO  → Continue...

2. Is the Python function private (_prefix)?
   YES → Drop the _ prefix → go to step 3
   NO  → go to step 3

3. Does the name collide with existing C symbols or is it too generic?
   YES → Add module prefix (Pattern 3)
   NO  → Continue...

4. Does the word order put verb before noun?
   YES → Reorder to noun_verb or module_noun_verb (Pattern 4)
   NO  → Use the name from step 2
```

---

## Anti-Patterns

1. **DON'T** add `hermes_` prefix to every function — only use domain-specific prefixes
2. **DON'T** use `c_` or `lib_` prefixes — use domain names
3. **DON'T** abbreviate words — `estimate` not `est`, `context` not `ctx`
4. **DON'T** keep Python `_` prefix in C — just drop it
5. **DON'T** rename functions that already match exactly
6. **DON'T** rename inside `#include` directives — header filenames ≠ function names

---

## File-Level Naming

C files should match the Python module name when possible:

```
Python: agent/prompt_builder.py                  → C: src/agent/system_prompt.c (combined with system_prompt.py)
Python: agent/model_metadata.py                  → C: src/agent/provider_metadata.c
Python: agent/prompt_caching.py                  → C: src/agent/prompt_caching.c
Python: agent/trajectory.py                      → C: src/agent/trajectory.c
Python: tools/terminal.py                        → C: src/tools/terminal.c
Python: gateway/platforms/telegram.py            → C: src/gateway/platforms/telegram.c
```

When C filename differs from Python, document the mapping in a comment.

---

## Verification Checklist

After porting a function:
- [ ] Function name matches Python (or follows a documented pattern)
- [ ] "Port of Python module.func_name()" comment precedes the function
- [ ] Header declaration matches the .c file function name
- [ ] No duplicate symbol errors at link time
- [ ] `make -j4` builds clean (0 new errors)
- [ ] `./test_runner.sh` passes 4/4
- [ ] `#include` lines were not corrupted by rename
