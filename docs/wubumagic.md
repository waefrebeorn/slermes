<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# WuBuMagic Doctrine

## What It Is
WuBuMagic is the slermes faithful-port engine. It translates Python (Hermes) functions into faithful C11 (slermes) implementations. Not stubs. Not scanner-gaming. Real logic, real behavior, real C.

## The Oath
Every WuBuMagic port MUST:
1. **Read the Python source** — understand what it actually does
2. **Map to slermes libs** — SQLite for ORM, libjson for dicts, libhttp for fetch, libproc for processes
3. **Preserve behavior** — same inputs produce same outputs, same error handling, same edge cases
4. **Be self-contained** — minimal includes, opaque structs, no god headers
5. **Compile clean** — `make slermes` passes, no warnings
6. **Pass the oracle** — differential test proves C matches Python

## What WuBuMagic is NOT
- NOT a stub generator (`return json_object();` is treason)
- NOT a regex replacer (blind find-and-replace is sabotage)
- NOT scanner-gaming (if the scanner says PORTED but it doesn't work, it's NOT ported)
- NOT "good enough" (trivial return values are a fired offense)

## The Translation Rules

### Python → C Pattern Map
| Python | C (slermes) |
|--------|-------------|
| `dict[key]` | `json_object_get(obj, key)` |
| `dict.get(key, default)` | `json_object_get(obj, key)` + null check + default |
| `dict[key] = val` | `json_set(obj, key, val)` |
| `json.dumps(obj)` | `json_serialize(obj)` |
| `json.loads(s)` | `json_parse(s, strlen(s))` |
| `session.query(Model).filter(...).first()` | `sqlite3_prepare_v2` + `sqlite3_bind_*` + `sqlite3_step` |
| `os.path.join(a, b)` | `path_join(a, b)` (libpath) |
| `subprocess.run(...)` | `proc_spawn(...)` (libproc) |
| `requests.get(url)` | `http_fetch(url, ...)` (libhttp) |
| `f"string {var}" | `sprintf`/`snprintf` with `%s` |
| `try/except` | return-code checking, `goto cleanup` pattern |
| `with ctx:` | explicit init/cleanup pairs |
| `list comprehension` | loop + `json_append` |
| `str.startswith/endswith` | `strncmp` + length check |
| `str.split(sep, maxsplit)` | `strtok_r` or manual scan |
| `str.strip()` | trim loop |
| `len(obj)` | `json_len(obj)` / `strlen(s)` / `array->count` |
| `isinstance(x, dict)` | `json_is_object(x)` |
| `isinstance(x, list)` | `json_is_array(x)` |
| `isinstance(x, str)` | `json_is_string(x)` |
| `x is None` | `!x` / `x == NULL` |
| `raise ValueError(msg)` | return error code + log |

### Structure Rules
1. **Opaque structs** — never expose internals, access via API only
2. **Minimal includes** — only what the function actually needs
3. **C11 only** — no C++, no compiler extensions, `-std=c11`
4. **Error handling** — every allocation/operation checked, `goto cleanup` for multi-step
5. **No landlocked statics** — oversized arrays → hive (lib/libhive)
6. **Name parity** — C function name MUST match Python name exactly (drop leading `_`)

### File Organization
- One C file per Python module: `src/ports/<module_path>.c`
- Header: `src/ports/<module_path>.h` (only if other files need the API)
- Include guard: `#pragma once`
- Self-contained: compile independently

### Verification
1. `make slermes` — must build clean
2. `make parity-walkway` — scanner says PORTED
3. Oracle test (if available) — differential proof
4. Manual review — does the C do what the Python does?

## The Startup Process

When the user says "~ WuBu ~ <module>":

1. **SCAN** — Run the parity scanner, identify REAL_GAP functions in the target module
2. **PRIORITIZE** — Rank by: leaf-first (no internal calls), then by impact (auth > util)
3. **READ** — For each function: read the Python source, understand the logic
4. **SEARCH** — Check if the function's dependencies already have C ports
5. **TRANSLATE** — Write faithful C using the pattern map above
6. **WIRE** — Add to `build/objects.mk` if new file
7. **BUILD** — `rm -f slermes && make slermes` MUST pass
8. **VERIFY** — Scanner says PORTED, oracle passes (if available)
9. **COMMIT** — One commit per function or logical group

## The Anti-Patterns (FIRED OFFENSES)
- `return json_object();` with no logic
- `/* TODO: Implement */`
- Copying the Python verbatim as a comment and returning a default
- Adding a PoP without a real function body
- Gaming the scanner by any means other than actual implementation
- Saying "this is too hard, it needs the Python runtime" without trying

## The Mantra
**"If it compiles but doesn't compute, it's not ported."**
