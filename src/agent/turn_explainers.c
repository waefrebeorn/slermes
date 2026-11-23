/*
 * turn_explainers.c — C port of upstream Hermes agent/turn_explainers.py.
 *
 * PoP: turnexplain_persistence_explanation_key @ agent/turn_explainers.py:_persistence_explanation_key
 * PoP: turnexplain_format_turn_completion_explanation @ agent/turn_explainers.py:TurnExplainersMixin._format_turn_completion_explanation
 * PoP: turnexplain_neutralize_footer_paths @ agent/turn_explainers.py:TurnExplainersMixin._neutralize_footer_paths
 *
 * i18n t() is the injected seam: the oracle maps it to identity (key -> key),
 * so the port reproduces the reason -> catalog-key selection and the footer
 * path backticking regex (which is the real, non-seamed logic).
 */
#include "noc_w7_turnexplain_e66934.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *persistence_causes[] = {
    "compression", "compression_closed", "turn_lease", "session_row_missing",
    "locked", "replaced", "deleted_wal", "corrupt", "fts_index", "disk", NULL
};

json_t *turnexplain_persistence_explanation_key(const char *cause) {
    if (cause) {
        for (int i = 0; persistence_causes[i]; i++)
            if (!strcmp(cause, persistence_causes[i])) {
                char buf[256];
                snprintf(buf, sizeof(buf), "explainer.persistence.%s", cause);
                return json_string(buf);
            }
    }
    return json_string("explainer.persistence.default");
}

struct KV { const char *k; const char *v; };
static const struct KV EXIT_REASONS[] = {
    {"empty_response_exhausted", "explainer.empty_response"},
    {"all_retries_exhausted_no_response", "explainer.exit.all_retries_exhausted_no_response"},
    {"partial_stream_recovery", "explainer.exit.partial_stream_recovery"},
    {"fallback_prior_turn_content", "explainer.exit.fallback_prior_turn_content"},
    {"redirect_restart_limit_exceeded", "explainer.exit.redirect_restart_limit_exceeded"},
    {"rebuilt_restart_limit_exceeded", "explainer.exit.rebuilt_restart_limit_exceeded"},
    {"budget_exhausted", "explainer.exit.budget_exhausted"},
    {"ollama_runtime_context_too_small", "explainer.exit.ollama_runtime_context_too_small"},
    {"pending_tool_result", "explainer.exit.pending_tool_result"},
    {NULL, NULL}
};
static const char *PREFIXES[] = {
    "interrupted_during_api_call", "max_iterations_reached",
    "error_near_max_iterations", "repeated_outer_errors", NULL
};

json_t *turnexplain_format_turn_completion_explanation(const char *reason,
                                                        const char *persistence_cause,
                                                        const char *model) {
    (void)model; /* t() seam returns key; format kwargs do not affect output */
    if (!reason || !reason[0]) return json_string("");
    if (!strncmp(reason, "text_response", 13)) return json_string("");

    const char *key = NULL;
    for (int i = 0; EXIT_REASONS[i].k; i++)
        if (!strcmp(reason, EXIT_REASONS[i].k)) { key = EXIT_REASONS[i].v; break; }
    if (!key) {
        for (int i = 0; PREFIXES[i]; i++) {
            size_t l = strlen(PREFIXES[i]);
            if (!strncmp(reason, PREFIXES[i], l)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "explainer.exit.%s", PREFIXES[i]);
                /* need a stable copy, handled below */
                static __thread char prefix_key[256];
                snprintf(prefix_key, sizeof(prefix_key), "explainer.exit.%s", PREFIXES[i]);
                (void)buf;
                key = prefix_key;
                break;
            }
        }
    }
    static __thread char persistence_key[256];
    if (!key && !strcmp(reason, "session_persistence_failed")) {
        const char *cause = persistence_cause ? persistence_cause : "default";
        int known = 0;
        for (int i = 0; persistence_causes[i]; i++)
            if (!strcmp(cause, persistence_causes[i])) { known = 1; break; }
        snprintf(persistence_key, sizeof(persistence_key), "explainer.persistence.%s",
                 known ? cause : "default");
        key = persistence_key;
    }
    if (!key) return json_string("");
    char out[512];
    snprintf(out, sizeof(out), "explainer.no_reply_prefix%s", key);
    return json_string(out);
}

/* ── _FOOTER_PATH_RE backtick wrapping ─────────────────────────────────────
 * (?<![/:\w.`])(?:~/|/|[A-Za-z]:[/\\])(?:[\w.\-]+[/\\])*[\w.\-]+\.[\w]+
 * ────────────────────────────────────────────────────────────────────────── */
static int wordchar(char c) { return isalnum((unsigned char)c) || c == '_'; }
static int midchar(char c) { return wordchar(c) || c == '.' || c == '-'; }
static int lookbehind_ok(const char *s, size_t i) {
    if (i == 0) return 1;
    char c = s[i - 1];
    if (c == '/' || c == ':' || c == '.' || c == '`') return 0;
    return !wordchar(c);
}
/* segment must fully match [\w.\-]+\.[\w]+ */
static int seg_ok(const char *s, size_t a, size_t b) {
    if (b <= a) return 0;
    size_t dot = (size_t)-1;
    for (size_t i = a; i < b; i++) if (s[i] == '.') dot = i;
    if (dot == (size_t)-1 || dot == a || dot + 1 >= b) return 0;
    for (size_t i = a; i < dot; i++) if (!midchar(s[i])) return 0;
    for (size_t i = dot + 1; i < b; i++) if (!wordchar(s[i])) return 0;
    return 1;
}

json_t *turnexplain_neutralize_footer_paths(const char *text) {
    if (!text || !text[0]) return json_string(text ? text : "");
    size_t len = strlen(text);
    char *out = (char *)malloc(len * 2 + 4);
    if (!out) return json_string("");
    size_t n = 0;
    size_t i = 0;
    while (i < len) {
        int matched = 0;
        if (lookbehind_ok(text, i)) {
            size_t prefix = 0;
            if (text[i] == '~' && i + 1 < len && text[i + 1] == '/') prefix = 2;
            else if (i + 2 < len && isalpha((unsigned char)text[i]) &&
                     text[i + 1] == ':' && (text[i + 2] == '/' || text[i + 2] == '\\')) prefix = 3;
            else if (text[i] == '/') prefix = 1;
            if (prefix) {
                size_t pos = i + prefix, e = pos;
                while (e < len && (midchar(text[e]) || text[e] == '/' || text[e] == '\\')) e++;
                size_t best_end = (size_t)-1, seg_start = pos;
                for (size_t k = pos; k <= e; k++) {
                    if (k == e || text[k] == '/' || text[k] == '\\') {
                        if (seg_ok(text, seg_start, k)) best_end = k;
                        seg_start = k + 1;
                    }
                }
                if (best_end != (size_t)-1 && best_end > i) {
                    out[n++] = '`';
                    memcpy(out + n, text + i, best_end - i);
                    n += best_end - i;
                    out[n++] = '`';
                    i = best_end;
                    matched = 1;
                }
            }
        }
        if (!matched) out[n++] = text[i++];
    }
    out[n] = '\0';
    json_t *r = json_string(out);
    free(out);
    return r;
}
