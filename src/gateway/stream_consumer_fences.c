/*
 * stream_consumer_fences.c — real C11 port of upstream Hermes
 *   gateway/stream_consumer_fences.py (code-fence display helpers).
 * PoP: w18_streamcons_75ce67_escape_code_fences_for_display @ gateway/stream_consumer_fences.py:escape_code_fences_for_display
 * PoP: w18_streamcons_75ce67_open_code_fence_span @ gateway/stream_consumer_fences.py:open_code_fence_span
 * PoP: w18_streamcons_75ce67_ensure_closed_code_fences @ gateway/stream_consumer_fences.py:ensure_closed_code_fences
 */
#include "hermes_json.h"
#include "noc_w18_streamcons_75ce67.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Number of non-overlapping "```" occurrences in [lo,hi) (Python str.count). */
static size_t count_triples(const char *s, size_t lo, size_t hi) {
    size_t c = 0, i = lo;
    while (i + 2 < hi) {
        if (s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') { c++; i += 3; }
        else i++;
    }
    return c;
}

/* Last "```" start in [lo,hi) with its end <= hi, or -1 (Python str.rfind). */
static long long rfind_triple(const char *s, size_t lo, size_t hi) {
    long long found = -1;
    for (size_t i = lo; i + 2 < hi; i++) {
        if (s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') found = (long long)i;
    }
    return found;
}

static size_t count_backticks(const char *s) {
    size_t c = 0;
    for (const char *p = s; *p; p++) if (*p == '`') c++;
    return c;
}

static void rstrip_newlines(char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '\n') s[--n] = '\0';
}

/* re.sub(r"```.*?```", "", s, DOTALL). */
static char *strip_complete_fences(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return dupstr(s ? s : "");
    size_t o = 0, i = 0;
    while (i < n) {
        if (i + 2 < n && s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') {
            size_t q = i + 3;
            while (q + 2 < n && !(s[q] == '`' && s[q + 1] == '`' && s[q + 2] == '`')) q++;
            if (q + 2 < n) { i = q + 3; continue; }  /* matched region -> drop */
            memcpy(out + o, s + i, n - i); o += n - i;  /* no closing -> literal rest */
            break;
        }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}

/* re.sub(r"```[^`]*$", "", s) in place. */
static void strip_trailing_open_fence(char *s) {
    size_t n = strlen(s);
    long long last = -1;
    for (size_t i = 0; i + 2 < n; i++) {
        if (s[i] == '`' && s[i + 1] == '`' && s[i + 2] == '`') last = (long long)i;
    }
    if (last < 0) return;
    for (size_t k = (size_t)last + 3; k < n; k++) {
        if (s[k] == '`') return;  /* a later backtick: the label pattern cannot reach $ */
    }
    s[last] = '\0';  /* write NUL at new length after shortening */
}

char *w18_streamcons_75ce67_escape_code_fences_for_display(const char *text) {
    if (!text) return NULL;  /* non-str input passes through */
    size_t n = strlen(text);
    size_t triples = count_triples(text, 0, n);
    char *out = (char *)malloc(n + 3 * triples + 1);
    if (!out) return dupstr(text);
    size_t o = 0;
    for (size_t i = 0; i < n; ) {
        if (i + 2 < n && text[i] == '`' && text[i + 1] == '`' && text[i + 2] == '`') {
            memcpy(out + o, "\\`\\`\\`", 6); o += 6; i += 3;
        } else {
            out[o++] = text[i++];
        }
    }
    out[o] = '\0';
    return out;
}

struct json_t *w18_streamcons_75ce67_open_code_fence_span(const char *text, long long upto) {
    if (!text) return json_null();
    size_t n = strlen(text);
    size_t h = (upto <= 0) ? 0 : (size_t)upto;
    if (h > n) h = n;
    if (count_triples(text, 0, h) % 2 == 0) return json_null();
    long long start = rfind_triple(text, 0, h);
    size_t end = n;
    for (size_t i = (size_t)start; i < n; i++) {
        if (text[i] == '\n') { end = i; break; }
    }
    json_t *arr = json_array();
    json_append(arr, json_number((double)start));
    json_append(arr, json_number((double)end));
    return arr;
}

char *w18_streamcons_75ce67_ensure_closed_code_fences(const char *text) {
    if (!text || !*text) return dupstr(text);  /* empty / non-str passes through */
    char *work = dupstr(text);
    if (!work) return NULL;
    if (count_triples(work, 0, strlen(work)) % 2 == 1) {
        rstrip_newlines(work);
        size_t l = strlen(work);
        char *tmp = (char *)malloc(l + 5);
        if (tmp) {
            memcpy(tmp, work, l);
            tmp[l] = '\n'; tmp[l + 1] = '`'; tmp[l + 2] = '`'; tmp[l + 3] = '`'; tmp[l + 4] = '\0';
            free(work); work = tmp;
        }
    }
    char *wo = strip_complete_fences(work);
    strip_trailing_open_fence(wo);  /* in place */
    int odd = (int)(count_backticks(wo) % 2);
    free(wo);
    if (odd) {
        size_t l = strlen(work);
        char *tmp = (char *)malloc(l + 2);
        if (tmp) {
            memcpy(tmp, work, l);
            tmp[l] = '`'; tmp[l + 1] = '\0';
            free(work); return tmp;
        }
    }
    return work;
}
