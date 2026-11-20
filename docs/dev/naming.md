<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Naming Standard

Enforced by `tests/verify_naming.py`. This is the document that gate cites; if
the gate and this file disagree, the gate is wrong.

## The rule

**A port unit is named for the Python module it ports**, so parity can be
audited by name instead of by archaeology.

```
src/<py_top_dir>/port_<py_module_stem>.c     implementation
include/port_<stem>.h                        interface
tests/sta_oracle_<stem>.py                   differential oracle
tests/t_port_<stem>.c                        C harness for that oracle
```

`<py_top_dir>` is the top-level Python package the module lives in
(`agent/`, `hermes_cli/`, `gateway/`, `pm/`, ...). `<stem>` is the Python module
filename without `.py`.

Examples:

| Python module | C unit | Header |
|---|---|---|
| `gateway/status.py` | `src/gateway/port_status.c` | `include/port_status.h` |
| `agent/display.py` | `src/agent/port_display.c` | `include/port_display.h` |
| `hermes_cli/auth.py` | `src/hermes_cli/port_auth.c` | `include/port_auth.h` |

## Why name-for-name

The parity scanner, the oracle registry and the gap ledger all join on the
module name. A unit named `src/gateway/gw_helpers.c` is invisible to every one
of them: it cannot be matched to `gateway/status.py`, so its work is
uncountable and its oracle cannot be found. Renaming is cheap; archaeology is
not, and it does not survive a rebase.

## What the gate enforces

These are the invariants parity actually depends on:

1. **A PoP annotation must name a Python file that EXISTS.** A port annotated
   against a module that is not in the tree claims credit for code nobody can
   diff against. Both this repo's Python tree and the upstream quarry are
   searched before a target is called missing.
2. **A unit's header, if it has one, is `include/port_<stem>.h`.**
3. **A unit in the LINKED tree is not named `*_wrappers.c` or `*_gaps.c`.**
   Those are scanner-invisible by design, so parity credit taken in them is
   unverifiable.

## Symbol naming

See `docs/dev/c-standards.md` §3. In short: every exported symbol in a unit
carries that unit's short lowercase prefix (`st_` for `port_status`, `ai_` for
`port_auth`), so a symbol's owner is legible at the call site and link-time
collisions between units are impossible.

## Renaming

Renames are welcome and authorized project-wide. Do them with a script, in one
commit, and re-run the oracle suite — a rename that is not propagated to the
harnesses compiles the product cleanly while silently breaking 13 oracles.
