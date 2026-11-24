/*
 * turn_context_compaction.c — real C11 port of upstream Hermes
 * agent/turn_context_compaction.py (gate w17_turncontex_eb8405).
 *
 * PoP: w17_turncontex_eb8405_blocked_reason @ agent/turn_context_compaction.py:_blocked_compress_reason
 * PoP: w17_turncontex_eb8405_codex_native @ agent/turn_context_compaction.py:_codex_native_auto_compaction
 *
 * The two pure helpers are exercised: the compressor's should_compress_info
 * collaborator is injected as explicit (have_info/should/reason/raise) values,
 * and the agent's api_mode / codex_app_server_auto_compaction attributes are
 * injected as explicit strings (NULL == missing/None). No agent mutation or
 * lazy collaborator is required by these two functions.
 */
#include "noc_w17_turncontex_eb8405.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local strdup clone (never named `dup`). */
static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *w17_turncontex_eb8405_blocked_reason(
    int have_info, int should_now, const char *reason, int do_raise,
    long long tokens, int has_attempts, long long attempts_spent) {
    char buf[96];
    int reason_falsy;
    (void)tokens; /* forwarded to the injected info() but not used by the port */
    if (!have_info) return NULL;       /* getattr(..., None) not callable */
    if (do_raise) return NULL;         /* info() raised -> None */
    reason_falsy = (!reason || reason[0] == '\0');
    if (has_attempts && should_now && reason_falsy) {
        snprintf(buf, sizeof(buf), "attempts_exhausted:%lld", attempts_spent);
        return dupstr(buf);
    }
    if (!reason) return NULL;          /* _reason is Python None */
    return dupstr(reason);             /* includes the empty string "" */
}

int w17_turncontex_eb8405_codex_native(const char *api_mode, const char *auto_compaction) {
    char low[256];
    size_t i, n;
    const char *val;
    if (!api_mode || strcmp(api_mode, "codex_app_server") != 0) return 0;
    val = (auto_compaction && auto_compaction[0]) ? auto_compaction : "native";
    n = strlen(val);
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)val[i]);
    low[n] = '\0';
    return (strcmp(low, "native") == 0 || strcmp(low, "off") == 0) ? 1 : 0;
}
