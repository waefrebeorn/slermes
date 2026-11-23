/*
 * approval_detection.c — real C11 port of pure helpers from
 * tools/approval_detection.py.
 *
 * PoP: appdetect_lower_preserving_flags @ tools/approval_detection.py:_lower_preserving_flags
 * PoP: appdetect_splice                 @ tools/approval_detection.py:_splice
 * PoP: appdetect_eval_payload           @ tools/approval_detection.py:_eval_payload
 * PoP: appdetect_interpreter_family     @ tools/approval_detection.py:_interpreter_family
 *
 * The upstream module is a 1900-line detection table driven by quote-aware
 * scanners and regex. These four helpers are the genuinely pure, deterministic
 * leaf functions of that table (flag-case projection, splice application,
 * eval argument joining, interpreter-name classification) and are ported
 * exactly; the pattern/variant machinery is out of scope for a C harness.
 */
#include "noc_appdetect.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *ad_dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return (char *)calloc(1, 1);
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *ad_dup(const char *s) { return ad_dup_n(s ? s : "", s ? strlen(s) : 0); }

/*
 * PoP: appdetect_lower_preserving_flags @ tools/approval_detection.py:_lower_preserving_flags
 */
char *appdetect_lower_preserving_flags(const char *command) {
    const char *p = command ? command : "";
    size_t n = strlen(p), i = 0, o = 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return ad_dup("");
    while (i < n) {
        size_t a = i;
        int ws = isspace((unsigned char)p[i]) ? 1 : 0;
        while (i < n && (isspace((unsigned char)p[i]) ? 1 : 0) == ws) i++;
        /* A dash-leading token is preserved verbatim; all others are lowered. */
        if (!ws && p[a] == '-') {
            for (size_t k = a; k < i; k++) out[o++] = p[k];
        } else {
            for (size_t k = a; k < i; k++) out[o++] = (char)tolower((unsigned char)p[k]);
        }
    }
    out[o] = '\0';
    return out;
}

typedef struct { long long s, e; const char *text; } AdEdit;

/* _splice assumes sorted, non-overlapping edits; sort defensively by start. */
static int ad_cmp_edit(const void *a, const void *b) {
    const AdEdit *x = (const AdEdit *)a, *y = (const AdEdit *)b;
    if (x->s < y->s) return -1;
    if (x->s > y->s) return 1;
    return 0;
}

/*
 * PoP: appdetect_splice @ tools/approval_detection.py:_splice
 */
char *appdetect_splice(const char *command, json_t *edits) {
    const char *src = command ? command : "";
    size_t n = strlen(src), count = 0, i, cap, o = 0;
    long long prev = 0;
    AdEdit *es = NULL;
    char *out;
    if (edits && edits->type == JSON_ARRAY) count = json_len(edits);
    if (count) es = (AdEdit *)malloc(count * sizeof(AdEdit));
    for (i = 0; i < count; i++) {
        json_t *e = json_get(edits, i);
        json_t *s = e && e->type == JSON_ARRAY ? json_get(e, 0) : NULL;
        json_t *ee = e && e->type == JSON_ARRAY ? json_get(e, 1) : NULL;
        json_t *tx = e && e->type == JSON_ARRAY ? json_get(e, 2) : NULL;
        es[i].s = s && s->type == JSON_NUMBER ? (long long)s->num_val : 0;
        es[i].e = ee && ee->type == JSON_NUMBER ? (long long)ee->num_val : es[i].s;
        es[i].text = tx && tx->type == JSON_STRING ? tx->str_val : "";
    }
    if (count > 1) qsort(es, count, sizeof(AdEdit), ad_cmp_edit);
    /* Upper bound: source + all replacement text. */
    cap = n + 16;
    for (i = 0; i < count; i++) cap += strlen(es[i].text) + 4;
    out = (char *)malloc(cap + 1);
    if (!out) { free(es); return ad_dup(""); }
    for (i = 0; i < count; i++) {
        long long st = es[i].s < 0 ? 0 : es[i].s;
        long long en = es[i].e < st ? st : es[i].e;
        if (en > (long long)n) en = (long long)n;
        if (st < prev) st = prev;
        if (st > (long long)n) st = (long long)n;
        for (long long k = prev; k < st; k++) out[o++] = src[k];
        size_t tl = strlen(es[i].text);
        if (tl) { memcpy(out + o, es[i].text, tl); o += tl; }
        prev = en > prev ? en : prev;
    }
    for (long long k = prev; k < (long long)n; k++) out[o++] = src[k];
    out[o] = '\0';
    free(es);
    return out;
}

/*
 * PoP: appdetect_eval_payload @ tools/approval_detection.py:_eval_payload
 */
char *appdetect_eval_payload(json_t *args) {
    size_t count = 0, i, start = 0, total = 0, o = 0;
    char *out;
    if (args && args->type == JSON_ARRAY) count = json_len(args);
    if (count && json_get(args, 0) && json_get(args, 0)->type == JSON_STRING &&
        !strcmp(json_get(args, 0)->str_val, "--"))
        start = 1;
    for (i = start; i < count; i++) {
        json_t *v = json_get(args, i);
        const char *s = v && v->type == JSON_STRING ? v->str_val : "";
        total += strlen(s);
    }
    if (count <= start || total == 0) return NULL;   /* "" or None */
    out = (char *)malloc(total + (count - start) + 1);
    if (!out) return NULL;
    for (i = start; i < count; i++) {
        json_t *v = json_get(args, i);
        const char *s = v && v->type == JSON_STRING ? v->str_val : "";
        size_t l = strlen(s);
        if (i > start) out[o++] = ' ';
        if (l) { memcpy(out + o, s, l); o += l; }
    }
    out[o] = '\0';
    return out;
}

static int ad_all_digits(const char *s, size_t from) {
    if (!s[from]) return 0;
    for (size_t i = from; s[i]; i++) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}
static int ad_digit_runs(const char *s, size_t from) {
    /* match (\.\d+)* after `from` */
    size_t i = from;
    while (s[i] == '.' && isdigit((unsigned char)s[i + 1])) {
        i += 2;
        while (isdigit((unsigned char)s[i])) i++;
    }
    return s[i] == '\0';
}

/*
 * PoP: appdetect_interpreter_family @ tools/approval_detection.py:_interpreter_family
 */
const char *appdetect_interpreter_family(const char *executable) {
    const char *p = executable ? executable : "";
    const char *base = p;
    char name[128];
    size_t n;
    for (const char *q = p; *q; q++)
        if (*q == '/' || *q == '\\') base = q + 1;
    n = strlen(base);
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    for (size_t i = 0; i < n; i++) name[i] = (char)tolower((unsigned char)base[i]);
    name[n] = '\0';
    if (n > 4 && !strcmp(name + n - 4, ".exe")) name[n - 4] = '\0';
    n = strlen(name);

    /* python: py | python[23]?(\.\d+)* */
    if (!strcmp(name, "py")) return "python";
    if (n >= 6 && !strncmp(name, "python", 6)) {
        size_t i = 6;
        if (name[i] == '2' || name[i] == '3') i++;
        if (ad_digit_runs(name, i)) return "python";
    }
    if (!strcmp(name, "node") || !strcmp(name, "nodejs")) return "node";
    if (n >= 4 && !strncmp(name, "perl", 4)) {
        size_t i = 4;
        while (isdigit((unsigned char)name[i])) i++;
        if (ad_digit_runs(name, i)) return "perl";
    }
    if (n >= 4 && !strncmp(name, "ruby", 4) && ad_digit_runs(name, 4)) {
        /* ruby[0-9.]*: also accepts bare "ruby" (already covered by runs). */
        return "ruby";
    }
    if (!strcmp(name, "php")) return "php";
    if (!strcmp(name, "powershell") || !strcmp(name, "pwsh")) return "powershell";
    if (!strcmp(name, "bun")) return "bun";
    if (!strcmp(name, "deno")) return "deno";
    (void)ad_all_digits;
    return NULL;
}
