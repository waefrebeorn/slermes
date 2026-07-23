/*
 * port_curses_ui.c — Faithful C11 port of pure helpers from
 * hermes_cli/curses_ui.py
 *
 * Ported: _query_matches, _is_boundary, _token_score, _fuzzy_score,
 * _filter_indices, _reconcile_cursor.
 * Curses/IO-coupled functions (_run_curses_menu, curses_checklist, etc.)
 * left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "json.h"
#include "curses_ui.h"

static const char *WORD_BOUNDARY = "-_/. ";

static int is_word_boundary(const char *target, int index) {
    if (index == 0) return 1;
    char prev = target[index - 1];
    for (const char *b = WORD_BOUNDARY; *b; b++) {
        if (prev == *b) return 1;
    }
    /* camelCase lower->upper transition: prev is lowercase, cur is uppercase */
    char cur = target[index];
    if (islower((unsigned char)prev) && isupper((unsigned char)cur)) return 1;
    return 0;
}

/* PoP: curses_query_matches @ hermes_cli/curses_ui.py:_query_matches */
int curses_query_matches(const char *label, const char *query) {
    if (!label || !query) return 0;
    /* normalize to lowercase */
    char normalized[1024];
    size_t i = 0;
    for (; label[i] && i < sizeof(normalized)-1; i++) {
        normalized[i] = (char)tolower((unsigned char)label[i]);
    }
    normalized[i] = '\0';

    /* split query into tokens */
    char qlower[1024];
    i = 0;
    for (; query[i] && i < sizeof(qlower)-1; i++) {
        qlower[i] = (char)tolower((unsigned char)query[i]);
    }
    qlower[i] = '\0';

    /* tokenize by whitespace */
    char *tokens[64];
    int ntokens = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(qlower, " \t", &saveptr);
    while (tok && ntokens < 64) {
        tokens[ntokens++] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    if (ntokens == 0) return 1;

    for (int t = 0; t < ntokens; t++) {
        int pos = 0;
        for (int j = 0; tokens[t][j]; j++) {
            char ch = tokens[t][j];
            char *found = strchr(normalized + pos, ch);
            if (!found) return 0;
            pos = (int)(found - normalized) + 1;
        }
    }
    return 1;
}

/* PoP: curses_token_score @ hermes_cli/curses_ui.py:_token_score */
double curses_token_score(const char *orig, const char *lower, const char *token, int *matched) {
    *matched = 0;
    if (!orig || !lower || !token || !*token) return 0.0;
    double score = 0.0;
    int prev = -1;
    int search_from = 0;
    int positions[256];
    int npos = 0;

    for (int j = 0; token[j]; j++) {
        char ch = token[j];
        char *found = strchr(lower + search_from, ch);
        if (!found) return -1.0;
        int idx = (int)(found - lower);
        if (npos < 256) positions[npos++] = idx;
        score += 1;
        if (prev >= 0 && idx == prev + 1) {
            score += 5;
        } else if (prev >= 0) {
            score -= (idx - prev - 1 < 3) ? (idx - prev - 1) : 3;
        }
        if (is_word_boundary(orig, idx)) score += 3;
        if (idx == 0) score += 5;
        prev = idx;
        search_from = idx + 1;
    }
    /* Prefix bonus: the token matched a contiguous prefix of the target. */
    if (npos > 0 && positions[0] == 0 && positions[npos-1] == npos - 1) {
        score += 8;
    }
    /* Exact full match dominates everything else. */
    if (strcmp(lower, token) == 0) {
        score += 20;
    }
    /* Slightly prefer shorter targets when scores are otherwise close. */
    score -= (double)strlen(lower) * 0.01;
    *matched = 1;
    return score;
}

/* PoP: curses_fuzzy_score @ hermes_cli/curses_ui.py:_fuzzy_score */
double curses_fuzzy_score(const char *label, const char *query, int *matched) {
    *matched = 0;
    if (!label || !query) return -1.0;
    char lower[1024];
    size_t i = 0;
    for (; label[i] && i < sizeof(lower)-1; i++) {
        lower[i] = (char)tolower((unsigned char)label[i]);
    }
    lower[i] = '\0';

    /* tokenize */
    char qlower[1024];
    i = 0;
    for (; query[i] && i < sizeof(qlower)-1; i++) {
        qlower[i] = (char)tolower((unsigned char)query[i]);
    }
    qlower[i] = '\0';

    char *tokens[64];
    int ntokens = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(qlower, " \t", &saveptr);
    while (tok && ntokens < 64) {
        tokens[ntokens++] = tok;
        tok = strtok_r(NULL, " \t", &saveptr);
    }

    if (ntokens == 0) { *matched = 1; return 0.0; }

    double total = 0.0;
    for (int t = 0; t < ntokens; t++) {
        int m = 0;
        double s = curses_token_score(label, lower, tokens[t], &m);
        if (!m) return -1.0;
        total += s;
    }
    *matched = 1;
    return total;
}

/* PoP: curses_filter_indices @ hermes_cli/curses_ui.py:_filter_indices */
json_t *curses_filter_indices(json_t *items, const char *query) {
    json_t *out = json_array();
    if (!items || items->type != JSON_ARRAY) return out;
    if (!query || !*query) {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) json_append(out, json_number((double)i));
        return out;
    }
    /* strip query */
    char q[1024];
    strncpy(q, query, sizeof(q)-1);
    q[sizeof(q)-1] = '\0';
    char *s = q;
    while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
    size_t l = strlen(s);
    while (l > 0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r'||s[l-1]=='\n')) s[--l]='\0';
    if (*s == '\0') {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) json_append(out, json_number((double)i));
        return out;
    }

    /* scored pairs: (index, score) */
    typedef struct { int idx; double score; } Pair;
    Pair pairs[1024];
    int npairs = 0;
    size_t n = json_len(items);
    for (size_t i = 0; i < n && npairs < 1024; i++) {
        json_t *item = json_get(items, i);
        if (!item || item->type != JSON_STRING) continue;
        int m = 0;
        double score = curses_fuzzy_score(item->str_val, s, &m);
        if (m) {
            pairs[npairs].idx = (int)i;
            pairs[npairs].score = score;
            npairs++;
        }
    }
    /* sort by (-score, idx) */
    for (int i = 0; i < npairs; i++) {
        for (int j = i+1; j < npairs; j++) {
            if (pairs[j].score > pairs[i].score ||
                (pairs[j].score == pairs[i].score && pairs[j].idx < pairs[i].idx)) {
                Pair tmp = pairs[i]; pairs[i] = pairs[j]; pairs[j] = tmp;
            }
        }
    }
    for (int i = 0; i < npairs; i++) {
        json_append(out, json_number((double)pairs[i].idx));
    }
    return out;
}

/* PoP: curses_reconcile_cursor @ hermes_cli/curses_ui.py:_reconcile_cursor */
void curses_reconcile_cursor(json_t *filtered, int cursor, int *out_cursor, int *out_pos) {
    if (!filtered || filtered->type != JSON_ARRAY || json_len(filtered) == 0) {
        *out_cursor = cursor;
        *out_pos = 0;
        return;
    }
    /* check if cursor in filtered */
    int found = 0;
    size_t n = json_len(filtered);
    for (size_t i = 0; i < n; i++) {
        if ((int)json_get(filtered, i)->num_val == cursor) { found = 1; *out_pos = (int)i; break; }
    }
    if (!found) {
        cursor = (int)json_get(filtered, 0)->num_val;
        *out_pos = 0;
    }
    *out_cursor = cursor;
}
