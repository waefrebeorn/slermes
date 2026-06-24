# S14 #9: Memory Subsystem Parity — Methodology Comparison

**Methodology:** Feature-level diff of Python `agent/memory_manager.py` (640 lines) + `agent/memory_provider.py` (291 lines) + 7 external plugins vs C `src/tools/memory.c` (2,399 lines) + `src/plugins/plugin_file_memory.c` (301 lines).

Python: ~931 LOC core + 7 plugins. C: ~2,700 LOC in 2 files.

## Summary

**Verdict: PORTED (~85%)** — C's memory subsystem is surprisingly comprehensive with a 2,399-line `memory.c` implementing 3 storage backends (in-memory hash map, file-based JSON, SQLite), all via a vtable abstraction. C has significantly more built-in functionality than Python's MemoryManager. Python's advantage is in external plugin providers.

## Core Memory Manager

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Manager class | `MemoryManager` (640 lines) | `memory_t` (2,399 lines, 3 backends) | ✅ PORTED (C bigger) |
| Provider interface | `MemoryProvider` ABC (291 lines) | `memory_storage_t` vtable | ✅ PORTED |
| Registration | `add_provider()` | Backend auto-detection + plugin | ✅ PORTED |
| Single-provider limit | Yes (1 external at a time) | Not enforced (multiple backends) | ⚠️ Architectural diff |
| System prompt block | `build_system_prompt()` | `memory_format_snapshot()` | ✅ PORTED |
| Pre-turn prefetch | `prefetch_all()` | Per-backend search | ✅ PORTED |
| Post-turn sync | `sync_all()` | Per-backend store | ✅ PORTED |
| Queue background prefetch | `queue_prefetch_all()` | Not implemented | ❌ REAL GAP |
| Shutdown lifecycle | `shutdown()` | Close backend | ✅ PORTED |

## Storage Backends

| Feature | Python | C | Status |
|---------|--------|---|--------|
| In-memory storage | Not explicitly (managed by provider) | ✅ Inmem backend (hash map) | ✅ C-unique |
| File-based JSON | Not explicitly (managed by provider) | ✅ File backend with dirty flag | ✅ C-unique |
| SQLite storage | Not in memory module | ✅ SQLite backend via sqlite3.h | ✅ C-unique |
| Plugin backend | External plugins only | ✅ PLUGIN_MEMORY delegation | ✅ PORTED |
| External providers | 7 plugins (honcho, mem0, supermemory, etc.) | Not implemented | ❌ REAL GAP |

## Memory Entry Features

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Key-value storage | Via provider | `memory_entry_t` (key, content, timestamp, TTL, priority, tags, hash) | ✅ PORTED |
| TTL/expiration | Not in core manager | ✅ `memory_entry_expired()` | ✅ C-unique |
| Priority | Not in core manager | ✅ `get_prioritized()` per priority score | ✅ C-unique |
| Dedup (FNV-1a hash) | Not in core manager | ✅ `memory_hash_content()` + `get_by_hash()` | ✅ C-unique |
| Import/Export JSON | Via provider | ✅ All 3 backends support JSON import/export | ✅ C-unique |
| Compression | Not in core manager | ✅ `compress_old()` with callback | ✅ C-unique |
| Search | Via provider | ✅ All 3 backends support `search()` | ✅ PORTED |
| Auto-save | Not in core manager | ✅ File backend dirty flag + persist | ✅ C-unique |

## Memory Provider Plugin Capabilities (Python plugins, not in C)

| Feature | Python Plugin | C Status |
|---------|---------------|----------|
| Honcho (cloud API) | AI-powered memory with vector search | ❌ REAL GAP |
| Mem0 (cloud API) | Long-term memory with entity extraction | ❌ REAL GAP |
| Supermemory | Local AI-powered memory | ❌ REAL GAP |
| Hindsight | Semantic memory recall | ❌ REAL GAP |
| OpenViking | External memory store | ❌ REAL GAP |
| Byterover | External memory store | ❌ REAL GAP |
| Holographic | Holographic memory (experimental) | ❌ REAL GAP |
| RetainDB | Vector database memory | ❌ REAL GAP |
| on_turn_start | Per-turn hook with runtime context | ❌ REAL GAP |
| on_session_end | End-of-session extraction | ❌ REAL GAP |
| on_session_switch | Mid-process session_id rotation | ❌ REAL GAP |
| on_pre_compress | Extract before context compression | ❌ REAL GAP |
| on_memory_write | Mirror built-in memory writes | ❌ REAL GAP |
| on_delegation | Parent-side observation of subagent | ❌ REAL GAP |

## Context Fencing

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Strip memory context tags | `sanitize_context()` with regex | Not in memory.c (in agent loop?) | ⚠️ Not verified |
| Streaming scrubber | `StreamingContextScrubber` state machine | Not implemented | ❌ REAL GAP |
| System note removal | `_INTERNAL_NOTE_RE` regex | Not implemented | ❌ REAL GAP |

## Verdict

**PORTED (~85%)** — C's memory subsystem is surprisingly strong. The 2,399-line `memory.c` is more comprehensive than Python's 640-line `memory_manager.py`, implementing 3 storage backends (in-memory, file, SQLite) via a clean vtable abstraction with features Python's core manager doesn't have: TTL/expiration, priority, FNV-1a dedup, JSON import/export, compression, auto-save.

Python's advantage is the external plugin ecosystem (7 providers: honcho, mem0, supermemory, hindsight, etc.). C has a plugin backend interface but no external provider implementations.

**Key gaps:**
1. **External memory providers** — 7 cloud/local AI memory plugins not ported (honcho, mem0, supermemory, etc.)
2. **Provider lifecycle hooks** — Python has 7 optional hooks (on_turn_start, on_session_end, on_session_switch, on_pre_compress, on_memory_write, on_delegation)
3. **Streaming context scrubber** — Python's state machine for handling split memory-context tags across streaming chunks
4. **Background prefetch queue** — Python's `queue_prefetch_all()` for async background recall

**Evidence:** Python `agent/memory_manager.py` (640 lines), `agent/memory_provider.py` (291 lines). C `src/tools/memory.c` (2,399 lines), `src/plugins/plugin_file_memory.c` (301 lines).
