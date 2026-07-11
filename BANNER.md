# ╔══════════════════════════════════════════════════════════════╗
# ║           Slermes — C Translation                            ║
# ║         of Hermes Agent (Nous Research)                      ║
# ║                                                              ║
# ║  Build: Clean  │  v559 │  Tests: 36/36  │  Oracle: 23/0 + 22/0 + 22/0 + 12/0 + 4/0 + 16/0 + 21/0 + 25/0 + 1611/0 fuzz ║
# ║  Ported: 4,879/9,731 (50.1%)  REAL_GAP: 4,801 (49.3%)  PARTIAL: 48  ║
# ║  ✓ monolith split x8 (oracle-verified, no double-coding):     ║
# ║    cron_prompt_sanitize + file_text_ops + file_fs_ops +        ║
# ║    file_pagination_ops + browser_redact + web_base64_img +     ║
# ║    skills_sync_fs (dir_hash MD5 / safe_rel / compute_relative) ║
# ║    image_gen_path (looks_like_absolute_file_path, 16/0) +      ║
# ║    send_message_target (parse_target_ref/display/retry, 21/0)  ║
# ║  ✓ v558: repaired corrupted port_cronjob_tools.c (N| prefixes + ║
# ║    truncation) + 4 faithful ports; oracle 21/0; cronjob REAL_GAP=0 ║
# ║  ✓ v559 DOCTRINE: "honest NA" fake-success demotions of failable ║
# ║    fns are BANNED — rewrite in C is the point. cronjob_dispatch + ║
# ║    execute_job_now now delegate to the REAL scheduler ║
# ║    (cron_cmd_handler: CRUD+fire); notify calls real provider ║
# ║    notify. WIRED orphaned port_scheduler.o. Oracle 25/0. ║
# ║  ✓ 0 STUB / 0 N/A  — no god headers, no void* passthrough    ║
# ╚══════════════════════════════════════════════════════════════╝
