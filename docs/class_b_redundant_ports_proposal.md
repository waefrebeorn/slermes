# Class-B Redundant Parallel Ports — Adoption / Deletion Proposal

During the "wired-to-nothing" assembly audit (`docs/wired_to_nothing_census.md`)
we found that a large fraction of the ~6,128 dead functions are **not** missing
callers. They are **complete alternative implementations that duplicate a live
subsystem**. Wiring them would violate AGENTS.md ("extend, don't duplicate",
"no dead code wired in without E2E proof") and risk forking behavior.

These are **architecture decisions**, not missing wires. This document proposes
the disposition for each Class-B cluster. Default recommendation: **DELETE** the
parallel port unless there is a concrete reason the live one cannot serve its
role, in which case **ADOPT** (promote the port to canonical and delete the
live one).

---

## B1 — Gateway inbound API server (`gateway/platforms/api_server_adapter_*.c`)

- **Dead functions:** ~1,100 (`api_server_handle_*`, `api_server_adapter_*`).
- **Live equivalent:** `src/api_server.c` — a complete raw-socket OpenAI-
  compatible REST server (`main.c` → `api_server_start`, linked in `DEPS_OBJ`).
  It already implements `/health`, `/v1/models`, `/v1/chat/completions`,
  `/v1/responses`, `/v1/sessions`, `/v1/skills`, `/v1/toolsets`, etc.
- **Relationship:** The adapter is the *gateway-integrated* variant (routes
  through `api_server_adapter_create/connect/send` and the gateway's agent
  runtime). `api_server.c` is the *standalone* variant. Both implement the
  same OpenAI surface; only one ships.
- **Recommendation: ADOPT-or-DELETE (human decision).**
  - **Option A (keep `api_server.c`):** Delete `gateway/platforms/api_server_adapter_*.c`
    and `gateway/port_gateway_platforms_api_server.c`. Lowest risk — the live
    server is proven and wired.
  - **Option B (adopt the adapter):** Make the gateway `run` start the adapter
    server (build the missing listener/router that calls `api_server_handle_*`),
    delete `src/api_server.c`, and repoint `main.c`'s `api_server_start` to the
    adapter. Higher risk — requires building the adapter's HTTP listener and
    E2E-testing parity with `api_server.c`.
- **Why not just wire it:** No missing caller — it needs a whole HTTP server
  loop. That is a build task (Class C), and doing it *in addition to*
  `api_server.c` produces two servers. Pick one.

## B2 — Skill-tool wrappers (`tools/port_skills_tool_wrappers.c`)

- **Dead functions:** `sklt_*` (~970) — `sklt_u_skills_scan_signature`,
  `sklt_skill_matches_platform`, `sklt_skill_normalize_name`,
  `sklt_u_load_skill_frontmatter`, etc.
- **Live equivalent:** `skills_hub.c`, `agent/skill_*.c`, `cli/cli_cmd_skills.c`
  — the live skill load/normalize/match path.
- **Relationship:** Parallel port of Python's `skills_tool.py` helpers. The C
  port reimplemented skill handling natively; these wrappers were never
  switched on.
- **Recommendation: DELETE.** The live skill system covers the same surface.
  Before deleting, grep each `sklt_*` for any *unique* logic not present in the
  live path (e.g., a signature-scan detail) and port that one function into
  `skills_hub.c` if found.

## B3 — Agent "remaining wrappers" (`agent/port_agent_remaining_wrappers.c`)

- **CORRECTION (reclassified from earlier draft):** This cluster is **Class A
  (unique orphans), NOT Class B (redundant).** Verified: 15 of its functions
  have NO live implementation anywhere in src/ (e.g.
  `should_parallelize_tool_batch`, `is_untrusted_tool`,
  `tool_output_risk_metadata`, `maybe_wrap_untrusted`,
  `plan_tool_batch_segments`, `paths_overlap`, `is_multimodal_tool_result`,
  `trajectory_normalize_msg`). A few overlap with live code
  (`is_mcp_tool_parallel_safe` in `tools/mcp_tool.c`,
  `extract_file_mutation_targets`/`make_tool_result_message` in sibling port
  files) but the bulk are the ported Python tool-dispatch parallelization /
  untrusted-tool logic with no live caller yet.
- **Disposition: WIRE (Class A), do NOT delete.** The live parallel-tool path
  (`agent/conversation_loop.c`, `agent/port_agent_tool_executor.c`) handles
  parallel calls but does not yet call these `u_*` helpers. Wiring them
  requires care (must integrate with the live parallel path, not a blind call)
  — track as Class-A assembly work, not a delete.
- Note: `agent_display_build_status_phrase` (also in this file) remains a
  wrong-contract stub (C `(const char*)->int` != Python
  `(tool_name,args,max_len)->str`); leave documented, port its dependency
  chain in a dedicated task if needed.

---

## Process for resolving Class-B

1. For each cluster, run the recommended grep for *unique* logic vs the live
   path. If unique logic exists, port it into the live module (Class-A style).
2. Delete the redundant parallel port file.
3. Re-run `docs/wired_to_nothing_census.md` regeneration to confirm the count
   drops and no live caller broke.
4. Keep this proposal updated with the chosen option per cluster.

## Status

- B1: OPEN — needs human keep-`api_server.c` vs adopt-adapter decision.
- B2: Proposed DELETE (after unique-logic salvage).
- B3: Proposed DELETE (after unique-logic salvage; `build_status_phrase` stub
  deleted outright).

These are intentionally NOT force-wired. They are documented so the
keep-or-delete call is made once, consciously, rather than by accidental
wiring that duplicates live infrastructure.
