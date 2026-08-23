# Water-cup batch expansion — protocol for parallel agents

Goal: one gate per ported Python module (target 600+). Each gate =
`tests/watercup/gates/<gate_name>/{harness.c,twin.py,gate.json}`.

## Per-gate contract (see slermes-water-cup-parity-oracle skill)

1. Pick ONE pure, deterministic Python function from the assigned module
   (branches/loops, scalar-or-string output; NO I/O in the traced path).
2. Find its C port: `grep -rn "PoP:.*@ <module>:<fn>" src/ lib/`.
3. Write `twin.py`: import LIVE python fn by path (copy an existing twin's
   import boilerplate), trace the SAME branch ladder rung-order as C.
4. Write `harness.c`: traced copy calling/reimplementing the C port logic,
   WCUP_BEGIN/_BRANCH/_VALSTR/_FINALIZE per rung. argv: `<c_fn> <inputs...>`,
   print exactly one cup hash line per input.
5. Write `gate.json`: name, subsystem, kind=cup, c_src=harness.c, c_fn,
   py_module, py_fn, inputs[].
6. Verify GREEN before declaring done:
       cc -I tests/watercup <extra flags> gates/<g>/harness.c <extra_src> -o build/<g> -lm
       ./build/<g> <c_fn> inputs...
   and `python3 wc_run.py <gate>` must report MATCH.
7. Gate naming: `<pybasename>_<fnname>` (e.g. sizefmt_format_bytes).

## Rules
- Rung order is the contract. V-events on both sides or neither.
- Harness must exercise the REAL C port logic (call it if linkable via
  extra_src, else faithful traced copy of it).
- No cheating: a constant-return harness will not match the twin's cup.
- If the C port is missing/broken, FIX THE PORT (that's a real parity win)
  and note it in gate.json "notes".
