# ╔══════════════════════════════════════════════════════════════╗
# ║           Slermes — C Translation                            ║
# ║         of Hermes Agent (Nous Research)                      ║
# ║                                                              ║
# ║  Build: Clean  │  v556b │  Tests: 36/36  │  Oracle: 23/0 + 19/0 + 18/0 + 22/0 + 22/0 + 12/0 + 4/0 + 1611/0 fuzz ║
# ║  Ported: 4,881/9,731 (50.2%)  REAL_GAP: 4,802 (49.3%)  PARTIAL: 48  ║
# ║  ✓ monolith split x6 (oracle-verified, no double-coding):     ║
# ║    cron_prompt_sanitize + file_text_ops + file_fs_ops +        ║
# ║    file_pagination_ops + browser_redact + web_base64_img +     ║
# ║    skills_sync_fs (dir_hash MD5 / safe_rel / compute_relative) ║
# ║  ✓ web_base64_img: faithful POSIX-ERE port, killed strdup stub ║
# ║  ✓ skills_sync_fs: consolidated dir_hash (was inline+dead lib) ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
