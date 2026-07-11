# ╔══════════════════════════════════════════════════════════════╗
# ║           Slermes — C Translation                            ║
# ║         of Hermes Agent (Nous Research)                      ║
# ║                                                              ║
# ║  Build: Clean  │  v562 │  Tests: 36/36  │  Oracle: 23/0 + 22/0 + 22/0 + 12/0 + 4/0 + 16/0 + 21/0 + 25/0 + 4/0 + 18/0 + 15/0 + 1611/0 fuzz ║
# ║  Ported: 4,879/9,731 (50.1%)  REAL_GAP: 4,790 (49.2%)  PARTIAL: 48  ║
# ║  ✓ v560: copilot_acp_client 2 pure struct-builders ported (oracle 4/0); ║
# ║    copilot_acp_client.py REAL_GAP=0. managed_modal._request_timeout_env ║
# ║    already ported (scanner prefix false-pos). ║
# ║  ✓ v561 DOCTRINE CORRECTION: "un-C-able" dismissals are WRONG — everything ║
# ║    is REAL_GAP work. Verified yuanbao MarkdownProcessor 9 helpers ARE ║
# ║    ported (yuanbao_md_*, scanner correctly marks PORTED); oracle 18/0. ║
# ║    Fixed 3 real C divergences from LIVE Python. ║
# ║  ✓ v562: cron/suggestions.py FULLY PORTED (10 fns, new ║
# ║    src/cron/cron_suggestions.c + header) — JSON file store w/ lock + ║
# ║    atomic write + 0600 perms, integrates w/ real cron_add_job. Oracle ║
# ║    15/0 vs LIVE Python lifecycle. REAL_GAP=0. ║
# ║    Caught + fixed: mutex unlock-on-success-path (deadlock), ║
# ║    get() returning borrowed ref for digit-leading uuids (UAF), ║
# ║    and 3 aliasing-ownership double-frees (libjson steals/borrows). ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
