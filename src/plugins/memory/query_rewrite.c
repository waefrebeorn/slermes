/*
 * query_rewrite.c -- real C11 port of the retrieval-question rules of upstream
 * Hermes plugins/memory/query_rewrite.py.
 *
 * PoP: w22_queryrewri_d659a2_bounded_user_message @ plugins/memory/query_rewrite.py:_bounded_user_message
 * PoP: w22_queryrewri_d659a2_extract_response_text @ plugins/memory/query_rewrite.py:_extract_response_text
 * PoP: w22_queryrewri_d659a2_normalize_rewrite @ plugins/memory/query_rewrite.py:_normalize_rewrite
 * PoP: w22_queryrewri_d659a2_rewrite @ plugins/memory/query_rewrite.py:rewrite_memory_query
 *
 * The model call (`auxiliary_client.call_llm`) and its logging are the seams;
 * everything the module decides about the text -- the char budget, the fenced
 * block, the "retrieval query:" label, the quote/backtick strip, the control
 * and whitespace squeeze, and the five rejection rules -- is the live rule.
 * The regexes are hand-matched: same alternatives, same word boundaries, same
 * case folding, ASCII (plus bytes >= 0x80 counting as word characters).
 */
#include "hermes_json.h"
#include "noc_w22_queryrewri_d659a2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QR_MAX_INPUT_CHARS 4000
#define QR_MAX_QUERY_CHARS 320
#define QR_HEAD_CHARS      3000
#define QR_TAIL_CHARS      900

/* ---------- small string helpers ---------- */

static char *qr_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int qr_is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' ||
           c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}

static unsigned char qr_lower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static int qr_ci_at(const char *s, size_t i, const char *lit) {
    size_t k;
    for (k = 0; lit[k]; k++) {
        if (!s[i + k] || qr_lower((unsigned char)s[i + k]) != (unsigned char)lit[k]) return 0;
    }
    return 1;
}

/* Python len() over a UTF-8 string. */
static size_t qr_cp_len(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    for (; *p; p++) if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Byte offset of character `cp` (`strlen` when cp is past the end). */
static size_t qr_cp_off(const char *s, size_t cp) {
    size_t seen = 0;
    const unsigned char *p = (const unsigned char *)s;
    if (cp == 0) return 0;
    for (; *p; p++) {
        if ((*p & 0xC0) != 0x80) {
            if (seen == cp) return (size_t)(p - (const unsigned char *)s);
            seen++;
        }
    }
    return strlen(s);
}

/* malloc'd [start_cp, end_cp) slice (end_cp < 0 means the end). */
static char *qr_slice_cp(const char *s, size_t start_cp, long end_cp) {
    size_t a = qr_cp_off(s, start_cp);
    size_t b = end_cp < 0 ? strlen(s) : qr_cp_off(s, (size_t)end_cp);
    size_t n;
    char *out;
    if (b < a) b = a;
    n = b - a;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s + a, n);
    out[n] = '\0';
    return out;
}

/* str.strip() with the module's whitespace set; malloc'd. */
static char *qr_strip(const char *s) {
    size_t a = 0, n;
    if (!s) s = "";
    n = strlen(s);
    while (n > 0 && qr_is_space((unsigned char)s[n - 1])) n--;
    while (n > a && qr_is_space((unsigned char)s[a])) { a++; n--; }
    {
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, s + a, n);
        out[n] = '\0';
        return out;
    }
}

/* str.rstrip(chars) for the quote/backtick set; malloc'd. */
static char *qr_strip_chars(const char *s, const char *chars) {
    size_t n = strlen(s), a = 0;
    for (;;) {
        if (n > a && s[n - 1] && strchr(chars, s[n - 1])) { n--; continue; }
        if (n > a && s[a] && strchr(chars, s[a])) { a++; continue; }
        break;
    }
    {
        char *out = (char *)malloc(n - a + 1);
        if (!out) return NULL;
        memcpy(out, s + a, n - a);
        out[n - a] = '\0';
        return out;
    }
}

static int qr_starts(const char *s, const char *lit) {
    return !strncmp(s, lit, strlen(lit));
}

static int qr_ends(const char *s, const char *lit) {
    size_t n = strlen(s), k = strlen(lit);
    return n >= k && !strcmp(s + n - k, lit);
}

/* ---------- _bounded_user_message ---------- */

char *w22_queryrewri_d659a2_bounded_user_message(const char *message) {
    char *text = qr_strip(message);
    char *head, *tail, *out;
    size_t cap, len;
    if (!text) return NULL;
    if (qr_cp_len(text) <= QR_MAX_INPUT_CHARS) return text;

    head = qr_slice_cp(text, 0, QR_HEAD_CHARS);
    if (!head) { free(text); return NULL; }
    {
        char *trimmed = qr_strip(head);       /* only the right side can move */
        size_t hn = strlen(trimmed);
        while (hn > 0 && qr_is_space((unsigned char)trimmed[hn - 1])) hn--;
        trimmed[hn] = '\0';
        free(head);
        head = trimmed;
    }
    {
        size_t total = qr_cp_len(text);
        tail = qr_slice_cp(text, total - QR_TAIL_CHARS, -1);
    }
    if (!tail) { free(head); free(text); return NULL; }
    {
        const char *t = tail;
        size_t skip = 0;
        while (*t && qr_is_space((unsigned char)*t)) { t++; skip++; }
        (void)skip;
        memmove(tail, t, strlen(t) + 1);      /* lstrip() */
    }
    cap = strlen(head) + strlen(tail) + 64;
    out = (char *)malloc(cap);
    if (!out) { free(head); free(tail); free(text); return NULL; }
    len = (size_t)snprintf(out, cap, "%s\n\n[... middle omitted ...]\n\n%s", head, tail);
    if (len + 1 > cap) { /* snprintf truncated: render into a sized buffer */
        char *big = (char *)malloc(len + 1);
        if (!big) { free(out); free(head); free(tail); free(text); return NULL; }
        snprintf(big, len + 1, "%s\n\n[... middle omitted ...]\n\n%s", head, tail);
        free(out);
        out = big;
    }
    free(head);
    free(tail);
    free(text);
    return out;
}

/* ---------- _extract_response_text ---------- */

char *w22_queryrewri_d659a2_extract_response_text(const json_t *response) {
    const json_t *choices, *first, *message, *content;
    if (!response || response->type != JSON_OBJECT) return qr_dup("");
    choices = json_obj_get(response, "choices");
    if (!choices || choices->type != JSON_ARRAY || choices->c.count == 0) return qr_dup("");
    first = choices->c.items[0];
    if (!first || first->type != JSON_OBJECT) return qr_dup("");
    message = json_obj_get(first, "message");
    if (!message || message->type != JSON_OBJECT) return qr_dup("");
    content = json_obj_get(message, "content");
    if (!content) return qr_dup("");
    if (content->type == JSON_STRING) return qr_dup(content->str_val);
    if (content->type != JSON_ARRAY) return qr_dup("");   /* None, numbers, dicts */

    {
        size_t i, total = 0;
        char *out;
        size_t pos = 0;
        for (i = 0; i < content->c.count; i++) {
            const json_t *part = content->c.items[i];
            const json_t *text;
            if (!part || part->type != JSON_OBJECT) continue;   /* getattr(part,'text',None) */
            text = json_obj_get(part, "text");
            if (text && text->type == JSON_STRING && text->str_val) total += strlen(text->str_val);
        }
        out = (char *)malloc(total + 1);
        if (!out) return NULL;
        for (i = 0; i < content->c.count; i++) {
            const json_t *part = content->c.items[i];
            const json_t *text;
            if (!part || part->type != JSON_OBJECT) continue;
            text = json_obj_get(part, "text");
            if (text && text->type == JSON_STRING && text->str_val) {
                size_t n = strlen(text->str_val);
                memcpy(out + pos, text->str_val, n);
                pos += n;
            }
        }
        out[pos] = '\0';
        return out;
    }
}

/* ---------- the regexes ---------- */

static int qr_is_word(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '_' || c >= 0x80;
}

static int qr_boundary(const char *s, size_t i, size_t n) {
    int before = (i > 0 && qr_is_word((unsigned char)s[i - 1]));
    int here = (i < n && qr_is_word((unsigned char)s[i]));
    return before != here;
}

/* One alternative matched at `i` (case-folded) and followed by \b. */
static int qr_word_at(const char *s, size_t i, size_t n, const char *alt) {
    size_t k = strlen(alt);
    if (i + k > n) return 0;
    if (!qr_ci_at(s, i, alt)) return 0;
    return qr_boundary(s, i + k, n);
}

/* `\b(?:alts)\b` anywhere in s. */
static int qr_search_words(const char *s, const char *const *alts, int count) {
    size_t n = strlen(s), i;
    int a;
    for (i = 0; i <= n; i++) {
        if (!qr_boundary(s, i, n)) continue;
        for (a = 0; a < count; a++) {
            if (qr_word_at(s, i, n, alts[a])) return 1;
        }
    }
    return 0;
}

/* `\binstructions?\b` -- "instruction" with an optional trailing "s". */
static int qr_search_instruction(const char *s) {
    static const char *const alts[] = {"instruction", "instructions"};
    return qr_search_words(s, alts, 2);
}

/* `\bsystem\s+prompt\b`, `\banswer\s+(?:directly|instead|the\s+user|this)\b`. */
static int qr_search_two_words(const char *s, const char *first, const char *const *seconds,
                               int third_split) {
    size_t n = strlen(s), i, k = strlen(first);
    int a;
    for (i = 0; i <= n; i++) {
        size_t j;
        if (!qr_boundary(s, i, n)) continue;
        if (i + k > n || !qr_ci_at(s, i, first)) continue;
        if (!qr_boundary(s, i + k, n)) continue;
        j = i + k;
        if (j >= n || !qr_is_space((unsigned char)s[j])) continue;
        while (j < n && qr_is_space((unsigned char)s[j])) j++;
        for (a = 0; a < 4; a++) {
            if (!seconds[a]) continue;
            if (third_split && a == 2) {
                /* "the\s+user": two words with whitespace between. */
                if (j + 3 > n || !qr_ci_at(s, j, "the")) continue;
                if (!qr_boundary(s, j + 3, n)) continue;
                {
                    size_t m = j + 3;
                    if (m >= n || !qr_is_space((unsigned char)s[m])) continue;
                    while (m < n && qr_is_space((unsigned char)s[m])) m++;
                    if (m + 4 > n || !qr_ci_at(s, m, "user")) continue;
                    if (qr_boundary(s, m + 4, n)) return 1;
                }
                continue;
            }
            if (qr_word_at(s, j, n, seconds[a])) return 1;
        }
    }
    return 0;
}

static int qr_search_system_prompt(const char *s) {
    static const char *const secs[] = {"prompt", NULL, NULL, NULL};
    return qr_search_two_words(s, "system", secs, 0);
}

static int qr_search_answer_leak(const char *s) {
    static const char *const secs[] = {"directly", "instead", "the user", "this"};
    return qr_search_two_words(s, "answer", secs, 1);
}

static int qr_instruction_leak(const char *s) {
    static const char *const verbs[] = {"ignore", "obey", "follow"};
    if (qr_search_words(s, verbs, 3)) return 1;
    if (qr_search_instruction(s)) return 1;
    if (qr_search_system_prompt(s)) return 1;
    return qr_search_answer_leak(s);
}

static int qr_question_start(const char *s) {
    static const char *const alts[] = {
        "what", "which", "who", "where", "when", "why", "how", "is", "are", "was", "were",
        "do", "does", "did", "has", "have", "had", "can", "could", "would", "should", "may",
        "might"
    };
    size_t n = strlen(s);
    int a;
    for (a = 0; a < (int)(sizeof(alts) / sizeof(alts[0])); a++) {
        if (qr_word_at(s, 0, n, alts[a])) return 1;
    }
    return 0;
}

static int qr_memory_grounding(const char *s) {
    static const char *const alts[] = {
        "user", "their", "they", "them", "previous", "prior", "past", "history", "preference",
        "preferences", "context", "known", "remembered", "earlier"
    };
    return qr_search_words(s, alts, (int)(sizeof(alts) / sizeof(alts[0])));
}

/* `[.!?]\s+\S` */
static int qr_internal_sentence(const char *s) {
    size_t n = strlen(s), i;
    for (i = 0; i < n; i++) {
        if (s[i] == '.' || s[i] == '!' || s[i] == '?') {
            size_t j = i + 1;
            if (j < n && qr_is_space((unsigned char)s[j])) {
                while (j < n && qr_is_space((unsigned char)s[j])) j++;
                if (j < n) return 1;
            }
        }
    }
    return 0;
}

/* `^(?:retrieval\s+query|memory\s+query|query|question)\s*:\s*` removed. */
static char *qr_drop_label(const char *s) {
    static const char *const pairs[][2] = {
        {"retrieval", "query"}, {"memory", "query"}, {NULL, NULL}, {"question", NULL}
    };
    size_t n = strlen(s), i = 0;
    int head_matched = 0;
    int p;
    if (qr_ci_at(s, 0, "query")) {
        i = 5;
        head_matched = 1;
    }
    for (p = 0; !head_matched && p < 4; p++) {
        const char *first = pairs[p][0];
        const char *second = pairs[p][1];
        size_t k;
        if (!first) continue;
        k = strlen(first);
        if (i + k > n || !qr_ci_at(s, i, first)) continue;
        if (!second) {
            i += k;
            head_matched = 1;
            break;
        }
        {
            size_t j = i + k;
            if (j >= n || !qr_is_space((unsigned char)s[j])) continue;
            while (j < n && qr_is_space((unsigned char)s[j])) j++;
            if (j + strlen(second) > n || !qr_ci_at(s, j, second)) continue;
            i = j + strlen(second);
            head_matched = 1;
        }
    }
    if (!head_matched) return qr_dup(s);
    while (i < n && qr_is_space((unsigned char)s[i])) i++;   /* \s* */
    if (i >= n || s[i] != ':') return qr_dup(s);             /* no label after all */
    i++;
    while (i < n && qr_is_space((unsigned char)s[i])) i++;   /* \s* */
    return qr_dup(s + i);
}

/* `[\x00-\x1f\x7f]+` -> " " */
static char *qr_drop_controls(const char *s) {
    size_t n = strlen(s), i, k = 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == 0x7f) {
            if (k > 0 && out[k - 1] != ' ') out[k++] = ' ';
            else if (k == 0) out[k++] = ' ';
            while (i + 1 < n && ((unsigned char)s[i + 1] < 0x20 || (unsigned char)s[i + 1] == 0x7f)) i++;
            continue;
        }
        out[k++] = s[i];
    }
    out[k] = '\0';
    return out;
}

/* `\s+` -> " " then strip(). */
static char *qr_squeeze(const char *s) {
    size_t n = strlen(s), i, k = 0;
    char *out = (char *)malloc(n + 1);
    char *stripped;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        if (qr_is_space((unsigned char)s[i])) {
            if (k > 0 && out[k - 1] != ' ') out[k++] = ' ';
            continue;
        }
        out[k++] = s[i];
    }
    out[k] = '\0';
    stripped = qr_strip(out);
    free(out);
    return stripped;
}

/* `^```(?:text)?\s*` removed (case-insensitive "text"). */
static char *qr_fence_head(const char *s) {
    size_t i = 3;
    if (qr_ci_at(s, i, "text")) i += 4;
    while (s[i] && qr_is_space((unsigned char)s[i])) i++;
    return qr_dup(s + i);
}

/* `\s*```$` removed. */
static char *qr_fence_tail(const char *s) {
    size_t n = strlen(s);
    if (n < 3 || strcmp(s + n - 3, "```")) return qr_dup(s);
    n -= 3;
    while (n > 0 && qr_is_space((unsigned char)s[n - 1])) n--;
    {
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, s, n);
        out[n] = '\0';
        return out;
    }
}

char *w22_queryrewri_d659a2_normalize_rewrite(const char *text) {
    char *cand, *tmp;

    cand = qr_strip(text ? text : "");
    if (!cand) return NULL;

    if (qr_starts(cand, "```") && qr_ends(cand, "```")) {
        tmp = qr_fence_head(cand);
        free(cand);
        if (!tmp) return NULL;
        cand = qr_fence_tail(tmp);
        free(tmp);
        if (!cand) return NULL;
    }

    tmp = qr_strip(cand);
    free(cand);
    if (!tmp) return NULL;
    cand = qr_drop_label(tmp);
    free(tmp);
    if (!cand) return NULL;

    tmp = qr_strip(cand);
    free(cand);
    if (!tmp) return NULL;
    cand = qr_strip_chars(tmp, "\"'`");
    free(tmp);
    if (!cand) return NULL;
    tmp = qr_strip(cand);
    free(cand);
    if (!tmp) return NULL;
    cand = tmp;

    tmp = qr_drop_controls(cand);
    free(cand);
    if (!tmp) return NULL;
    cand = qr_squeeze(tmp);
    free(tmp);
    if (!cand) return NULL;

    /* Reject: empty/too long, not a question, not grounded in user memory,
     * instruction leakage, or more than one sentence. */
    if (!cand[0] || qr_cp_len(cand) > QR_MAX_QUERY_CHARS) { free(cand); return qr_dup(""); }
    if (!qr_question_start(cand) || !qr_memory_grounding(cand) || qr_instruction_leak(cand)) {
        free(cand);
        return qr_dup("");
    }
    {
        char *body = qr_dup(cand);
        size_t n = body ? strlen(body) : 0;
        while (n > 0 && body[n - 1] == '?') n--;
        body[n] = '\0';
        if (qr_internal_sentence(body)) {
            free(body);
            free(cand);
            return qr_dup("");
        }
        free(body);
    }
    if (!qr_ends(cand, "?")) {
        size_t n = strlen(cand);
        char *out = (char *)realloc(cand, n + 2);
        if (!out) { free(cand); return NULL; }
        out[n] = '?';
        out[n + 1] = '\0';
        cand = out;
    }
    return cand;
}

char *w22_queryrewri_d659a2_rewrite(const char *user_message, const json_t *response,
                                    int llm_raises) {
    char *bounded, *text, *out;
    bounded = w22_queryrewri_d659a2_bounded_user_message(user_message);
    if (!bounded) return NULL;
    if (!bounded[0]) { free(bounded); return qr_dup(""); }
    if (llm_raises) { free(bounded); return qr_dup(""); }
    free(bounded);
    text = w22_queryrewri_d659a2_extract_response_text(response);
    if (!text) return NULL;
    out = w22_queryrewri_d659a2_normalize_rewrite(text);
    free(text);
    return out;
}
