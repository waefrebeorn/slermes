#ifndef NOC_W17_TURNCONTEX_EB8405_H
#define NOC_W17_TURNCONTEX_EB8405_H

/* Python Optional[str] is represented as a malloc'd C string, or NULL for None. */
char *w17_turncontex_eb8405_blocked_reason(
    int have_info, int should_now, const char *reason, int do_raise,
    long long tokens, int has_attempts, long long attempts_spent);

/* Python bool. NULL api_mode == missing/None; NULL auto_compaction == missing. */
int w17_turncontex_eb8405_codex_native(const char *api_mode, const char *auto_compaction);

#endif
