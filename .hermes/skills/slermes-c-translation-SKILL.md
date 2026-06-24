---
name: slermes-c-translation
description: "Slermes C translation of Hermes Agent — patterns, pitfalls, and conventions for the C codebase in slermes/. Use when working on C source files, adding features, fixing build errors, or porting Python functionality to C."
version: 1.21.0
author: OWL
license: MIT
metadata:
  hermes:
    tags: [c-translation, slermes, porting, build, debugging]
    related_skills: [systematic-debugging]
---

# Slermes C Translation

## Overview

Slermes is a C translation of the Hermes Agent Python codebase. Python reference: `/home/wubu/hermes-agent-dev/agent/`. C code: `slermes/src/` (main) + `slermes/lib/` (libraries).

## Build & Test

```bash
cd slermes && make -j4            # build
cd slermes && ./test_runner.sh    # run tests (4 basic tests)
PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "..."  # commit
```

**Pre-existing build warnings** (NOT new errors):
- `agent_loop.c:865`: incompatible pointer type (`memory_entry_t *` → `char *`)
- `file_safety.c:539`: dangling pointer to `r_home`
- `send_message.c:192`: implicit declaration of `is_write_denied`

## Python Reference Access

```bash
cat /home/wubu/hermes-agent-dev/agent/<module>.py
cat /home/wubu/hermes-agent-dev/tools/<tool>.py
cat /home/wubu/hermes-agent-dev/hermes_cli/<cmd>.py
```

Key directories:
- `agent/` — Agent internals (providers, memory, caching, compression)
- `tools/` — Tool implementations (browser, terminal, files, etc.)
- `hermes_cli/` — CLI commands and setup
- `gateway/` — Gateway runner and platform adapters
- `plugins/` — Plugin system (memory, providers, etc.)

## Naming Strategy

**Core rule:** Match Python function names exactly. See `references/naming-strategy.md` for full details including decision tree and 5 naming patterns.

### Quick Reference

| Pattern | Rule | Example |
|---------|------|---------|
| Exact match | Use Python name as-is (40%) | `estimate_tokens_rough` → `estimate_tokens_rough` |
| Drop `_` prefix | Remove Python private marker (20%) | `_estimate_message_chars` → `estimate_message_chars` |
| Add domain prefix | Add module prefix for namespace (25%) | `_is_openrouter_base_url` → `provider_is_openrouter_base_url` |
| Word reorder | Put domain prefix first (10%) | `_is_telegram_thread_not_found` → `telegram_is_thread_not_found` |
| Semantic rename | Different name for different approach (5%) | Rare — only when implementation differs |

### Common Prefixes
- `provider_` — Model provider logic (most common, ~15 functions)
- `context_` — Context file loading
- `terminal_` — Terminal/shell tools
- `telegram_` — Telegram gateway
- `bedrock_` — AWS Bedrock adapter
- `model_` — Model metadata
- `tool_` — Tool utilities
- `estimate_` — Token estimation
- `cmd_` — CLI commands
- `gw_` — Gateway core

### Header Include Pitfall
**NEVER rename inside `#include` directives.** The word-boundary rename of `ansi_strip` → `strip_ansi` changed `#include "ansi_strip.h"` to `#include "strip_ansi.h"`, breaking the build. Header filenames are NOT function names.

**Fix:** After bulk rename, always check `#include` lines:
```bash
grep -rn '#include.*strip_ansi' src/ lib/ --include="*.c" --include="*.h"
```

## JSON API — Two Systems

**CRITICAL:** Two separate JSON libraries exist. Never mix them.

### System 1: `lib/json.h` (libjson)
Used by `lib/libtooldispatch/` and other libs.
- Type: `json_t` with anonymous union
- Access: `node->c.count`, `node->c.items[i]`, `node->c.keys[i]`
- Builders: `json_string()`, `json_array()`, `json_object()`
- Mutate: `json_append(arr, item)`
- NO `json_new_*`, NO `json_array_append()`, NO `json_equals()`

### System 2: `include/hermes_json.h` / `src/deps/json.c` (hermes_json)
Used by most of `src/agent/`.
- Type: `json_node_t`
- Builders: `json_new_string()`, `json_new_array()`, `json_new_object()`
- Mutate: `json_array_append(a, i)`, `json_object_set(o, k, v)`

## C Code Conventions

### Struct Field Names
- `hermes_error_t`: `.message` (not `.msg`), `.context` (not `.ctx`)
- `agent_state_t`: `.message_capacity` (not `.messages_capacity`)

### Enum Prefixes
- Message roles: `MSG_USER`, `MSG_ASSISTANT`, `MSG_TOOL`, `MSG_SYSTEM`
- Error codes: `HERMES_ERR_IO`, `HERMES_ERR_RATE_LIMITED`, `HERMES_ERR_CONNECTION`

### String Functions
- Use `strncasecmp` for case-insensitive comparison (not `stricmp`)
- `strncasecmp` available without `strings.h` include

### Header Include Best Practices
Every header must be self-contained:
- `size_t` → `#include <stddef.h>`
- `memcpy`, `strcmp`, etc. → `#include <string.h>`
- `malloc`, `free`, `strtol` → `#include <stdlib.h>`
- `time()`, `difftime()` → `#include <time.h>`
- `getcwd()` → `#include <unistd.h>`

### Adding New Source Files
1. Add `src/agent/<name>.o` to `AGENT_OBJ` in Makefile
2. Add function declarations to `include/hermes_agent.h`
3. **Check for symbol collisions** with existing `lib/` libraries BEFORE writing code
4. Run `grep -rn "your_struct_or_fn_name" lib/` before implementing

### C Portability
- **No GCC statement expression macros** `({ ... })` — use regular `static` functions
- `pthread_t`, `pthread_create`, `pthread_detach`, `pthread_mutex_t` available
- Use `pthread_detach()` for fire-and-forget threads

## Git Pitfalls

### .hermes Directory is Gitignored
```bash
git add -f .hermes/mind-palace/battleship-v40.md
PRE_COMMIT_ALLOW_NO_CONFIG=1 git commit -m "..."
```

### NEVER use `git add -f .` from slermes/
It grabs ALL files including `*.o`, `*.so`, binaries, `__pycache__/`. Use explicit file lists.

### Pre-commit Hooks
Always use `PRE_COMMIT_ALLOW_NO_CONFIG=1` — pre-commit is not configured.

## Name Parity Protocol

When porting OR renaming a function:

1. **Extract actual C function names** from the target file first — don't guess
2. **Check for duplicates** — `grep -rn "func_name" src/ lib/ include/ --include="*.c"`
3. **Verify the rename won't break includes** — check `#include` lines separately
4. **Build to verify** — `make -j4` must pass with 0 errors
5. **Update header declarations** — .h files must match .c files

### Rename Verification Steps
```bash
# 1. Find all references to old name
grep -rn "old_name" src/ lib/ include/ --include="*.c" --include="*.h"

# 2. Check if new name already exists
grep -rn "new_name" src/ lib/ include/ --include="*.c" --include="*.h"

# 3. Check includes separately (word-boundary rename can break includes)
grep -rn '#include.*old_name' src/ lib/ --include="*.c" --include="*.h"

# 4. After rename, build
make -j4

# 5. Run tests
./test_runner.sh
```

## Current State (v77)

- **Battleship:** v77
- **Name parity:** 27 total renames, 0 remaining
- **C source files:** 276+ (210 src/ + 66 lib/)
- **Build:** Clean, 4/4 tests pass

## Reference Files

- `references/naming-strategy.md` — Python→C naming conventions (decision tree + patterns)
- `references/py-c-name-mapping.md` — Complete Python↔C function name mapping
- `references/audit-results.md` — Full audit data from checkpoint 31
- `references/deep-audit-results.md` — Deep audit data from checkpoint 32
- `references/json-api-details.md` — JSON API function signatures, struct access patterns
- `references/da-sweep-cp45.md` — Triple DA sweep results (checkpoint 45)
- `references/da-sweep-cp46.md` — Triple DA sweep results (checkpoint 46)
- `references/full-directory-scan-methodology.md` — Scan procedure and misclassification patterns
