# Slermes oracle harness infrastructure

This directory is the single home for faithful port-vs-LIVE-Python oracle
checks. It exists to stop every port from reinventing its own fixtures,
compile flags, and runner. Reuse it. Do NOT scatter fixtures in /tmp or
recreate a per-port runner.

> **FAP (Functional Alignment Problem):** a C implementation that is statically
> "ported" (annotated, compiles, depth-check clean) but whose *runtime behavior*
> on concrete inputs diverges from LIVE Python Hermes. The parity scanner
> (`slermes_parity_battleground.py`) is **blind** to FAPs — it never executes
> anything. The oracle harness is the *only* thing that finds them. A
> `cases: MISMATCH` from this harness **is a FAP**. See `docs/fap.md` for the
> canonical definition, the real-vs-false FAP distinction, and the triage
> procedure.

Two oracle contracts are supported:

  A) "piped" (legacy, run_one_oracle.sh)
     The C harness (tests/t_port_<name>.c) prints oracle-readable lines on
     stdout; tests/sta_oracle_<name>.py reads them on stdin and asserts
     against LIVE Python. Run with: tests/run_one_oracle.sh <name>

  B) "diff" (canonical-JSON, run_oracle.sh)
     Both the C harness and the Python oracle emit the SAME canonical JSON
     for a given fixture. run_oracle.sh compiles the harness, runs it and
     the oracle over each fixture, and diffs the two outputs. Any diff =
     **FAP** (mismatch). Used by structured ports (e.g. patch_parser).

Fixtures live in fixtures/ and are committed. Add new ones there, not /tmp.

## fixtures/ layout

  fixtures/<port_name>/
      <case>.in          # raw input fed to both harness and oracle
      ...

The harness and oracle are told which fixture to read via argv[1].

## How to find FAPs (the only correct procedure)

```bash
# Run ALL registered ports (each diffs C vs LIVE Python over fixtures):
bash tests/oracle/run_oracles.sh            # CI mode: exit 1 on any FAP

# Run a single port:
bash tests/oracle/run_oracles.sh <port_name>
```

Every `cases: MISMATCH` is a **FAP candidate**. Triage (full steps in
`docs/fap.md`):
1. Re-run both sides locally and diff the raw outputs.
2. If the diff is environment state (live `git status`, hostname, clock, `TERM`,
   `NO_COLOR`) → **FALSE FAP**: add a `strip_patterns`/`strip_fields` entry to
   `tests/oracle/registry.json`'s `normalize` block. Do NOT change C.
3. If the diff is real behavior → **Real FAP**: read the LIVE Python source,
   fix the C, re-run until `MATCH`.

> The harness writes artifacts under `tests/oracle/build/` (never /tmp) so they
> are version-controllable and inspectable. `results.jsonl` records every run
> with commit SHA + verdict for **stale detection** — a previously-`MATCH`ing
> port that flips to `MISMATCH` is flagged as a regression.

## Adding a new oracle port (and keeping it a FAP detector)

  1. Write src/.../port_<name>.c + .h (faithful, single-line /* PoP: */).
  2. Register the .o in build/objects.mk.
  3. Write tests/t_port_<name>.c (reads fixture from argv[1], prints result).
  4. Write tests/sta_oracle_<name>.py (reads same fixture, prints expected from
     LIVE Python).
  5. Drop fixtures in fixtures/<name>/ (at least one *.in).
  6. **Register the port in tests/oracle/registry.json** under `ports` with
     `harness`/`oracle`/`fixtures` paths. An unregistered port never runs, so
     its FAPs stay silent. This is the single most common way FAPs hide.
  7. Verify: bash tests/oracle/run_oracles.sh <name>  →  cases: MATCH
  8. Wire into build/test.mk if you want it in `make test`.
