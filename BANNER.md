# ╔══════════════════════════════════════════════════════════════╗
# ║           Slermes — C Translation                            ║
# ║         of Hermes Agent (Nous Research)                      ║
# ║                                                              ║
# ║  Build: Clean  │  v561 │  Tests: 36/36  │  Oracle: 23/0 + 22/0 + 22/0 + 12/0 + 4/0 + 16/0 + 21/0 + 25/0 + 4/0 + 18/0 + 1611/0 fuzz ║
# ║  Ported: 4,879/9,731 (50.1%)  REAL_GAP: 4,800 (49.3%)  PARTIAL: 48  ║
# ║  ✓ v560: copilot_acp_client 2 pure struct-builders ported (oracle 4/0); ║
# ║    copilot_acp_client.py REAL_GAP=0. managed_modal._request_timeout_env ║
# ║    already ported (scanner prefix false-pos). ║
# ║  ✓ v561 DOCTRINE CORRECTION: "un-C-able" dismissals are WRONG — everything ║
# ║    is REAL_GAP work. Verified yuanbao MarkdownProcessor 9 helpers ARE ║
# ║    ported (yuanbao_md_*, scanner correctly marks PORTED); oracle 18/0. ║
# ║    Fixed 3 real C divergences from LIVE Python: split_into_atoms trailing ║
# ║    newline, sanitize_markdown_table missing edge-pipes on separator rows, ║
# ║    markdown_hint_system_prompt literal-\n vs real-newline. The 148 yuanbao ║
# ║    REAL_GAPs are the async token-fetch + streaming middleware pipeline — ║
# ║    genuine C-rewritable work (libcurl+event loop), NOT demotable. ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
