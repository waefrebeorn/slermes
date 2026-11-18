<!-- PARITY:AUTO -->
| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Slermes C Standards

The single authoritative standard for C in this repository. Every rule here is
either **enforced by a gate** (`tests/verify_c_standards.py`, `verify_naming.py`,
`verify_header_guards.py`) or is a convention a reviewer will ask you to follow.
Where this document and a gate disagree, the gate is wrong — fix the gate.

This replaces the thin "Code Style" bullet list in `docs/dev/index.md`.

## 1. Language

- **C11**, `-std=c11`, no GNU extensions, no C++, no VLAs, no `alloca`.
- Warnings that are errors: `-Werror=implicit-function-declaration` (always).
  The build's `PROFILE` adds more; `PROFILE=strict` is the bar for new work.
- POSIX/`winlibs` portability is required: Windows, Linux and macOS all build
  from this tree. Anything platform-specific goes behind `include/compat/`.

## 2. File layout

```
src/<py_top_dir>/port_<py_module_stem>.c     implementation
include/port_<py_module_stem>.h              interface (if it has one)
tests/sta_oracle_<py_module_stem>.py         differential oracle
tests/t_port_<py_module_stem>.c              C harness driving that oracle
```

`<py_top_dir>` is the top-level Python package the module lives in
(`agent/`, `hermes_cli/`, `gateway/`, `pm/`, ...).

A unit is named for the **Python module it ports**, so parity can be audited by
name instead of by archaeology. See `verify_naming.py`.

### 2.1 Header comment

Every `src/**/port_*.c` opens with a file comment naming its source module and
its PoP annotations:

```c
/* port_status.c -- pure helpers ported from gateway/status.py.
 *
 * PoP: st_hermes_home_assignments     @ gateway/status.py:hermes_home_assignments
 * PoP: st_command_line_names_home     @ gateway/status.py:command_line_names_hermes_home
 *
 * Anything that is NOT a pure helper, and why it lives elsewhere.
 */
```

Rules:
- Line 1 is exactly `/* port_<stem>.c -- ` followed by a description.
- At least one `PoP:` line.
- `PoP: <c_function> @ <python_file>:<python_function>` — the C name must be
  defined in this file; the Python file must exist in this repo or in the
  upstream quarry.

### 2.2 Includes

1. The unit's own header, first: `#include "port_<stem>.h"`.
2. Then system headers (`<...>`), then first-party headers (`"..."`).
3. No include of a `.c` file, ever.

## 3. Naming

| What | Convention | Example |
|---|---|---|
| Exported function | `<unit_prefix>_<snake_case>` | `st_hermes_home_assignments` |
| `static` function | `<unit_prefix>_<snake_case>` | `st_is_sep` |
| Exported type | `<unit_prefix>_<snake_case>_t` | `st_profile_t` |
| `static` file-scope variable | `s_<snake_case>` | `s_last_err` |
| Constant / macro | `UPPER_SNAKE_CASE` | `ST_MAX_TOKENS` |
| Header guard | `PORT_<STEM>_H` | `PORT_STATUS_H` |

- The **unit prefix** is a short lowercase abbreviation of the unit, shared by
  every exported symbol in the file (`st_` for `port_status`, `ai_` for
  `port_auth`, `dsp_` for display helpers). It exists so a symbol's owning
  unit is legible at the call site and so link-time collisions are impossible.
- **Never** use a name reserved by C, C POSIX, or the C library: no leading
  underscore, no `_[A-Z]`, no `l`/`l0`/`l1`/`lO` (letter/digit confusion),
  no libc names (`strlen`, `malloc`, `index`, ...). Enforced by
  `scripts/drop_reserved_underscores.py`.
- Snake case only. No camelCase, no Hungarian notation.

## 4. Interfaces

- An exported function is declared in `include/port_<stem>.h` and **only** there.
- The header states ownership and failure semantics on the declaration:
  caller frees, returns NULL on failure, writes at most `n` bytes.
- The header includes only what its own declarations need (`<stddef.h>` for
  `size_t`, not `hermes.h`).
- `static` for anything not in the header. If it does not need to be exported,
  it must not be.
- No global mutable state across units; pass context in.

## 5. Correctness

- **Every** buffer is initialized: `char buf[N] = {0};`. Uninitialized arrays
  are the single most common bug class in this codebase.
- Check `malloc`/`calloc`/`realloc`/`strdup` returns. On failure, free what was
  allocated and return `NULL` (or `false`).
- Every allocation has exactly one owner, named in the header. No double free,
  no leaking on an early-return path.
- `snprintf`/`strncpy` are not "safe" — check the bound and NUL-terminate.
- Never return a pointer to a stack buffer.
- String slicing: copy, do not alias into an input buffer that may be freed.
- Error paths are tested, not just the happy path.

## 6. JSON

- Structured I/O is JSON, built and read with the repo's `json_t` API.
- `json_obj_get` returns a `json_t *`; use `json_get_str` / `json_get_int` /
  `json_get_bool` for scalars. Assigning a `json_t *` to a `char *` is a
  compile error, and a silent `void *` cast around it is a bug.
- Never hand-format JSON into a string buffer.

## 7. Comments

- A comment explains **why**, not what. `/* i++ */` is noise.
- A non-obvious constant, a magic number, or a deliberate deviation from the
  Python original gets a comment saying so.
- PoP annotations are **credit and audit data**. A function without one is
  invisible to the parity scanner and cannot be counted. Do not add a PoP
  annotation to a function you did not port — an unbacked annotation is worse
  than none.
- Keep comments in sync with the code. A comment that contradicts the code is
  worse than no comment.

## 8. Formatting

- 4 spaces, **no tabs**.
- LF line endings everywhere in the repo.
- Every file ends with exactly one newline.
- No trailing whitespace.
- Braces on the same line, `if (...) {`.
- Keep lines to ~100 columns.

## 9. Porting a Python module

1. Name the unit after the Python module (§2).
2. Port only **pure** helpers first — no I/O, no global state. They are the ones
   an oracle can differentially test.
3. Annotate every ported function with PoP (§2.1).
4. Write the oracle (`tests/sta_oracle_<stem>.py`) and the harness
   (`tests/t_port_<stem>.c`) **before** you trust the port.
5. Declare the unit `incomplete` in `tests/oracle/registry.json` until the
   oracle passes. Declaring it complete while the oracle cannot run is how a
   green number stops meaning anything.

## 10. Gates

Run before every commit:

```bash
python tests/verify_c_standards.py     # this document's machine-checkable rules
python tests/verify_naming.py          # unit/file naming standard
python tests/verify_header_guards.py   # include guards
python tests/verify_build_health.py --no-link
python tests/verify_oracle_instrument.py --parity
```

Gates **fail on new violations**. A pre-existing violation is allowlisted with
its reason; deleting an allowlist entry without fixing the file is a gate
failure. A green number you had to allowlist your way to is not green.
