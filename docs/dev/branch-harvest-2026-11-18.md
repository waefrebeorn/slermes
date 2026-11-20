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

# Branch harvest & main-only consolidation — 

**Policy (user directive):** slermes is a **main-only** project. No feature
branches, no integrate branch. All work lands on `main` directly. Stale
branches were audited for harvestable work, preserved as archive TAGS where
they hold history, then deleted.

## Audit results

| Branch | Unique commits vs integrate | Verdict |
|---|---|---|
| `integrate` | — (was the work branch, 815 ahead of old main) | Merged into `main`, deleted |
| `main` (old local) | 0 — direct ancestor of integrate | Fast-forwarded |
| `local-port-work` | 0 — same commit as old local main | Deleted |
| `waefrebeorn/slermes` | 0 | Deleted |
| `backup-pre-sift` | 14,679 (pre-sift  lineage; work already in integrate via the sift) | Tag `archive/pre-sift-`, deleted |
| `vault` | orphan museum snapshot ( era) | Tag `archive/vault-museum`, deleted |
| `bot/js-autofix` | 1 — `npm run fix` formatting of apps/desktop (cosmetic, no behavior) | Tag `archive/js-autofix`, deleted |
| `fix/credits-free-tier-suffix-trust` | 1 — Python credits fix | **Harvested** (see below) |
| `fix/config-context-length-survives-model-switch` | 1 — Python agent fix | Backlog (see below) |
| `fix/gateway-trailing-reasoning-stream` | 1 — Python gateway fix | Backlog (see below) |

Archive tags keep every commit recoverable (`git checkout archive/<tag>`)
without keeping branches alive.

## Harvested: credits free-tier detection

The fix branch taught: a `:free` suffix trusted blindly is a silent billing
leak when a provider prices the slug > 0. The **live quarry superseded that
branch** — upstream's current `is_free_tier_model` trusts the suffix again
but adds the welcome-host rule and an authoritative pricing-cache peek. The
C port was ported to the LIVE function (quarry is the authority, not stale
branches):

- `src/agent/credits_tracker.c` — `cli_agent_credits_tracker_is_free_tier_model`
  now: suffix/stealth → no-base gate → welcome host → pricing-cache peek
  (`models_peek_cached_pricing`, new in `src/hermes_cli/models_gaps.c`).
  Previously the object was never even linked; now in `CUPBOARD_PORT_AGENT`.
- `src/hermes_cli/models_helpers.c` — `is_model_free` upgraded to live
  `_is_model_free`: subscription-billed rows are free; string prices parse
  like Python `float()` (missing → "1", junk → fail-open billable, hex
  rejected).
- Proof: `tests/t_port_credits_free_tier.c` + `tests/sta_oracle_credits_free_tier.py`
  (registered; replays cache seeds in order on both sides).

Known remaining divergence (documented, not hidden): the C pricing cache
stores plain C-string keys, so the Python `root + "\x00auth:" + digest`
authenticated-catalog dimension cannot be represented yet. The peek's auth
scan is forward-compatible but inert until the cache grows length-aware
keys. Gateway sessions never populate the picker cache, so the suffix path
(the one that matters there) is exact.

## Backlog (Python-era fixes whose behavior the C port must match)

1. **context_length pin on model switch** (`ee51d485c1`): when `/model`
   returns to the configured default, the model's `context_length` pin must
   be restored. No `context_length` handling found in
   `src/hermes_cli/model_switch.c` / `port_model_switch.c` — port the rule
   and add an oracle. Python reference: `agent/agent_runtime_helpers.py`
   (file removed from this repo; read it from the archive tag or quarry
   history: `git show ee51d485c1`).
2. **Trailing reasoning delivery** (`b20fc56281`): when streaming already
   sent the body, the reasoning block must still be delivered.
   `src/gateway/stream_consumer.c` suppresses inline think blocks; verify
   the trailing-block path against `git show b20fc56281` and add an oracle.

## Upstream

The parent Python upstream (NousResearch/hermes-agent) is pulled via the
quarry clone at `C:\Users\eman5\hermes-agent` (origin = NousResearch).
`scripts/sync-upstream.sh` refreshes it; slermes never pushes C code back.
Parity scans read the quarry live, so a quarry pull immediately re-targets
every oracle and watercup gate.
