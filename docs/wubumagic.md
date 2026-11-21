<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
| `json.loads(s)` | `json_parse(s, &err)` |
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
6. **Naming standard** — units are named for the Python module they port
   (`port_<stem>.c`), exported symbols carry a per-unit prefix (`st_`, `ai_`),
   and the `/* PoP: <c_fn> @ <module.py>:<py_fn> */` annotation is the parity
   join — see docs/dev/c-standards.md and docs/dev/naming.md (gate-enforced)

### File Organization (the enforced standard)
- Implementation: `src/<py_top_dir>/port_<py_module_stem>.c`
- Interface: `include/port_<stem>.h`, guard `PORT_<STEM>_H`
- Oracle: `tests/sta_oracle_<stem>.py` + harness `tests/t_port_<stem>.c`
- The unit's own header is included first; no `.c` includes, ever
- Self-contained: compile independently; register the object in the right
  `build/objects.mk` cupboard (registered ≠ reachable — check the link line)

### Verification
1. `make slermes` — must build clean
2. `python tests/verify_build_health.py --no-link` — sources + port-object gate
3. Oracle — differential proof vs LIVE Python (`tests/run_one_oracle.sh`)
4. Watercup gate for pure helpers — path parity, not just output
5. `make parity` — scanner sentinel refreshed from live sources
A port is parity-closed only when oracle AND watercup say MATCH.

## The Startup Process

When the user says "~ WuBu ~ <module>":

1. **SCAN** — Run the parity scanner, identify REAL_GAP functions in the target module
2. **PRIORITIZE** — Rank by: leaf-first (no internal calls), then by impact (auth > util)
3. **READ** — For each function: read the Python source, understand the logic
4. **SEARCH** — Check if the function's dependencies already have C ports
5. **TRANSLATE** — Write faithful C using the pattern map above
6. **WIRE** — Add to `build/objects.mk` if new file
7. **BUILD** — `rm -f slermes && make slermes` MUST pass
8. **VERIFY** — oracle passes (mandatory where one exists), watercup gate
   for pure helpers, then `make parity` refreshes the scanner sentinel
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
