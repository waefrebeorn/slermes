/* gate: agent_shell_hooks_parse_response — traced copy of
 * src/agent/shell_hooks_ports.c::shk_parse_response (+ shk_block_message).
 * LIVE twin: agent/shell_hooks.py::_parse_response (pre_tool_call /
 * pre_llm_call rungs).
 *
 * Shared branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty/whitespace stdout -> None
 *   B2: JSON parse fail or non-object -> None
 *   B3: block directive (action=="block" || decision=="block")
 *       B3a primary message field wins / B3b fallback / else default
 *   B4: context non-empty string -> pass-through
 *   B5: exhausted -> None
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* --- minimal JSON object scanner (flat, mirrors libjson usage in the port:
 *     we only need top-level string fields of a JSON object) --- */
static char *json_field_str(const char *s, const char *key) {
    /* find "key" at top level of a flat object; naive but faithful enough
     * for these payloads (no escaped quotes in gate inputs). */
    size_t klen = strlen(key);
    const char *p = s;
    int depth = 0;
    while (*p) {
        if (*p == '"') {
            const char *ks = p + 1;
            const char *ke = ks;
            while (*ke && *ke != '"') ke++;
            if (!*ke) return NULL;
            if (depth <= 1 && (size_t)(ke - ks) == klen && strncmp(ks, key, klen) == 0) {
                const char *q = ke + 1;
                while (*q == ' ' || *q == ':' ) q++;
                if (*q == '"') {
                    q++;
                    const char *ve = q;
                    while (*ve && *ve != '"') ve++;
                    char *out = malloc((size_t)(ve - q) + 1);
                    memcpy(out, q, (size_t)(ve - q));
                    out[ve - q] = '\0';
                    return out;
                }
                return NULL; /* non-string value */
            }
            p = ke + 1;
        } else if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') depth--;
        p++;
    }
    return NULL;
}

static int looks_like_object(const char *s) {
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    return *s == '{';
}
static int json_valid_ish(const char *s) {
    /* gate payloads are well-formed except deliberate junk; treat
     * leading '{' with balanced quotes/braces as parsed-ok. */
    if (!looks_like_object(s)) return 0;
    int depth = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) return 1; }
    }
    return depth == 0;
}

static char *wc_block_message(const char *primary, const char *fallback) {
    if (primary && *primary) return strdup(primary);      /* B3a */
    if (fallback && *fallback) return strdup(fallback);   /* B3b */
    return strdup("Blocked by shell hook.");
}

static char *wc_parse_response(const char *stdout_json) {
    WCUP_BEGIN("parse_response");
    char *result = NULL;
    const char *s = stdout_json ? stdout_json : "";
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    if (!*s) {                                            /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    if (!json_valid_ish(s) || !looks_like_object(s)) {    /* B2 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char *action = json_field_str(s, "action");
        char *decision = json_field_str(s, "decision");
        char *context = json_field_str(s, "context");
        int is_block = (action && strcmp(action, "block") == 0) ||
                       (decision && strcmp(decision, "block") == 0);
        free(action); free(decision);
        if (is_block) {                                   /* B3 */
            WCUP_BRANCH(1);
            char *pa = json_field_str(s, "action");
            const char *primary, *fallback;
            if (pa && strcmp(pa, "block") == 0) {
                primary = json_field_str(s, "message");
                fallback = json_field_str(s, "reason");
            } else {
                primary = json_field_str(s, "reason");
                fallback = json_field_str(s, "message");
            }
            free(pa);
            result = wc_block_message(primary, fallback);
            free((char *)primary); free((char *)fallback);
            goto out;
        }
        WCUP_BRANCH(0);
        if (context && *context) {                        /* B4 */
            WCUP_BRANCH(1);
            result = strdup(s);
            free(context);
            goto out;
        }
        WCUP_BRANCH(0);                                   /* B5 */
        free(context);
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_response");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_response <stdout-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *r = wc_parse_response(argv[i]);
        free(r);
    }
    return 0;
}
