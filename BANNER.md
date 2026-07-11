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
# ║  Build: Clean  │  v568 │  Tests: 36/36  │  Oracle: 17 suites 0 mismatch + 1611/0 fuzz ║
# ║  Ported: 4,897/9,731  REAL_GAP: 4,772  PARTIAL: 48  ║
# ║  ✓ v562: cron/suggestions.py FULLY PORTED (10 fns) oracle 15/0. ║
# ║  ✓ v563: hermes_cli/logs.py FULLY PORTED (11 fns) oracle 20/0. ║
# ║  ✓ v564: agent/learning_graph.py +3 pure transforms oracle 6/0. ║
# ║  ✓ v565: agent/message_sanitization.py FULLY PORTED (11/11) oracle 5/0. ║
# ║  ✓ v566: agent/video_gen_provider.py FULLY PORTED (13/13) oracle 1/0. ║
# ║  ✓ v567: agent/file_safety.py FULLY PORTED (15/15) oracle 1/0. ║
# ║  ✓ v568: hermes_cli/partial_compress.py FULLY PORTED (4/4) — added ║
# ║    _coerce_keep (cmd_compress_coerce_keep, in cli/commands.c). Oracle ║
# ║    11/0 vs LIVE (clamp [1,100], DEFAULT 2, non-int -> default). ║
# ║    Module REAL_GAP=0. ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
