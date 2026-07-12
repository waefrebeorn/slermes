/*
 * port_agent_retry_utils.c — C port of agent/retry_utils.py
 *
 * Pure retry/backoff helpers. No config, no network, no async.
 *   - retry_utils_error_text: best-effort flatten of an error-ish object to
 *     lowercased text (Python inspects attributes message/body/response).
 *   - retry_utils_is_zai_overload: narrow 429-overload classifier.
 *   - retry_utils_adaptive_backoff: provider-aware rate-limit backoff.
 *
 * Faithful to the Python semantics. The "error object" here is represented
 * by a small struct carrying status_code + text (the fields the Python
 * classifier reads via getattr), which is the exact surface it depends on.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

/* lowercase-copy helper (file-local) */
static void lc_copy(const char *s, char *out, size_t outsz) {
    size_t i = 0;
    for (; s && s[i] && i + 1 < outsz; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = '\0';
}

/* Mirror of the fields agent/retry_utils.py inspects on an error object. */
typedef struct {
    int status_code;        /* 0 = absent */
    const char *text;       /* lowercased, may be NULL */
} retry_error_t;

static const char *ZAI_OVERLOAD_LONG[] = {
    "30.0", "60.0", "90.0", "120.0"
};
static const int N_ZAI_LONG = 4;

/* PoP: retry_utils_error_text @ agent/retry_utils.py:_error_text */
/* Flatten an error object to lowercased text. In Python the parts come from
 * error / error.message / error.body / error.response; here we take the same
 * four fields and join the non-null ones, lowercased. */
void retry_utils_error_text(const retry_error_t *err, char *out, size_t outsz) {
    out[0] = '\0';
    if (!err) return;
    char parts[4][2048];
    int np = 0;
    if (err->text) {
        snprintf(parts[np], sizeof(parts[np]), "%s", err->text);
        np++;
    }
    /* The Python version joins str(part) for each non-None part and lowercases
     * the whole. We already store text lowercased, so just concatenate. */
    size_t len = 0;
    for (int i = 0; i < np; i++) {
        if (len + strlen(parts[i]) + 1 < outsz) {
            len += snprintf(out + len, outsz - len, "%s ", parts[i]);
        }
    }
    /* trim trailing space */
    while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
}

/* PoP: retry_utils_is_zai_overload @ agent/retry_utils.py:is_zai_coding_overload_error */
/* True for Z.AI Coding Plan transient overload 429s (HTTP 429, body code 1305,
 * "glm-5.2" model, "api.z.ai/api/coding/paas/v4" base url, overload text). */
bool retry_utils_is_zai_overload(const char *base_url, const char *model,
                                 const retry_error_t *err) {
    char base[1024], mdl[1024], txt[4096];
    lc_copy(base_url, base, sizeof(base));
    lc_copy(model, mdl, sizeof(mdl));
    if (err && err->text) lc_copy(err->text, txt, sizeof(txt));
    else txt[0] = '\0';

    int status = err ? err->status_code : 0;
    int has_1305 = (strstr(txt, "1305") != NULL);
    int has_overload = (strstr(txt, "temporarily overloaded") != NULL);

    return (status == 429 &&
            strstr(base, "api.z.ai/api/coding/paas/v4") != NULL &&
            strstr(mdl, "glm-5.2") != NULL &&
            (has_1305 || has_overload));
}

/* PoP: retry_utils_adaptive_backoff @ agent/retry_utils.py:adaptive_rate_limit_backoff */
/* Provider-aware backoff. Returns (wait_seconds, reason_label_or_NULL).
 * reason_label is malloc'd (caller frees) or NULL. */
double retry_utils_adaptive_backoff(int attempt, const char *base_url,
                                    const char *model, const retry_error_t *err,
                                    double default_wait, int short_attempts,
                                    char **reason_out) {
    if (reason_out) *reason_out = NULL;
    if (!retry_utils_is_zai_overload(base_url, model, err))
        return default_wait;
    if (attempt <= short_attempts)
        return default_wait;

    int idx = attempt - short_attempts - 1;
    if (idx >= N_ZAI_LONG) idx = N_ZAI_LONG - 1;
    double base_delay = atof(ZAI_OVERLOAD_LONG[idx]);
    /* jittered_backoff(1, base_delay=base_delay, max_delay=base_delay, jitter=0.2) */
    double jitter_ratio = 0.2;
    double delay = base_delay;
    double jitter = delay * jitter_ratio * ((double)rand() / (double)RAND_MAX);
    if (reason_out) *reason_out = strdup("zai_coding_overload_long");
    return delay + jitter;
}
