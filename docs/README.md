# Slermes docs — read in this order

A C11 port of the Hermes Agent Python tree. Four documents, each answering one question.

| doc | question it answers |
|---|---|
| **[ecosystem-gaps.md](ecosystem-gaps.md)** | Where does the port actually stand, what is the real backlog, what do we do next? Measured inventory, per-subsystem ratios, the 5-bucket surface table, the six-part plan. **Start here.** |
| **[port-baseline.md](port-baseline.md)** | What is the standard a number has to meet, and what did the last gate sweep find? |
| **[heldout-fuzz.md](heldout-fuzz.md)** | How does held-out fuzzing work, what has it found, and which hypotheses died to measurement? Open items live here. |
| **[dev/](dev/)** | Design notes for individual subsystems. |

## The one command

```bash
bash scripts/check.sh          # artifact runs, build healthy, numbers derived now
bash scripts/check.sh --gates  # + held-out fuzz across every wave plan
bash scripts/check.sh --full   # + rebuild first
```

## The rules this repo runs on

1. **A silent exit is a failure.** `slermes.exe` prints nothing and exits 127 without its
   DLLs; every downstream script read that as "not run" for months. `bundle_dlls.sh`
   fixes it and `check.sh` proves it every run.
2. **Never report a count a tool did not produce on this run.** No carried numbers, no
   extrapolation, no "should be".
3. **PoP annotation is not parity.** A `PoP:` comment claims a port; only a gate that
   passes, or a flow that runs, demonstrates one. 557 placeholder PoP lines currently
   claim behaviour that decides nothing.
4. **A missing measurement is not a zero.** A gate that prints no summary is `NO-SUMMARY`,
   never `VACUOUS`.
5. **Shrink before fixing.** The honest minimal case is usually not the shape the plausible
   story predicts — six hypotheses in `heldout-fuzz.md` died exactly there.
6. **Revert rather than argue.** If the fix does not move the measurement, restore the tree
   and record the negative result.

## Where things live

- `src/` — the port, mirroring the Python package layout.
- `tests/oracle_<gate>.py` — differential oracle per gate; `tests/fixtures/<gate>.jsonl` is
  **input-only** (no recorded `out`; the oracle computes the expectation on every run).
- `scripts/check.sh` — the entry point. `port_audit.py` (claims and gates),
  `classify_surface.py` (5-bucket surface), `gate_fuzz.py` (held-out fuzz),
  `fuzz_shrink.py` (minimiser), `bundle_dlls.sh` (make the artifact start).
- `build/objects.mk` — **the link manifest**. A `.c` file that is not listed there is not in
  the product, no matter how complete it is. `mcp_serve.c` (400+ lines, a whole HTTP server)
  was missing from it while a stub shadowed its API, so `mcp-serve` could only ever print
  "Failed to start server".
