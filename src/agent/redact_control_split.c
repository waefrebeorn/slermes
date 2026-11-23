/*
 * redact_control_split.c — C port of upstream Hermes agent/redact_control_split.py.
 *
 * PoP: w13_redactcont_3747d1_strip_shadow @ agent/redact_control_split.py:_strip_shadow
 * PoP: w13_redactcont_3747d1_union @ agent/redact_control_split.py:_union
 * PoP: w13_redactcont_3747d1_mask_control_split @ agent/redact_control_split.py:mask_control_split_tokens
 * PoP: w13_redactcont_3747d1_mask_prefix @ agent/redact_control_split.py:mask_prefix_matches
 *
 * Pattern seam: prefix_re is the documented family built by the oracle as
 * re.compile("(" + re.escape(prefix) + "[A-Za-z0-9_+\\-./]+)"), with sub_re=None
 * and skip=None. Fixtures restrict text to ASCII control chars (0x00-0x1f,
 * 0x7f) so Python's codepoint indices equal C's byte indices; U+0080+ split
 * chars and \x9b are not exercised.
 */
#include "noc_w13_redactcont_3747d1.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPAN_CAP 1024

struct span { int start, end; int flag; };
struct spans { struct span *v; size_t n, cap; };
struct shadow { char *text; unsigned char *noise; int *orig; size_t n; };

static int is_control(unsigned char c) { return c <= 0x1f || c == 0x7f; }

/* _CSI_SEQ_RE.match(text, i) at an ESC introducer. */
static int csi_end(const char *t, int i) {
    int n = (int)strlen(t);
    unsigned char c = (unsigned char)t[i];
    if (c != 0x1b && c != 0x9b) return 0;
    int j = i;
    if ((c == 0x1b && j + 1 < n && t[j+1] == '[') || c == 0x9b) {
        j += (c == 0x1b) ? 2 : 1;
        while (j < n && (unsigned char)t[j] >= 0x30 && (unsigned char)t[j] <= 0x3f) j++;
        while (j < n && (unsigned char)t[j] >= 0x20 && (unsigned char)t[j] <= 0x2f) j++;
        if (j < n && (unsigned char)t[j] >= 0x40 && (unsigned char)t[j] <= 0x7e) return j + 1;
    }
    if (c == 0x1b) {
        int k = i + 1, p = k;
        while (p < n && (unsigned char)t[p] >= 0x20 && (unsigned char)t[p] <= 0x2f) p++;
        if (p > k && p < n && (unsigned char)t[p] >= 0x30 && (unsigned char)t[p] <= 0x7e)
            return p + 1;
        if (k < n && (t[k] == 'N' || t[k] == 'O') && k + 1 < n &&
            !is_control((unsigned char)t[k+1])) return k + 2;
        if (k < n && (t[k]=='7'||t[k]=='8'||t[k]=='='||t[k]=='>'||t[k]=='M')) return k + 1;
    }
    return 0;
}

static struct shadow strip_shadow(const char *text) {
    struct shadow s;
    memset(&s, 0, sizeof(s));
    int n = (int)strlen(text);
    s.text = (char *)malloc((size_t)n + 1);
    s.noise = (unsigned char *)calloc((size_t)n + 1, 1);
    s.orig = (int *)malloc(((size_t)n + 1) * sizeof(int));
    int i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)text[i];
        int ce = (c == 0x1b || c == 0x9b) ? csi_end(text, i) : 0;
        if (ce) {
            for (int k = i; k < ce; k++) s.noise[k] = 1;
            i = ce;
            continue;
        }
        if (is_control(c)) s.noise[i] = 1;
        else { s.text[s.n] = text[i]; s.orig[s.n] = i; s.n++; }
        i++;
    }
    s.text[s.n] = '\0';
    return s;
}

static void span_add(struct spans *sp, int start, int end, int flag) {
    if (sp->n == sp->cap) {
        sp->cap = sp->cap ? sp->cap * 2 : SPAN_CAP;
        sp->v = (struct span *)realloc(sp->v, sp->cap * sizeof(struct span));
    }
    sp->v[sp->n].start = start;
    sp->v[sp->n].end = end;
    sp->v[sp->n].flag = flag;
    sp->n++;
}

static int span_cmp(struct span a, struct span b) {
    int c = (a.start > b.start) - (a.start < b.start);
    if (c == 0) c = (a.end > b.end) - (a.end < b.end);
    if (c == 0) c = (a.flag > b.flag) - (a.flag < b.flag);
    return c;
}

/* _union: sorted (start,end,flag), overlaps merged, merged flag = all(parts). */
static struct spans union_spans(const struct spans *in) {
    struct spans out = {0};
    if (!in->n) return out;
    struct span *a = (struct span *)malloc(in->n * sizeof(struct span));
    memcpy(a, in->v, in->n * sizeof(struct span));
    for (size_t i = 1; i < in->n; i++) {
        struct span cur = a[i];
        size_t j = i;
        while (j > 0 && span_cmp(cur, a[j-1]) < 0) { a[j] = a[j-1]; j--; }
        a[j] = cur;
    }
    for (size_t i = 0; i < in->n; i++) {
        if (out.n && a[i].start < out.v[out.n-1].end) {
            out.v[out.n-1].end = a[i].end > out.v[out.n-1].end ? a[i].end : out.v[out.n-1].end;
            out.v[out.n-1].flag = out.v[out.n-1].flag && a[i].flag;
        } else {
            span_add(&out, a[i].start, a[i].end, a[i].flag);
        }
    }
    free(a);
    return out;
}

/* ---------- documented pattern family ---------- */
static int is_body_char(unsigned char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
           c=='_'||c=='+'||c=='-'||c=='.'||c=='/';
}

static int match_from(const char *text, const char *prefix, int pos, int *s, int *e) {
    int n = (int)strlen(text);
    int pl = (int)strlen(prefix);
    if (pl <= 0) return 0;
    for (int i = pos; i <= n - pl; i++) {
        if (memcmp(text + i, prefix, (size_t)pl) != 0) continue;
        int j = i + pl;
        while (j < n && is_body_char((unsigned char)text[j])) j++;
        if (j > i + pl) { if (s) *s = i; if (e) *e = j; return 1; }
    }
    return 0;
}

static struct spans iter_with_swallowed(const char *text, const char *prefix) {
    struct spans base = {0}, all = {0};
    int n = (int)strlen(text);
    int pos = 0, s = 0, e = 0;
    while (pos <= n && match_from(text, prefix, pos, &s, &e)) {
        span_add(&base, s, e, 0);
        pos = e;
    }
    for (size_t i = 0; i < base.n; i++) {
        int ms = base.v[i].start, me = base.v[i].end;
        span_add(&all, ms, me, 0);
        int p = ms + 1, is = 0, ie = 0;
        while (p <= n && match_from(text, prefix, p, &is, &ie) && is < me) {
            if (ie > me) { span_add(&all, is, ie, 0); break; }
            p = ie;
        }
    }
    free(base.v);
    return all;
}

static int any_noise(const unsigned char *noise, int start, int end) {
    for (int k = start; k < end; k++) if (noise[k]) return 1;
    return 0;
}

static int bisect_left(const int *a, size_t n, int x) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1; else hi = mid;
    }
    return (int)lo;
}

static int fullmatch(const char *shadow, int start, int end, const char *prefix) {
    int s = 0, e = 0;
    if (!match_from(shadow, prefix, start, &s, &e)) return 0;
    return s == start && e == end;
}

static struct spans candidate_spans(const char *text, const struct shadow *sh,
                                    const char *prefix) {
    struct spans out = {0};
    struct spans ms = iter_with_swallowed(sh->text, prefix);
    int ntext = (int)strlen(text);
    for (size_t i = 0; i < ms.n; i++) {
        int bs = ms.v[i].start, be = ms.v[i].end;
        if (bs < 0 || be <= bs || (size_t)be > sh->n) continue;
        int start = sh->orig[bs];
        int end = sh->orig[be - 1] + 1;
        int first_nl = -1;
        for (int k = start; k < end; k++) {
            if (text[k] == '\n' || text[k] == '\r') {
                if (first_nl < 0 || k - start < first_nl) first_nl = k - start;
            }
        }
        if (first_nl >= 0) {
            char *span = (char *)malloc((size_t)(end - start + 1));
            memcpy(span, text + start, (size_t)(end - start));
            span[end - start] = '\0';
            int ss = 0, se = 0;
            int found = match_from(span, prefix, 0, &ss, &se);
            free(span);
            if (found) {
                int le = bisect_left(sh->orig, sh->n, start + first_nl);
                end = (le > 0) ? sh->orig[le - 1] + 1 : start;
                int glued_left = start > 0 && sh->noise[start - 1];
                if (!(glued_left || any_noise(sh->noise, start, end)) ||
                    !fullmatch(sh->text, bs, le, prefix))
                    continue;
            }
        }
        int rend = end;
        while (rend > start && text[rend - 1] == '=') rend--;
        int ok = 1;
        for (int k = start; k < rend; k++) {
            if (sh->noise[k]) continue;
            if (!is_body_char((unsigned char)text[k])) { ok = 0; break; }
        }
        if (ok && (end >= ntext || text[end] != '=')) span_add(&out, start, end, 0);
    }
    free(ms.v);
    return out;
}

static json_t *assemble(const char *text, struct spans sp, const char *mask, int use_flag) {
    struct spans merged = union_spans(&sp);
    size_t cap = strlen(text) + merged.n * (strlen(mask) + 4) + 16;
    char *out = (char *)malloc(cap);
    if (!out) { free(merged.v); return json_string(""); }
    size_t pos = 0, o = 0;
    int ntext = (int)strlen(text);
    for (size_t i = 0; i < merged.n; i++) {
        int s = merged.v[i].start, e = merged.v[i].end;
        if (use_flag && merged.v[i].flag) continue;
        if (s < 0 || e < s || e > ntext || s < (int)pos) continue;
        memcpy(out + o, text + pos, (size_t)s - pos); o += (size_t)s - pos;
        memcpy(out + o, mask, strlen(mask)); o += strlen(mask);
        pos = (size_t)e;
    }
    memcpy(out + o, text + pos, (size_t)ntext - pos); o += (size_t)ntext - pos;
    out[o] = '\0';
    json_t *r = json_string(out);
    free(out); free(merged.v);
    return r;
}

/* ---------- public API ---------- */
struct json_t *w13_redactcont_3747d1_strip_shadow(const char *text) {
    struct shadow s = strip_shadow(text ? text : "");
    json_t *o = json_object();
    json_set(o, "shadow", json_string(s.text));
    json_t *noise = json_array(), *orig = json_array();
    size_t text_len = strlen(text ? text : "");
    for (size_t i = 0; i < text_len; i++)
        json_append(noise, json_number(s.noise[i] ? 1.0 : 0.0));
    for (size_t i = 0; i < s.n; i++)
        json_append(orig, json_number((double)s.orig[i]));
    json_set(o, "noise", noise);
    json_set(o, "orig_idx", orig);
    free(s.text); free(s.noise); free(s.orig);
    return o;
}

struct json_t *w13_redactcont_3747d1_union(const json_t *spans) {
    struct spans in = {0};
    if (json_is_array(spans)) {
        for (size_t i = 0; i < json_len(spans); i++) {
            json_t *e = json_get(spans, i);
            if (!json_is_array(e) || json_len(e) < 3) continue;
            json_t *a0 = json_get(e, 0), *a1 = json_get(e, 1), *a2 = json_get(e, 2);
            int st = (int)(a0 ? a0->num_val : 0);
            int en = (int)(a1 ? a1->num_val : 0);
            int fl = (a2 && a2->type == JSON_BOOL && a2->bool_val) ? 1 : 0;
            span_add(&in, st, en, fl);
        }
    }
    struct spans merged = union_spans(&in);
    json_t *out = json_array();
    for (size_t i = 0; i < merged.n; i++) {
        json_t *a = json_array();
        json_append(a, json_number((double)merged.v[i].start));
        json_append(a, json_number((double)merged.v[i].end));
        json_append(a, json_bool(merged.v[i].flag ? true : false));
        json_append(out, a);
    }
    free(in.v); free(merged.v);
    return out;
}

struct json_t *w13_redactcont_3747d1_mask_control_split(const char *text,
                                                        const char *prefix, const char *mask) {
    if (!text) text = "";
    if (!prefix) prefix = "";
    if (!mask) mask = "";
    int has_ctrl = 0;
    for (const char *p = text; *p; p++)
        if (is_control((unsigned char)*p) || (unsigned char)*p == 0x9b) { has_ctrl = 1; break; }
    if (!has_ctrl) return json_string(text);
    struct shadow sh = strip_shadow(text);
    if (sh.n == strlen(text)) {
        free(sh.text); free(sh.noise); free(sh.orig);
        return json_string(text);
    }
    struct spans sp = candidate_spans(text, &sh, prefix);
    json_t *r = assemble(text, sp, mask, 1);
    free(sp.v); free(sh.text); free(sh.noise); free(sh.orig);
    return r;
}

struct json_t *w13_redactcont_3747d1_mask_prefix(const char *text,
                                                 const char *prefix, const char *mask) {
    if (!text) text = "";
    if (!prefix) prefix = "";
    if (!mask) mask = "";
    struct spans ms = iter_with_swallowed(text, prefix);
    for (size_t i = 0; i < ms.n; i++) ms.v[i].flag = 1;
    json_t *r = assemble(text, ms, mask, 0);
    free(ms.v);
    return r;
}
