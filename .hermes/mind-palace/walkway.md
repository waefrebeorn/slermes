# Walkway — Slermes C Translation

## Current Sprint: Gap Blitz + Stub Hunt

**Active:** v541 (2026-07-07)
**PORTED:** 4,664 (47.9%)
**REAL_GAP:** 5,067 (52.1%)
**Façade audit:** COMPLETE — 18 files / 52 fake-looking stubs rewritten as REAL ports; + follow-up sweep closed 3 more real fakes (antigravity HTTP, desktop OAuth, relay-WS) and reworded 4 misworded comments. Binary links, 36/36 tests pass.

## Mantra

> "Rewriting in scratch in C" is the point of the project.
> Anything that falls under that is reclassified as REAL_GAP.
> No stubs. No scaffolding. No "for later."

## Active Loops

### Loop 1: Gap Blitz (Current)
- 6-13 gap blitzes: COMPLETE
- Next: 14-gap blitz (7 modules)
- Then: 15-gap, 16-gap, etc.

### Loop 2: Stub Hunt (Next Priority)
- 3,350 scaffolding stubs identified
- Priority: cli.py (244), gateway/run.py (224), feishu.py (190)
- Each stub must be replaced with real C logic

### Loop 3: Devil's Advocate Review
- Every "PORTED" function must prove it's not a stub
- Triple-check: form not function gaps
- 1:1 parity to TempleOS/Arch/Ubuntu infrastructure

## Completed Milestones
- v443: 10-gap blitz complete
- v444: 11-gap blitz complete
- v445-448: 12-gap blitz complete (9 modules)
- v449: 13-gap blitz complete (8 modules)
- 2026-06-21: Discovered 3,350 scaffolding stubs
- v541: FAÇADE AUDIT COMPLETE — 18 files / 52 fake-looking stubs → real ports (libhttp/libjson/libwebsocket/libcrypto/libbase64/libmcp_oauth + real subprocess/fs). Binary links, 36/36 tests pass.

## Next Actions
1. 14-gap blitz (7 modules)
2. Stub hunt — implement 3,350 scaffolding stubs
3. Update battleship.md with fresh gap catalog
4. Devil's audit of all PORTED classifications
