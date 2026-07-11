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
# ║  Build: Clean  │  v564 │  Tests: 36/36  │  Oracle: 13 suites 0 mismatch (…+ cli_logs 20/0 + learning_graph 6/0) + 1611/0 fuzz ║
# ║  Ported: 4,893/9,731  REAL_GAP: 4,776  PARTIAL: 48  ║
# ║  ✓ v562: cron/suggestions.py FULLY PORTED (10 fns) oracle 15/0. ║
# ║  ✓ v563: hermes_cli/logs.py FULLY PORTED (11 fns) oracle 20/0. ║
# ║  ✓ v564: agent/learning_graph.py +3 pure transforms (build_edges, ║
# ║    density_stats, _memory_skill_edges) extended into existing ║
# ║    port_learning_graph_helpers.c — corrects the old \"un-portable ║
# ║    REAL_GAP\" header claim per doctrine. Oracle 6/0 vs LIVE Python. ║
# ║    ported 6->9; 7 remaining gaps are filesystem-coupled (rglob/reads). ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
