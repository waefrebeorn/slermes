# Slermes ecosystem: measured state, gaps, and the plan

Every number below was produced by a tool on the run that wrote this file
(`scripts/port_audit.py`, a filesystem inventory of both trees, `objdump`, and the
binary itself). Nothing is carried from an earlier sweep.

## 1. What the trees are

| | files | size | unit |
|---|---|---|---|
| `hermes-agent` (reference) | — | 85,993 KB | `.py` under the package |
| `slermes/src` (port) | 1,878 | 69,009 KB | `.c` |

The port is **80.2 % of the reference by bytes**. That number is close to meaningless on
its own — C is 1.5–1.9× more verbose than Python for the same behaviour, so the
per-subsystem ratios below are the only honest reading.

| subsystem | hermes KB | slermes KB | ratio | reading |
|---|---|---|---|---|
| `agent` | 8,204 | 12,609 | 153.7 % | verbosity, not extra behaviour |
| `gateway` | 7,506 | 14,170 | 188.8 % | same |
| `tools` | 6,803 | 9,445 | 138.8 % | same |
| `cron` | 1,031 | 1,323 | 128.4 % | same |
| `persistence` | 2,872 | 2,884 | 100.4 % | genuinely complete |
| `hermes_cli` | 15,480 | 13,381 | 86.4 % | real remainder |
| `plugins` | 5,039 | 4,663 | 92.5 % | near |
| **`tui_gateway`** | **2,465** | **911** | **37.0 %** | **the big gap** |
| `sitecopy`, `scripts`, `evals`, `optional-skills`, `slermes` pkg | 33,769 | 0 | 0 % | deliberately not ported |

C-only code with no Python counterpart: `cli` 7,020 KB (the C front-end), `lv` 157 KB,
`acp` 91 KB, `provider` 86 KB, `pet` 55 KB, `lua` 10 KB.

## 2. What the audit says about claims

```
ported files            1878   (8068 behaviour PoP lines)
pending headers          570   <- header admits no behaviour port
placeholder PoP lines     542   <- *_pending symbols, decide nothing
no claim                2365
gates with >= 5 cases   2296   (112240 authored cases)
no fixture               146   <- nothing ever drives the harness
fixture < 5 cases          1
constants-only surface    72
no harness                 3
```

Two of these are the real structural gaps, and neither is a gate:

* **570 pending headers.** An interface exists, no behaviour behind it. This is the
  largest single block of "looks ported, is not".
* **2365 files with no claim at all** — no PoP line, no header. Either dead weight or
  unported work nobody has admitted to. Until that bucket is classified, the ratio in
  section 1 is a *file size* comparison and nothing more.

`542 placeholder PoP lines` are the third: a `*_pending` symbol that records an intention
as though it were a measurement. These inflate the ported count without moving behaviour.

### 2a. The claimless surface, classified (P2)

`scripts/classify_surface.py` over all 4,813 `.c` files:

| bucket | files | KB | what it is |
|---|---|---|---|
| **done** | 1,878 | 18,755 | a real (non-placeholder) PoP claim, no pending admission |
| **todo** | 1,110 | 16,628 | real function bodies, **no claim at all** |
| **seam** | 811 | 25,391 | touches files, sockets, TLS, processes — needs an environment, not a pure oracle |
| **outside** | 219 | 7,196 | C-only trees with no Python counterpart (`cli`, `pet`, `lv`, `lua`, `desktop`, `compat`, `ports`, `provider`) |
| **empty** | 795 | 435 | ~550 B each: placeholder TUs, no bodies |
| **total** | 4,813 | 68,405 | |

`todo + seam + outside + empty = 2,935`, exactly the audit's 570 pending headers plus 2,365
no-claim files. Two of those buckets decide what "coverage" can mean from here:

* **`todo` is the real porting backlog: 1,110 files / 16.6 MB of code that exists and has
  never been compared to anything.** It is not smaller than the done bucket — it is nearly
  the same size. Any percentage that omits it is a percentage of the wrong set.
* **`seam` is bigger still, in bytes: 811 files / 25.4 MB — 37 % of the tree's source.**
  These cannot be closed by value→value oracles at all; they need environment-level tests
  or an explicit "documented seam" decision. This is where P5's end-to-end flow earns its
  keep.

**Retracted on the way:** my first classifier used a return-type regex and reported
*1,346 empty files / 28 %*; a second cut flipped 1,877 into "empty" because a missing body
was the only branch that absorbed unclaimed code. Both were wrong for the same reason — a
heuristic tuned to prove a story. The trustworthy check is the one now in the tool:
`empty` holds 795 files averaging 550 B, i.e. it is only ever true placeholders.

## 3. The artifact gap — the finding to act on first

`slermes.exe` (29.75 MB) **runs**, and answers:

```
$ PATH=/c/msys64/mingw64/bin:$PATH ./slermes.exe --version
WuBu Slermes v0.0.0-slermes
Slermes C Translation
Build: Nov 19 2026 01:01:13
```

Without that PATH entry it prints **nothing** and exits **127**. Not a crash, not a
message: silence plus 127, which every shell script and every gate reads as "command not
found" or "not run". `objdump -p` explains it — the binary needs seven non-system DLLs
(`SDL2`, `SDL2_ttf`, `libcrypto-3-x64`, `libssl-3-x64`, `libpcre2-8-0`, `libtre-5`,
`zlib1`) that are not beside it:

This is **not** a translation bug. It is a packaging gap that has been mis-read as a
broken binary for as long as `--version` has been in the notes, and it means the port's
own smoke test has been unable to tell "works" from "silently refuses to start".

`src/main.c` dispatches 15 commands (`gateway`, `cron`, `init`, `setup`, `doctor`,
`update`, `completions`, `acp`, `mcp-serve`, `api-server`, `tui`, `--tui`, `--version`,
`--help`, `-v`), and there is a desktop GUI (`desktop_gui.c`, `app_desktop.c`). 436 `.c`
files touch real I/O (`fopen`, `socket`, `SSL_`, `curl_easy`), so the tree is not a pure
function library — it has wiring, and the wiring has never been exercised end to end.

## 4. Gaps, ranked by what they cost

1. **Packaging.** Seven DLLs on PATH, silent 127 without them. Every downstream claim
   rests on a binary nobody can run reliably.
2. **`tui_gateway` at 37 %.** The interactive face of the product is the least-ported
   subsystem. The port is strongest exactly where it matters least (pure helpers) and
   weakest where a user meets it.
3. **570 pending headers + 2,365 unclaimed files.** Unclassified surface. The true
   completion ratio cannot be stated until these are sorted into *done*, *seam*,
   *deliberately out of scope*, and *todo*.
4. **146 gates nothing drives.** Measured-oracle coverage is not coverage of the tree.
5. **Performance.** `w33_portabilit_b54bb9` is 49 s against live's 1.39 s on a 15.7 MB
   backup — every JSON primitive measured fast, so the cost is field-level work.
6. **End-to-end behaviour.** 112,240 authored cases across 2,296 gates, and not one of
   them starts the gateway and completes a turn.

## 5. Plan: six parts, in order

**P1 — Make the artifact honest (smallest, unblocks everything).**
Copy the seven DLLs beside the binary, add a `scripts/smoke.sh` that runs
`--version`, `--help`, `doctor` and fails loudly on empty output or 127, and wire it into
`tests/verify_build_health.py`. Exit criterion: `./slermes.exe --version` prints the
banner with no PATH help, and the health gate fails if it ever stops doing so.

**P2 — Classify the unclaimed surface (measurement, not porting).** ✅ **DONE** —
`scripts/classify_surface.py`, table in section 2a: 2,935 claimless files resolved into
todo 1,110 / seam 811 / outside 219 / empty 795, summing exactly.

**P3 — Kill the placeholder class.**
542 `*_pending` PoP lines either become behaviour with a gate or become *todo* in P2's
table. Exit criterion: zero placeholder lines counted as behaviour — the audit tool
subtracts them from the ported total.

**P4 — `tui_gateway`, subsystem by subsystem.**
37 % → 80 %+. Big parts, in dependency order, each with a harness that drives the real
command path rather than a pure helper. This is where the product actually lives.

**P5 — One end-to-end flow.**
Start the gateway, open a session, send a message, get a reply, write a row, read it back.
One script, one exit code. This is the only test that can say "it works".

**P6 — Performance and the long tail.**
The 49 s → sub-2 s `_portability` chase, the 3 case-0 gates, the 146 fixture-less gates.

Order matters: P1 before everything (nothing else is verifiable without a runnable
artifact), P2 before P3/P6 (you cannot rank what you have not classified), P4 after P5's
harness exists so the largest subsystem is measured by the same end-to-end instrument.

## 6. What this replaces

`docs/port-baseline.md` measured gates. Gates are a proxy: 2,296 of them, each small,
and the board has sat near 60 % clean for three sweeps because gate-level triage finds
one defect at a time in code that is otherwise whole. The parts above are chosen by
*consequence* — can the thing run, is the interactive face there, is the claim honest —
not by which gate is cheap to close.

## 7. P5 result: the product runs, and it was hiding its own code

**One end-to-end flow now passes** (`scripts/flow_mcp.sh`):

```
POST /mcp initialize  -> {"result":{"protocolVersion":"2025-03-26","serverInfo":{"name":"hermes-c-mcp"}}}
POST /mcp tools/list  -> {"result":{"tools":[{"name":"transcribe",...}]}}
```

Three faults were stacked, none of them visible on the gate board:

1. `src/mcp_serve.c` — 400+ lines, a whole HTTP/JSON-RPC server — **was never listed in
   `build/objects.mk`**. Four one-line stubs in `infrastructure_stubs.c` shadowed its API,
   so the command could only print "Failed to start server". This is the placeholder class
   from P3, except here the *real* implementation was already written.
2. On Windows a `SOCKET` is not a file descriptor: `read()`/`write()`/`close()` fail on
   one. The server accepted connections and answered **nothing** — `netstat` said
   LISTENING, curl timed out at 0 bytes. Needs `recv`/`send`/`closesocket`/`ioctlsocket`.
3. The link then failed with `Permission denied` on `slermes.exe` — a leftover server
   process holding the file. Not a build fault; a discipline fault.

### The structural consequence

`scripts/orphan_audit.py`: **903 `.c` files, 7.6 MB, are absent from `build/objects.mk`.**
The binary ships **19 % fewer translation units than the tree contains.**

| tree | orphan files | KB |
|---|---|---|
| `src/gateway` | 88 | 1,680 |
| `src/hermes_cli` | 149 | 1,339 |
| `src/agent` | 130 | 1,137 |
| `src/tools` | 113 | 1,116 |
| `src/cli` | 26 | 924 |
| `src/plugins` | 213 | 431 |
| `src/tui_gateway` | 38 | 17 |

This reorders the plan. Before P3 kills placeholder PoPs or P4 ports `tui_gateway`, the tree
has to stop hiding finished work: **an orphan that compiles is free progress, and an orphan
that does not compile is a real backlog item rather than an absence.** The `todo` bucket from
section 2a is therefore not 1,110 files of unported work — part of it is written and merely
unlinked, and the two cannot be told apart until each orphan is compiled.

**New P2.5 (now ahead of P3):** compile every orphan in batches, wire the clean ones into
`objects.mk`, and re-derive the surface table. Exit criteria: `orphan_audit.py` reports 0
compile-clean orphans left unwired, `LINK_RC=0`, and `flow_mcp.sh` still passes.

Note for the tooling: 903 object paths on one `make` command line is `WinError 206` on this
box — compile in chunks of 60.

## 8. The Windows-portability backlog (measured by pattern scan)

The orphan compile audit finished: of 903 files missing from the link manifest, **872
compile clean and 31 fail**. The 31 are not random — they are the POSIX surface, and a
pattern scan over `src/**/*.c` sizes the whole class:

| POSIX dependency | files | why it cannot work on Windows |
|---|---|---|
| `sys/wait.h` | **63** | header exists in mingw, the functions behind it do not |
| `fork()` | **52** | no `fork` on Windows; needs `CreateProcess` |
| `ncurses` | 15 | not shipped in the winlibs toolchain at all |
| `termios.h` | 15 | no terminal control block |
| `dlfcn.h` | 4 | needs `LoadLibrary`/`GetProcAddress` |
| `pthread_timedjoin_np` | 3 | non-portable GNU extension |
| `sys/syscall.h` | 1 | Linux-only |
| `strlwr()` conflicting decl | 1 | mingw already declares it differently |

**This is why the closure loop stalls.** It converged to 785 candidate objects and then
failed with no guilty object to drop, because the remaining undefined references are
`__imp_gl*` (needs `-lopengl32`, which exists in the toolchain) and the `fork`/`wait` family
(which silently do not exist). Wiring more files cannot fix a call to a function the
platform does not have.

**The big part this names:** a single POSIX-compatibility layer — `spawn`/`wait` on
`CreateProcess`, `LoadLibrary` for `dlopen`, a termios shim for the TUI, and either a
vendored `pdcurses` or a decision that the curses TUI is not part of the Windows target.
Until that exists, **52 files' worth of behaviour is unbuildable on the platform the port is
being developed on**, and every one of them is invisible to the gate board because gates
call pure functions, not processes.

Order: `-lopengl32` first (free, unblocks the GL cluster), then the spawn/wait shim (biggest
class, 63+52 files overlap heavily), then PDCurses or an explicit scope decision.

## 9. The orphan gap is closed: 877 translation units linked

`unshadow.py` converged in one pass — 877 objects, link `rc=0`, zero stub collisions, zero
candidate faults. `build/objects.mk` went from 3,560 to **4,437 objects**.

The reason it worked now and not four attempts earlier is not that the loop improved. Each
failure was measuring the next blocker, and the blockers were fixed by hand between runs:

| blocker | fix |
|---|---|
| 375 undefined references (`fork`/`wait`/`dlopen`) | `hermes_posix_compat.c` |
| 5 candidate files that did not compile | gettid, timed join, `strlwr`, `char*`/`json_t*`, private config type |
| `__imp_gl*` unresolved | `-lopengl32` |
| 26 `multiple definition` in `infrastructure_stubs.o` | disappeared once the real objects were in the same set |

Verified after wiring: `LINK_RC=0`, `port object gate: PASS`, health `1159/1159`,
`flow_mcp.sh` PASS, and six string-carrying gates unchanged (`w28_adapter_ca7894_p3` 70/0,
`w28_files_ee43ed_p1` 85/0, `w32_methodscon_ce0f5d_p1` 70/0, `w29_hostedroom_8927a5_p3` 77/0,
`w25_reporter_f01f70` 35/0, `w26_provider_9eacd0` 24/0).

**Measured state after:**

```
ported files  1878 (8068 behaviour PoP lines)     done     1878 files  18755 KB  39.0%
pending hdrs   570                                 todo     1110 files  16629 KB  23.1%
placeholder    542                                 seam      812 files  25406 KB  16.9%
no claim      2366                                 outside   219 files   7197 KB   4.5%
gates >= 5    2296 (112240 cases)                  empty     795 files    435 KB  16.5%
no fixture     146                                 total    4814 files  68422 KB
```

What is still outside the product: **89 superseded duplicate splits** (their symbols live in
linked monoliths; deciding which copy is authoritative is a per-symbol judgement) and **26
files that do not compile** — the curses/termios/ConPTY class, which are design decisions
rather than renames. 8 of 52 `fork()` call sites have migrated to the seam; the rest need a
pipe-based variant (`hermes_popen`) before they can move.
