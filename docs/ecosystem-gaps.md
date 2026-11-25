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

**P2 — Classify the unclaimed surface (measurement, not porting).**
Sort the 2,365 no-claim files and 570 pending headers into four buckets with counts:
*done*, *seam*, *out of scope*, *todo*. Exit criterion: a table in this file where the
four counts sum to 2,935, so the completion ratio becomes a real denominator.

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
