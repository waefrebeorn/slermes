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
# ║  Build: Clean  │  v563 │  Tests: 36/36  │  Oracle: 23/0 + 22/0 + 22/0 + 12/0 + 4/0 + 16/0 + 21/0 + 25/0 + 4/0 + 18/0 + 15/0 + 20/0 + 1611/0 fuzz ║
# ║  Ported: 4,890/9,731 (50.2%)  REAL_GAP: 4,779 (49.1%)  PARTIAL: 48  ║
# ║  ✓ v562: cron/suggestions.py FULLY PORTED (10 fns) oracle 15/0. ║
# ║  ✓ v563: hermes_cli/logs.py FULLY PORTED (11 fns, new ║
# ║    src/cli/port_cli_logs.c + header) — pure log view/filter: parse_since, ║
# ║    parse_line_timestamp, extract_level, extract_logger_name, ║
# ║    line_matches_component, matches_filters, read_last_n_lines, ║
# ║    read_tail, tail, follow, list. POSIX-ERE regex (no \s/\S/?:). ║
# ║    Oracle 20/0 vs LIVE Python (fixture log, filtered tail, listing). ║
# ║    Bug fixed: hermes_regex never sets group_count -> compared ║
# ║    groups[i]!=NULL instead. REAL_GAP=0. ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
