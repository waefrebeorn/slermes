# Mission: Assemble the Unassembled Lego Kit

## One-line mission
The C port has ~6,128 functions that are **defined but never called**. The
project "has all the Legos but hasn't built the kit." This mission is to
**assemble** that kit: connect orphaned functions to live callers, and resolve
the ones that are redundant or need a missing assembler.

## Why this matters
A function that is never called is dead weight — it can't affect behavior, it
rots, and (worse) it *looks* like capability that doesn't exist. The port's
value is in working behavior, not in the count of translated lines. Assembly is
what turns translation into a working agent.

## The three classes (the core insight)
The dead functions are NOT mostly "one caller away." They are:

- **Class A — unique orphaned helpers (WIRE).** Ported helpers with no live
  equivalent; their caller prints raw/empty instead of using them. Wiring
  improves output and assembles the Lego. *This is the real assembly work.*
- **Class B — redundant parallel ports (DECIDE, don't wire).** Complete
  alternative implementations that duplicate a live subsystem. Wiring them
  violates AGENTS.md ("extend, don't duplicate", "no dead code wired without
  E2E proof"). These are keep-or-delete architecture decisions. See
  `class_b_redundant_ports_proposal.md`.
- **Class C — whole unwired subsystems (BUILD the assembler).** Ported as Lego
  sets but the event loop / main was never built (desktop GUI, Electron app,
  window/pty layers). Scoping these is a separate build task.

## Hard rules (how to avoid gap-mistakes)
These exist because earlier "fixes" were false-premise and had to be reverted:
1. **Verify the premise before wiring.** Grep the whole tree for the function
   name. 0 external refs = genuinely dead. If a live equivalent already exists,
   it's Class B, not A.
2. **Oracle-check, don't assume.** A doc (CHANGELOG/usage-gap) describing a
   "gap" is NOT proof of a C defect. Cross-check against Python's *actual
   runtime behavior* (e.g., `hermes_cli/main.py` argparse logic) before
   "fixing." The two earlier reversals (redact_config_value, X-04) failed this.
3. **No stubs, no wrong-contract ports.** If a function is `(void)arg; return 0;`
   or its C signature doesn't match Python, it's a stub — document, don't fake.
4. **Build + 4 oracles green after every change.** json / multi / json_in /
   scalar_str must ALL MATCH. No exception.
5. **Class B is a human decision.** Document the keep-or-delete call; never
   force-wire a duplicate.

## Definition of done (per cluster)
- Class A: function has ≥1 external caller; build+oracles green; output
  verified to improve (smoke test).
- Class B: decision recorded in `class_b_redundant_ports_proposal.md`; if
  DELETE, unique logic salvaged into live module first, file removed, census
  regenerated.
- Class C: a build plan exists (separate task); not wired ad-hoc.

## Tracking
- `wired_to_nothing_census.md` — full 6,128-function census, classified A/B/C.
- `class_b_redundant_ports_proposal.md` — Class-B decisions.
- `ASSEMBLY_MISSION.md` — this file.
- Commits: kanban formatters, agent display formatters, gateway directory
  formatter, kanban→cli_display_error routing.
