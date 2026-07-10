# ╔══════════════════════════════════════════════════════════════╗
# ║           Slermes — C Translation                            ║
# ║         of Hermes Agent (Nous Research)                      ║
# ║                                                              ║
# ║  Build: Clean  │  v555 │  Tests: 36/36  │  Oracle: 23/0 + 19/0 + 18/0 + 22/0 + 22/0 + 1611/0 fuzz ║
# ║  Ported: 4,881/9,731 (50.2%)  REAL_GAP: 4,802 (49.3%)  PARTIAL: 48  ║
# ║  ✓ residual-façade backlog: 11/12 honest demotions (no fake   ║
# ║    success); every remaining gap is genuine REAL_GAP         ║
# ║  ✓ monolith split x4 (oracle-verified): cron_prompt_sanitize  ║
# ║    + file_text_ops + file_fs_ops + file_pagination_ops +     ║
# ║    browser_redact extracted as self-contained C11 modules    ║
# ║  ✓ browser_redact: faithful POSIX-ERE port of agent.redact   ║
# ║    (vendor prefixes, auth headers, DB urls, JWT, cdp urls)   ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
