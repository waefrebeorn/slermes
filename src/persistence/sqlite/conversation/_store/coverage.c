/*
 * coverage.c - C11 port of upstream
 * persistence/sqlite/conversation/_store/coverage.py (gate w34_coverage_56490e).
 *
 * PoP: w34_coverage_56490e_text_of @ persistence/sqlite/conversation/_store/coverage.py:_text_of
 * PoP: w34_coverage_56490e_holds_as_block @ persistence/sqlite/conversation/_store/coverage.py:_holds_as_block
 * PoP: w34_coverage_56490e_images_replaced @ persistence/sqlite/conversation/_store/coverage.py:_images_replaced
 * PoP: w34_coverage_56490e_carried_tail_slots @ persistence/sqlite/conversation/_store/coverage.py:_carried_tail_slots
 * PoP: w34_coverage_56490e_uncounted_merged_rows @ persistence/sqlite/conversation/_store/coverage.py:_uncounted_merged_rows
 * PoP: w34_coverage_56490e_tail_originals @ persistence/sqlite/conversation/_store/coverage.py:_tail_originals
 *
 * Seams (left in the caller, never touched here): the state.db connection and
 * every statement it runs (_carried_tail_ids, _uids_active_outside,
 * _resolve_carried_row_ids, _matching_active_ids, _matching_retired_ids,
 * _merged_user_run, _proved_coverage), the live StoreHolder, and the writer's
 * own serializer plus the stored-row digest the claims compare through
 * (_index_held_dicts, _claim_stored_copy, _rewritten_carry, _replays_as,
 * _copy_of_current_row, _rewrite_explains).  What is ported is the part that is
 * a pure function of the values: the text a message carries, whole-line
 * containment, the image-placeholder rewrite predicate, and the three span
 * computations the commit's positional rewind is sized by.
 *
 * Value caveats, both forced by the JSON seam: a number is a double, so
 * Python's int/float split is "integral value" here (`3` and `3.0` both read as
 * int; `_row_id: 1.0` therefore cannot be told from `_row_id: 1`), and
 * str() of a container falls back to the JSON spelling instead of Python's
 * repr.  Scalar str() (str, int, float, bool, None) is exact.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_coverage_56490e.h"

/* _IMAGE_PART_TYPES */
static const char *const COV_IMAGE_PART_TYPES[] = {"image_url", "input_image", "image"};
#define COV_IMAGE_PART_TYPES_N 3

/* _MERGED_DURABLE_ROWS / _UNNAMED_DURABLE_ROWS / _ABSORBED_ROW_IDS (session_rules.turns) */
#define COV_MERGED_DURABLE_ROWS "_merged_durable_rows"
#define COV_UNNAMED_DURABLE_ROWS "_unnamed_durable_rows"
#define COV_ABSORBED_ROW_IDS "_absorbed_row_ids"
#define COV_ROW_ID "_row_id"

/* ---------------------------------------------------------------- helpers */

static char *cov_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python truthiness. */
static int cov_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* Python float repr: shortest round-trip decimal, exponent form outside
 * 1e-4 <= |v| < 1e16, "-0.0" for negative zero.  No libm. */
static char *cov_number_repr(double v) {
    char buf[64];
    long long ll;
    int p;
    if (v != v) return cov_dup("nan");
    if (v > 1.7976931348623157e308) return cov_dup("inf");
    if (v < -1.7976931348623157e308) return cov_dup("-inf");
    if (v == 0.0) return cov_dup((1.0 / v) < 0.0 ? "-0.0" : "0.0");
    if (v > -1e16 && v < 1e16) {
        ll = (long long)v;
        if ((double)ll == v) {
            snprintf(buf, sizeof buf, "%lld", ll);
            return cov_dup(buf);
        }
    }
    for (p = 1; p <= 17; p++) {
        snprintf(buf, sizeof buf, "%.*g", p, v);
        if (strtod(buf, NULL) == v) break;
    }
    return cov_dup(buf);
}

/* Python str(value); falsey values are the caller's business. */
static char *cov_py_str(const json_t *v) {
    char *s, *out;
    if (!v) return cov_dup("");
    switch (v->type) {
    case JSON_STRING: return cov_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return cov_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: return cov_number_repr(v->num_val);
    case JSON_NULL:   return cov_dup("None");
    default:
        s = json_serialize(v);
        out = cov_dup(s);
        free(s);
        return out;
    }
}

/* Python `value or ""` then str(): a falsey text carries nothing. */
static char *cov_text_value(const json_t *v) {
    if (!cov_truthy(v)) return cov_dup("");
    return cov_py_str(v);
}

typedef struct {
    char *buf;
    size_t len, cap;
    int oom;
} cov_sb;

static void cov_sb_add(cov_sb *sb, const char *s, size_t n) {
    if (sb->oom) return;
    if (sb->len + n + 1 > sb->cap) {
        size_t cap = sb->cap ? sb->cap : 64;
        char *grown;
        while (sb->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(sb->buf, cap);
        if (!grown) { sb->oom = 1; return; }
        sb->buf = grown;
        sb->cap = cap;
    }
    if (n && s) memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

static void cov_sb_cstr(cov_sb *sb, const char *s) {
    cov_sb_add(sb, s, s ? strlen(s) : 0);
}

/* UTF-8 decode of one codepoint; returns the bytes consumed (>= 1). */
static size_t cov_utf8_next(const unsigned char *s, size_t n, unsigned long *cp) {
    unsigned char b;
    size_t need, i;
    unsigned long v;
    if (n == 0) { *cp = 0; return 0; }
    b = s[0];
    if (b < 0x80) { *cp = b; return 1; }
    if (b >= 0xC2 && b <= 0xDF) { need = 2; v = b & 0x1F; }
    else if (b >= 0xE0 && b <= 0xEF) { need = 3; v = b & 0x0F; }
    else if (b >= 0xF0 && b <= 0xF4) { need = 4; v = b & 0x07; }
    else { *cp = b; return 1; }
    if (need > n) { *cp = b; return 1; }
    for (i = 1; i < need; i++) {
        if ((s[i] & 0xC0) != 0x80) { *cp = b; return 1; }
        v = (v << 6) | (unsigned long)(s[i] & 0x3F);
    }
    *cp = v;
    return need;
}

/* str.isspace(): the set str.strip() removes. */
static int cov_is_space(unsigned long cp) {
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\v' || cp == '\f') return 1;
    if (cp >= 0x1C && cp <= 0x1F) return 1;          /* file/group/record/unit separators */
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

/* s[off:end].strip() == "" */
static int cov_blank(const char *s, size_t off, size_t end) {
    const unsigned char *u = (const unsigned char *)s;
    size_t i = off;
    unsigned long cp;
    while (i < end) {
        size_t step = cov_utf8_next(u + i, end - i, &cp);
        if (!step) break;
        if (!cov_is_space(cp)) return 0;
        i += step;
    }
    return 1;
}

/* Python str.find(needle, from): byte offset or -1. */
static long long cov_find(const char *hay, size_t hn, const char *needle, size_t nn, size_t from) {
    size_t i;
    if (nn == 0) return from <= hn ? (long long)from : -1;
    if (from > hn || nn > hn) return -1;
    for (i = from; i + nn <= hn; i++)
        if (hay[i] == needle[0] && memcmp(hay + i, needle, nn) == 0) return (long long)i;
    return -1;
}

/* Python `==` over JSON values: numbers by value (bool counts as 0/1),
 * objects key-order-insensitive, containers element-wise. */
static int cov_nullish(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static int cov_py_eq(const json_t *a, const json_t *b) {
    size_t i, n;
    if (cov_nullish(a) || cov_nullish(b)) return cov_nullish(a) && cov_nullish(b);
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_STRING:
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!cov_py_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        n = a->c.count;
        if (n != b->c.count) return 0;
        for (i = 0; i < n; i++) {
            const char *k = a->c.keys[i];
            if (!k) return 0;
            if (!cov_py_eq(a->c.items[i], json_obj_get(b, k))) return 0;
        }
        return 1;
    default:
        return 0;
    }
}

/* isinstance(value, int): an integral number, and True/False (bool is an int
 * subclass).  `_row_id` is read through this, and a bool stamp still names a
 * single row, so such a dict is excluded from the "behind" count. */
static int cov_is_instance_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    if (v->type != JSON_NUMBER) return 0;
    if (v->num_val != v->num_val) return 0;
    if (v->num_val > -1e18 && v->num_val < 1e18) return (double)(long long)v->num_val == v->num_val;
    return 0;
}

/* type(value) is int (not bool): the stamps size the rewind through this. */
static int cov_is_exact_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && cov_is_instance_int(v);
}

static long long cov_int_of(const json_t *v, long long def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}

/* len(value or ()) for the shapes the stamps hold. */
static long long cov_len_or0(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_STRING: return (long long)strlen(v->str_val ? v->str_val : "");
    case JSON_ARRAY:  return (long long)json_len(v);
    case JSON_OBJECT: return (long long)v->c.count;
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return 0;
    case JSON_NUMBER: return 0;
    }
    return 0;
}

static const char *cov_str_of(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* --------------------------------------------------------------- ported */

/* _text_of */
json_t *w34_coverage_56490e_text_of(const json_t *in) {
    const json_t *content = in && in->type == JSON_OBJECT ? json_obj_get(in, "content") : NULL;
    cov_sb sb;
    size_t i, n;
    int first = 1;
    json_t *out;

    if (content && content->type == JSON_STRING)
        return json_string(content->str_val ? content->str_val : "");
    if (!content || content->type != JSON_ARRAY) return json_string("");

    memset(&sb, 0, sizeof sb);
    n = json_len(content);
    for (i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        const json_t *type;
        char *text;
        if (!part || part->type != JSON_OBJECT) continue;
        type = json_obj_get(part, "type");
        if (!type || type->type != JSON_STRING || !type->str_val) continue;
        if (strcmp(type->str_val, "text") && strcmp(type->str_val, "input_text") &&
            strcmp(type->str_val, "output_text")) continue;
        if (!first) cov_sb_cstr(&sb, "\n");  /* "\n".join: between kept parts only */
        first = 0;
        text = cov_text_value(json_obj_get(part, "text"));
        if (!text) { free(sb.buf); return json_string(""); }
        cov_sb_add(&sb, text, strlen(text));
        free(text);
    }
    if (sb.oom) { free(sb.buf); return json_string(""); }
    out = json_string(sb.buf ? sb.buf : "");
    free(sb.buf);
    return out;
}

/* _holds_as_block */
json_t *w34_coverage_56490e_holds_as_block(const json_t *in) {
    const char *inner = cov_str_of(in && in->type == JSON_OBJECT ? json_obj_get(in, "inner") : NULL);
    const char *outer = cov_str_of(in && in->type == JSON_OBJECT ? json_obj_get(in, "outer") : NULL);
    size_t in_len = strlen(inner), on_len = strlen(outer);
    size_t line_lo = 0, scan_lo = 0, next_nl;
    long long start, next_after;

    /* Cursors keep the two line-edge scans linear over the whole search: hits
     * arrive in ascending order, so no byte is rescanned. */
    next_nl = on_len;
    { size_t k; for (k = 0; k < on_len; k++) if (outer[k] == '\n') { next_nl = k; break; } }

    start = cov_find(outer, on_len, inner, in_len, 0);
    while (start != -1) {
        size_t s = (size_t)start, e = s + in_len, line_end;
        while (scan_lo < s) {
            if (outer[scan_lo] == '\n') line_lo = scan_lo + 1;
            scan_lo++;
        }
        while (next_nl < e) {
            next_after = cov_find(outer, on_len, "\n", 1, next_nl + 1);
            next_nl = next_after < 0 ? on_len : (size_t)next_after;
            if (next_nl == on_len) break;
        }
        line_end = next_nl;
        /* rpartition("\n")[2] before the hit; partition("\n")[0] after it */
        if (cov_blank(outer, line_lo, s) && cov_blank(outer, e, line_end))
            return json_bool(true);
        start = cov_find(outer, on_len, inner, in_len, s + 1);
    }
    return json_bool(false);
}

static int cov_is_image_part(const json_t *part) {
    size_t i;
    const json_t *type;
    if (!part || part->type != JSON_OBJECT) return 0;
    type = json_obj_get(part, "type");
    if (!type || type->type != JSON_STRING || !type->str_val) return 0;
    for (i = 0; i < COV_IMAGE_PART_TYPES_N; i++)
        if (!strcmp(type->str_val, COV_IMAGE_PART_TYPES[i])) return 1;
    return 0;
}

/* _images_replaced */
json_t *w34_coverage_56490e_images_replaced(const json_t *in) {
    const json_t *stored = in && in->type == JSON_OBJECT ? json_obj_get(in, "stored") : NULL;
    const json_t *held = in && in->type == JSON_OBJECT ? json_obj_get(in, "held") : NULL;
    size_t i, n;
    int swapped = 0;

    if (!stored || !held || stored->type != JSON_ARRAY || held->type != JSON_ARRAY) return json_bool(false);
    n = json_len(stored);
    if (n != json_len(held)) return json_bool(false);
    for (i = 0; i < n; i++) {
        const json_t *was = json_get(stored, i);
        const json_t *now = json_get(held, i);
        const json_t *now_type;
        if (cov_py_eq(was, now)) continue;
        now_type = now && now->type == JSON_OBJECT ? json_obj_get(now, "type") : NULL;
        if (!(cov_is_image_part(was) && now_type && now_type->type == JSON_STRING &&
              now_type->str_val && !strcmp(now_type->str_val, "text")))
            return json_bool(false);
        swapped = 1;
    }
    return json_bool(swapped ? true : false);
}

/* _carried_tail_slots */
json_t *w34_coverage_56490e_carried_tail_slots(const json_t *in) {
    const json_t *carried = in && in->type == JSON_OBJECT ? json_obj_get(in, "carried") : NULL;
    long long compacted = cov_int_of(in && in->type == JSON_OBJECT ? json_obj_get(in, "compacted_count") : NULL, 0);
    long long tail = cov_int_of(in && in->type == JSON_OBJECT ? json_obj_get(in, "tail_count") : NULL, 0);
    long long threshold, count = 0;
    size_t i, n;

    if (tail <= 0 || !carried || carried->type != JSON_OBJECT) return json_number(0.0);
    threshold = compacted - tail;
    n = carried->c.count;
    for (i = 0; i < n; i++) {
        const json_t *v = carried->c.items[i];
        if (!cov_is_instance_int(v)) continue;
        if (cov_int_of(v, 0) >= threshold) count++;
    }
    return json_number((double)count);
}

/* _uncounted_merged_rows */
json_t *w34_coverage_56490e_uncounted_merged_rows(const json_t *in) {
    const json_t *tail = in && in->type == JSON_OBJECT ? json_obj_get(in, "tail") : NULL;
    long long total = 0;
    size_t i, n;

    if (!tail || tail->type != JSON_ARRAY) return json_number(0.0);
    n = json_len(tail);
    for (i = 0; i < n; i++) {
        const json_t *message = json_get(tail, i);
        const json_t *merged, *unnamed;
        long long unlisted = 0, extra = 0;
        if (!message || message->type != JSON_OBJECT) continue;
        if (cov_is_instance_int(json_obj_get(message, COV_ROW_ID))) continue;
        merged = json_obj_get(message, COV_MERGED_DURABLE_ROWS);
        unnamed = json_obj_get(message, COV_UNNAMED_DURABLE_ROWS);
        if (cov_is_exact_int(merged))
            unlisted = cov_int_of(merged, 0) - 1 - cov_len_or0(json_obj_get(message, COV_ABSORBED_ROW_IDS));
        if (unlisted > 0) total += unlisted;
        if (cov_is_exact_int(unnamed)) {
            extra = cov_int_of(unnamed, 0);
            total += extra;
        }
    }
    return json_number((double)total);
}

/* _tail_originals */
json_t *w34_coverage_56490e_tail_originals(const json_t *in) {
    const json_t *covered = in && in->type == JSON_OBJECT ? json_obj_get(in, "covered_active") : NULL;
    const json_t *merged = in && in->type == JSON_OBJECT ? json_obj_get(in, "merged_away") : NULL;
    long long tail = cov_int_of(in && in->type == JSON_OBJECT ? json_obj_get(in, "tail_count") : NULL, 0);
    json_t *out = json_array();
    long long *ids = NULL, *away = NULL;
    size_t n = 0, na = 0, i, guard, guard_max;
    long long width = tail, need;
    size_t start;

    if (covered && covered->type == JSON_ARRAY) {
        n = json_len(covered);
        if (n) {
            ids = (long long *)calloc(n, sizeof(long long));
            if (!ids) return out;
            for (i = 0; i < n; i++) ids[i] = cov_int_of(json_get(covered, i), 0);
        }
    }
    if (merged && merged->type == JSON_ARRAY) {
        na = json_len(merged);
        if (na) {
            away = (long long *)calloc(na, sizeof(long long));
            if (!away) { free(ids); return out; }
            for (i = 0; i < na; i++) away[i] = cov_int_of(json_get(merged, i), 0);
        }
    }

    guard_max = (long long)n * 2 + 16;
    for (guard = 0; guard < guard_max; guard++) {
        long long hits = 0;
        long long idx = -width;  /* covered_active[-width:] */
        if (idx >= 0) start = (size_t)idx < n ? (size_t)idx : n;
        else start = ((size_t)(-idx) < n) ? n - (size_t)(-idx) : 0;
        for (i = start; i < n; i++) {
            size_t j;
            for (j = 0; j < na; j++)
                if (away[j] == ids[i]) { hits++; break; }
        }
        need = tail + hits;
        if (need <= width || width >= (long long)n) break;
        width = need;
    }
    for (i = start; i < n; i++) json_append(out, json_number((double)ids[i]));
    free(ids);
    free(away);
    return out;
}
