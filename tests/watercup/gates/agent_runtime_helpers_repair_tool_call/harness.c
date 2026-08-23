/* gate: agent_runtime_helpers_repair_tool_call — traced copy of
 * src/tools/registry.c::repair_tool_call (+ its static helpers name_norm /
 * name_camel_to_snake / name_strip_tool_suffix / levenshtein_*).
 * LIVE twin: agent/agent_runtime_helpers.py::repair_tool_call.
 *
 * The real registry.c drags pthread + tool-result/idempotency deps into the
 * link, so this is a line-faithful traced copy of its repair pipeline over a
 * fixed valid-name table (the registry contents the twin mirrors as a set).
 *
 * Shared branch ladder (twin.py computes each rung with LIVE's own helpers;
 * harness uses registry.c's own helpers):
 *   B1 empty input
 *   B2 lowered direct match
 *   B3 normalized (lower + -/space -> _) match
 *   B4 CamelCase->snake_case match
 *   B5 any suffix-stripped variant match (2 strip rounds, LIVE set semantics)
 * Value: final repaired name from the REAL pipeline incl. fuzzy rung
 *        (C: levenshtein ratio > 0.7; LIVE: difflib cutoff 0.7), "" for None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static const char *const NAMES[] = {
    "terminal", "read_file", "write_file", "web_search", "todo_tool", NULL
};

static int names_contains(const char *s) {
    if (!s || !s[0]) return 0;
    for (int i = 0; NAMES[i]; i++)
        if (strcmp(s, NAMES[i]) == 0) return 1;
    return 0;
}

/* ── verbatim from src/tools/registry.c ─────────────────────────────── */
static char *name_norm(const char *s) {
    if (!s) return NULL;
    char *r = strdup(s);
    if (!r) return NULL;
    for (char *p = r; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
        if (*p == '-' || *p == ' ') *p = '_';
    }
    return r;
}

static char *name_camel_to_snake(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t len = strlen(s), out_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && isupper((unsigned char)s[i]) && islower((unsigned char)s[i - 1]))
            out_len++;
        out_len++;
    }
    char *r = malloc(out_len + 1);
    if (!r) return NULL;
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && isupper((unsigned char)s[i]) && islower((unsigned char)s[i - 1]))
            r[pos++] = '_';
        r[pos++] = (char)tolower((unsigned char)s[i]);
    }
    r[pos] = '\0';
    return r;
}

static char *name_strip_tool_suffix(const char *s) {
    if (!s || !s[0]) return NULL;
    size_t len = strlen(s);
    static const char *suffixes[] = {"_tool", "-tool", "tool"};
    for (size_t i = 0; i < 3; i++) {
        size_t slen = strlen(suffixes[i]);
        if (len > slen) {
            const char *tail = s + len - slen;
            bool match = true;
            for (size_t j = 0; j < slen; j++) {
                if (tolower((unsigned char)tail[j]) != (unsigned char)suffixes[i][j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                size_t new_len = len - slen;
                while (new_len > 0 && (s[new_len - 1] == '_' || s[new_len - 1] == '-'))
                    new_len--;
                return strndup(s, new_len);
            }
        }
    }
    return NULL;
}

static size_t levenshtein_distance(const char *s1, const char *s2) {
    size_t len1 = strlen(s1), len2 = strlen(s2);
    size_t *row = malloc((len2 + 1) * sizeof(size_t));
    if (!row) return (size_t)-1;
    for (size_t j = 0; j <= len2; j++) row[j] = j;
    for (size_t i = 1; i <= len1; i++) {
        size_t prev = row[0];
        row[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            size_t min = row[j - 1] + 1;
            if (row[j] + 1 < min) min = row[j] + 1;
            if (prev + cost < min) min = prev + cost;
            prev = row[j];
            row[j] = min;
        }
    }
    size_t dist = row[len2];
    free(row);
    return dist;
}

static double levenshtein_ratio(const char *s1, const char *s2) {
    if (!s1 || !s2) return 0.0;
    size_t len1 = strlen(s1), len2 = strlen(s2);
    if (len1 == 0 && len2 == 0) return 1.0;
    size_t max_len = len1 > len2 ? len1 : len2;
    return 1.0 - (double)levenshtein_distance(s1, s2) / (double)max_len;
}
/* ── end registry.c helpers ─────────────────────────────────────────── */

/* Faithful trace of registry.c::repair_tool_call over the fixed name table */
static char *wc_repair_tool_call(const char *tool_name) {
    WCUP_BEGIN("repair_tool_call");
    static char result[256];
    result[0] = '\0';

    char *lowered = NULL, *normalized = NULL, *camel = NULL;
    if (!tool_name || !tool_name[0]) {          /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);

    lowered = strdup(tool_name);
    for (char *p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* All rungs emitted unconditionally (twin mirrors this order); each
     * rung is gated by the previous ones exactly like twin.py. */
    lowered = strdup(tool_name);
    for (char *p = lowered; *p; p++) *p = (char)tolower((unsigned char)*p);
    normalized = name_norm(tool_name);
    camel = name_camel_to_snake(tool_name);

    int hit_lowered = names_contains(lowered);                /* B2 */
    WCUP_BRANCH(hit_lowered);
    if (hit_lowered) snprintf(result, sizeof result, "%s", lowered);

    int hit_norm = !hit_lowered && normalized && names_contains(normalized);  /* B3 */
    WCUP_BRANCH(hit_norm);
    if (hit_norm) snprintf(result, sizeof result, "%s", normalized);

    int hit_camel = !hit_norm && camel && names_contains(camel);              /* B4 */
    WCUP_BRANCH(hit_camel);
    if (hit_camel) snprintf(result, sizeof result, "%s", camel);

    /* 4. Variant set with suffix stripping (up to 2x) — same ADD_VAR set
     * construction as registry.c, bounded to 64 entries. */
#define MAX_VARIANTS 64
    char *variants[MAX_VARIANTS];
    size_t n_variants = 0;
    bool var_hit = false;

#define ADD_VAR(v) do { \
        if ((v) && n_variants < MAX_VARIANTS) { \
            bool _dup = false; \
            for (size_t _i = 0; _i < n_variants; _i++) { \
                if (strcmp(variants[_i], (v)) == 0) { _dup = true; break; } \
            } \
            if (!_dup) variants[n_variants++] = strdup(v); \
        } \
    } while (0)

    ADD_VAR(tool_name);
    ADD_VAR(lowered);
    ADD_VAR(normalized);
    ADD_VAR(camel);
    if (!hit_camel) {
        for (int round = 0; round < 2; round++) {
            size_t old_count = n_variants;
            for (size_t i = 0; i < old_count; i++) {
                char *stripped = name_strip_tool_suffix(variants[i]);
                if (stripped) {
                    ADD_VAR(stripped);
                    char *sn = name_norm(stripped);
                    ADD_VAR(sn);
                    free(sn);
                    char *sc = name_camel_to_snake(stripped);
                    ADD_VAR(sc);
                    free(sc);
                }
                free(stripped);
            }
        }
        for (size_t i = 0; i < n_variants; i++) {
            if (variants[i][0] && names_contains(variants[i])) {
                snprintf(result, sizeof result, "%s", variants[i]);
                var_hit = true;
            }
        }
    }
    for (size_t j = 0; j < n_variants; j++) free(variants[j]);
    WCUP_BRANCH(var_hit);                                    /* B5 */
    if (var_hit) goto out_f;

    /* 5. Fuzzy: levenshtein ratio > 0.7 (registry.c real rung) */
    const char *best_match = NULL;
    double best_ratio = 0.7;
    for (int i = 0; NAMES[i]; i++) {
        double ratio = levenshtein_ratio(lowered, NAMES[i]);
        if (ratio > best_ratio) { best_ratio = ratio; best_match = NAMES[i]; }
    }
    if (best_match) snprintf(result, sizeof result, "%s", best_match);

out_f:
    free(lowered); free(normalized); free(camel);
out:
    {
        char cup[32];
        WCUP_VALSTR(result);
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("repair_tool_call");
        return strdup(result);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s repair_tool_call <tool_name>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        free(wc_repair_tool_call(argv[i]));
    }
    return 0;
}
