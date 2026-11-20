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

# Devil's-advocate worklist (measured, marked for work)

Audit of parity claims against the doctrine: a claimed port is guilty until
proven. PoP annotation != parity; existence != verification; source != binary.
Scans run against the live tree and quarry; raw data in
`doc_devils_audit.json` / `doc_devils_refined.json` (session scratch, not committed).

## Findings

1. **False-green watercup verdicts.** 2076 of 2,115 gates
   classified PORTED have `ok:false` (only 39 verified ok:true; 445 carry
   mismatches>0). Cause: `fix_all_gates.py` / `fix_ported_gates.py` set
   `classification=PORTED` when a harness is (re)generated for a tagged C
   function — existence, not equivalence. WORK: split CLAIMED vs VERIFIED;
   fleet summaries must count ok:true && mismatches==0 only. Do not rename
   classification mid-fleet (gate dir names derive from it); add a verified
   field and fix the reporting. Then triage the ok:false residue into
   infra-bug / real-divergence / stub.

2. **Dead parity credit: 628 PoP-tagged C files are in NO
   link list** (Makefile + build/*.mk checked; nm-verified absent from
   slermes.exe: async_coro_new, agent_init_warn_memory_provider_unavailable).
   They claim ports that never compile into the binary. WORK: triage each —
   (a) wire into objects.mk + build + nm-sweep, (b) SAC-excluded by design
   (document), (c) strip the PoP claim. Add scanner DA-5 BUILT-LINK check.

3. **Stub bodies under PoP tags: 653 candidates in 262 files**
   (empty bodies or `(void)args; return const;`). 414 of them also sit in
   unbuilt files (double fake). Verified sample: src/cli/cli_gaps.c is a
   grab-bag of empty stubs claiming hermes_cli/plugins_registrars, pairing,
   partial_compress, local_runtime/detect, browser_supervisor_capture fns.
   WORK: re-port each cluster with real behavior + leaf oracle, or strip the
   tag. Top clusters:

   | stubs | file |
   |---|---|
   | 88 | `src/cli/cli_gaps.c` |
   | 22 | `src/hermes_cli/setup_gaps.c` |
   | 18 | `lib/libplugin/plugin.c` |
   | 14 | `src/hermes_cli/gateway_gaps2.c` |
   | 10 | `src/agent/conversation_compression_gaps2.c` |
   | 10 | `src/hermes_cli/update_cmd_gaps.c` |
   | 9 | `src/agent/auxiliary_client_gaps.c` |
   | 9 | `src/hermes_cli/local_runtime/gguf_gaps.c` |
   | 7 | `lib/libskillusage/skill_usage.c` |
   | 7 | `src/agent/agent_gaps.c` |
   | 7 | `src/agent/model_metadata_gaps.c` |
   | 6 | `src/agent/conversation_compression_gaps.c` |
   | 6 | `src/gateway/gateway_gaps.c` |
   | 6 | `src/gateway/run_gaps.c` |
   | 6 | `src/plugins/platforms/qqbot/qqbot_constants.c` |
   | 5 | `src/agent/context_compressor_gaps.c` |
   | 5 | `src/agent/conversation_loop_gaps.c` |
   | 5 | `src/agent/redact_gaps.c` |
   | 5 | `src/cron/scheduler_gaps.c` |
   | 5 | `src/gateway/session_gaps.c` |

4. **TODO/stub markers inside tagged functions: 195.**
   WORK: same queue as (3); a marker is an admission, not a port.

5. **Orphan claims: 3199 tags target Python functions that no
   longer exist in the quarry** (AST-verified incl. methods, assigns, imports;
   984 more use unresolvable legacy module keys).
   Top modules: gateway/run.py 162, tools/browser_tool.py 147,
   plugins/platforms/weixin/adapter.py 116, qqbot/adapter.py 96.
   WORK: evidence-based re-target to renamed live fns (unique-match rule),
   else strip the tag. Never re-target on guesswork.

6. **Duplicate claims: 1412 py functions claimed by 2+ C
   functions** (all different-name; some are legitimate complementary split
   ports). WORK: review; keep splits only where both halves are real and
   non-overlapping; dedupe the rest.

7. **Thin flow fixtures: 11 files with functions under
   5 cases** (worst: 3-case functions in api_server_flow, auxiliary_client_flow,
   credential_pool_flow, scheduler_flow). WORK: extend generators to >=5
   branch-covering cases per function; a 3-case function is untested branches.

## Clean results (devil's advocate found no fault)

- Duplicate global symbols across built objects: **0** (the 9 nm hits were
  PE `.refptr.*` artifacts, not definitions).
- All 14 flow harnesses call real port symbols (gwb_/acf_/ccflow_/sf_/
  cp_flow_/scf_/ttlf_/gwsl_/cfgls_/kdbf_/grun_flow_/cc_flow_/cch_flow_);
  none echo fixture traces. The mismatch-debug history itself proves the
  comparisons are live.
- Verified-green flow oracles stand on central re-runs, not agent self-reports.

## Priority order

1. False-green reporting fix (stops fake gains at the source).
2. Stub clusters + unbuilt files (largest fake-credit volume; dispatch as
   agent waves with leaf oracles, biggest clusters first).
3. Scanner DA-5 built-link criterion (makes the gap map binary-honest).
4. Orphan tag cleanup, duplicate review, fixture extension.

Honest gap totals pending: `scripts/parity_truth.py` re-run in flight; its
DA-4 stub exclusion already discounts annotation-only claims. The 9,317/
37,307 headline predates this audit and WILL shrink when DA-5 lands.

## Triple-check verdict (independent second pass + live AST behavior analysis)

Of the 653 stub-body candidates: **503 GUILTY** (live function has real
logic — branches/calls/state — while the C body is empty or constant),
**21 FAITHFUL** (live also returns a constant; the port is honest),
**116 ORPHAN STUBS** (live fn deleted upstream — tag-strip queue),
**38 extraction false positives** (second-pass brace match found real
bodies; first pass mis-attributed — 13 removed from the guilty count).
Method: `stub_triplecheck.py` — independent C body re-extraction + quarry
AST behavior scoring (If/For/Try/With=3, Call/BoolOp/Compare/Raise=2,
Assign=1); a constant-return port is only lazy if live does more.

Revival wave 1 (5 agents, in flight): cli_gaps ×2, setup_gaps, plugin.c,
gateway gaps — ~139 guilty fns. Wave 2 lists prepared (unowned clusters):
update_cmd_gaps 9, conversation_compression_gaps* 15, skill_usage+
model_metadata 13, agent_gaps+conversation_loop_gaps+scheduler_gaps 16,
tail 311 across ~180 files. auxiliary_client_gaps (9) held until the
wave-3 aux debugger finishes.

Bulk wiring: 612 unwired tagged files with >=1 real fn batch-built;
only clean-built objects get wired (latent compile errors in never-built
files are themselves a work queue — e.g. computer_use.c incompatible
pointer types).
