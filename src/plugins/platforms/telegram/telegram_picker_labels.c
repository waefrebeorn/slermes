/*
 * telegram_picker_labels.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_picker_labels.py (gate
 * w24_telegrampi_acbcc8).
 *
 * PoP: w24_telegrampi_acbcc8_shared_label_prefix @ plugins/platforms/telegram/telegram_picker_labels.py:shared_label_prefix
 * PoP: w24_telegrampi_acbcc8_model_button_label @ plugins/platforms/telegram/telegram_picker_labels.py:model_button_label
 * PoP: w24_telegrampi_acbcc8_around_difference @ plugins/platforms/telegram/telegram_picker_labels.py:_around_difference
 * PoP: w24_telegrampi_acbcc8_labels @ plugins/platforms/telegram/telegram_picker_labels.py:model_button_labels
 * PoP: w24_telegrampi_acbcc8_constants @ plugins/platforms/telegram/telegram_picker_labels.py:MODEL_LABEL_BUDGET
 *
 * Ported: the whole module.  It is display-only text arithmetic, so there is no
 * seam at all -- no Telegram client, no callback data, no catalog fetch.  The
 * caller owns the ids; ``suffix_for`` (a callable) becomes a {id: suffix} map,
 * which is everything a pure function can see of it.
 *
 * Lengths and slices are counted in Unicode code points (Python ``str``), so a
 * multi-byte id is cut where Python cuts it.  Python's negative slice stops
 * (``s[:room-1]`` with a small room, ``s[-(k):]`` with k == 0) are reproduced
 * exactly, including the overlap a budget smaller than the head produces.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegrampi_acbcc8.h"

#define LABEL_BUDGET W24_TELEGRAMPI_ACBCC8_MODEL_LABEL_BUDGET
#define ELLIPSIS "\xE2\x80\xA6"       /* U+2026 ... */
#define MIDDOT_SP " \xC2\xB7 "        /* " . " (U+00B7) */

/* ---------------------------------------------------------------- strings */

static const char *as_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static char *xdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static size_t cp_len_n(const char *s, size_t bytes) {
    size_t n = 0, i;
    for (i = 0; i < bytes && s[i]; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) n++;
    return n;
}

static size_t cp_len(const char *s) { return s ? cp_len_n(s, strlen(s)) : 0; }

/* Byte offset just past code point `cp` (clamped at the terminator). */
static size_t cp_off(const char *s, size_t cp) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t i = 0, n = 0;
    while (p[i] && n < cp) {
        i++;
        while (p[i] && (p[i] & 0xC0) == 0x80) i++;
        n++;
    }
    return i;
}

/* Python s[i:j] over code points, with negative indices and clamping. */
static char *cp_slice(const char *src, long long i, long long j) {
    const char *s = src ? src : "";
    size_t n = cp_len(s), oa, ob;
    long long a = i, b = j;
    char *out;
    if (a < 0) { a += (long long)n; if (a < 0) a = 0; } else if (a > (long long)n) a = (long long)n;
    if (b < 0) { b += (long long)n; if (b < 0) b = 0; } else if (b > (long long)n) b = (long long)n;
    if (b < a) b = a;
    oa = cp_off(s, (size_t)a);
    ob = cp_off(s, (size_t)b);
    out = (char *)malloc(ob - oa + 1);
    if (!out) return NULL;
    memcpy(out, s + oa, ob - oa);
    out[ob - oa] = '\0';
    return out;
}

static char *join3(const char *a, const char *mid, const char *b) {
    size_t la = a ? strlen(a) : 0, lm = mid ? strlen(mid) : 0, lb = b ? strlen(b) : 0;
    char *o = (char *)malloc(la + lm + lb + 1);
    if (!o) return NULL;
    if (la) memcpy(o, a, la);
    if (lm) memcpy(o + la, mid, lm);
    if (lb) memcpy(o + la + lm, b, lb);
    o[la + lm + lb] = '\0';
    return o;
}

static json_t *take(char *s) {
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* os.path.commonprefix over two strings, counted in code points. */
static size_t common_prefix_len(const char *a, const char *b) {
    size_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return cp_len_n(a, i);
}

/* max(len(m) for m in model_ids); *ok = 0 when an element is not a string. */
static size_t max_len(const json_t *arr, int *ok) {
    size_t i, n = json_len(arr), best = 0;
    *ok = 1;
    for (i = 0; i < n; i++) {
        const char *s = as_str(json_get(arr, i));
        size_t l;
        if (!s) { *ok = 0; return 0; }
        l = cp_len(s);
        if (l > best) best = l;
    }
    return best;
}

/* ---------------------------------------------------------------- pieces */

/* MODEL_LABEL_BUDGET - (len(suffix) + 3 if suffix else 0), signed like Python. */
static long room_signed(const char *suffix) {
    size_t sl = cp_len(suffix);
    return (long)LABEL_BUDGET - (sl ? (long)sl + 3 : 0);
}

static char *with_suffix(const char *short_id, const char *suffix) {
    if (!suffix || !*suffix) return xdup(short_id);
    return join3(short_id, MIDDOT_SP, suffix);
}

/* _strip: drop the shared prefix, then keep the tail after the last '/'. */
static char *strip_id(const char *model_id, const char *prefix) {
    const char *rest = model_id;
    const char *slash = NULL, *p;
    if (prefix && *prefix) {
        size_t pl = strlen(prefix);
        if (strncmp(model_id, prefix, pl) == 0) rest = model_id + pl;
    }
    for (p = rest; *p; p++)
        if (*p == '/') slash = p;
    if (slash) rest = slash + 1;
    return xdup(rest);
}

/* _around_difference over a C string list (never empty: caller applies `or [""]`). */
static char *around_difference(const char *name, const char *const *others, size_t n_others, long room) {
    size_t n = cp_len(name), i, diff = 0;
    long head, start;
    char *body, *head_s, *out, *trimmed, *joined;

    if ((long)n <= room) return xdup(name);

    for (i = 0; i < n_others; i++) {
        size_t c = common_prefix_len(name, others[i]);
        if (c > diff) diff = c;
    }
    if ((long)diff < room - 4) {
        char *head_only = cp_slice(name, 0, room - 1);
        out = join3(head_only, ELLIPSIS, "");
        free(head_only);
        return out;
    }
    head = room / 4;
    if (head < 4) head = 4;
    start = (long)diff - 4;
    if (start < head + 1) start = head + 1;
    body = cp_slice(name, start, start + room - head - 1);
    if (body && start + (long)cp_len(body) < (long)n) {
        trimmed = cp_slice(body, 0, (long)cp_len(body) - 1);
        joined = join3(trimmed, ELLIPSIS, "");
        free(trimmed);
        free(body);
        body = joined;
    }
    head_s = cp_slice(name, 0, head);
    out = join3(head_s, ELLIPSIS, body ? body : "");
    free(head_s);
    free(body);
    return out;
}

/* ------------------------------------------------------------- public API */

json_t *w24_telegrampi_acbcc8_shared_label_prefix(const json_t *model_ids) {
    size_t n, i, best = 0, cut = 0;
    int ok = 1;
    char *common;

    if (!model_ids || model_ids->type != JSON_ARRAY) return json_string("");
    n = json_len(model_ids);
    if (n < 2) return json_string("");
    best = max_len(model_ids, &ok);
    if (!ok) return json_string("ERROR:type");
    if (best <= LABEL_BUDGET) return json_string("");

    common = xdup(as_str(json_get(model_ids, 0)));
    if (!common) return json_null();
    for (i = 1; i < n; i++) {
        const char *other = as_str(json_get(model_ids, i));
        size_t c = common_prefix_len(common, other);
        common[c] = '\0';
    }
    {
        size_t cl = cp_len(common);
        for (i = 0; i < cl; i++) {
            size_t off = cp_off(common, i);
            if (common[off] == '.') cut = i + 1;   /* m.end() of the last dot */
        }
    }
    if (cut == 0) { free(common); return json_string(""); }
    for (i = 0; i < n; i++)
        if (cp_len(as_str(json_get(model_ids, i))) <= cut) { free(common); return json_string(""); }
    {
        char *pfx = cp_slice(common, 0, (long)cut);
        json_t *out = json_string(pfx ? pfx : "");
        free(pfx);
        free(common);
        return out;
    }
}

json_t *w24_telegrampi_acbcc8_model_button_label(const json_t *model_id,
                                                 const json_t *prefix,
                                                 const json_t *suffix) {
    const char *id = as_str(model_id);
    const char *pfx, *sfx;
    char *short_id, *out;
    long room, head, k, start;
    size_t n;

    if (!id) return json_string("ERROR:type");
    if (prefix && prefix->type != JSON_NULL && !as_str(prefix)) return json_string("ERROR:type");
    if (suffix && suffix->type != JSON_NULL && !as_str(suffix)) return json_string("ERROR:type");
    pfx = as_str(prefix);
    sfx = as_str(suffix);
    if (!sfx) sfx = "";

    short_id = strip_id(id, pfx);
    if (!short_id) return json_null();
    room = room_signed(sfx);
    n = cp_len(short_id);
    if ((long)n > room) {
        char *hs, *ts;
        head = room / 3;               /* Python // floors toward -inf */
        if (head < 4) head = 4;
        k = room - head - 1;
        /* Python short[-k:]: k > 0 -> last k chars, k == 0 -> whole string,
         * k < 0 -> from index -k. */
        start = (k > 0) ? (long)n - k : -k;
        hs = cp_slice(short_id, 0, head);
        ts = cp_slice(short_id, start, (long)n);
        {
            char *mid = join3(hs, ELLIPSIS, ts);
            free(hs);
            free(ts);
            free(short_id);
            short_id = mid;
        }
    }
    out = with_suffix(short_id ? short_id : "", sfx);
    free(short_id);
    return take(out);
}

json_t *w24_telegrampi_acbcc8_around_difference(const json_t *name,
                                                const json_t *others,
                                                const json_t *room) {
    const char *nm = as_str(name);
    const char *empty[1];
    const char **list = NULL;
    size_t n = 0, i, cap = 0;
    char *out;
    long r = 0;

    empty[0] = "";
    if (!nm) return json_string("ERROR:type");
    if (room && room->type == JSON_NUMBER) r = (long)room->num_val;
    else if (room && room->type == JSON_BOOL) r = room->bool_val ? 1 : 0;
    else if (room && room->type != JSON_NULL) return json_string("ERROR:type");

    if (others && others->type == JSON_ARRAY) {
        cap = json_len(others);
        if (cap) list = (const char **)calloc(cap, sizeof(char *));
    }
    for (i = 0; i < cap; i++) {
        const char *o = as_str(json_get(others, i));
        if (list) list[n++] = o ? o : "";
    }
    if (n == 0) {                       /* caller's `or [""]` fallback */
        list = empty;
        n = 1;
    }
    out = around_difference(nm, list, n, r);
    if (list != empty) free((void *)list);
    return take(out);
}

json_t *w24_telegrampi_acbcc8_labels(const json_t *model_ids,
                                     const json_t *suffixes) {
    size_t n, i, j;
    int ok = 1;
    char *prefix = NULL;
    char **names = NULL, **sfx = NULL, **base = NULL, **labels = NULL;
    const char **others = NULL;
    const char *empty[1];
    json_t *prefix_node = NULL, *out = json_array();

    empty[0] = "";
    if (!model_ids || model_ids->type != JSON_ARRAY) return json_string("ERROR:type");
    n = json_len(model_ids);
    (void)max_len(model_ids, &ok);
    if (!ok) return json_string("ERROR:type");
    if (n == 0) return out;

    {
        json_t *p = w24_telegrampi_acbcc8_shared_label_prefix(model_ids);
        prefix = xdup(as_str(p));
        json_free(p);
    }
    prefix_node = json_string(prefix ? prefix : "");
    names = (char **)calloc(n, sizeof(char *));
    sfx = (char **)calloc(n, sizeof(char *));
    base = (char **)calloc(n, sizeof(char *));
    labels = (char **)calloc(n, sizeof(char *));
    others = (const char **)calloc(n, sizeof(char *));
    if (!names || !sfx || !base || !labels || !others) {
        free(names); free(sfx); free(base); free(labels); free((void *)others);
        free(prefix); json_free(prefix_node); json_free(out);
        return json_null();
    }
    for (i = 0; i < n; i++) {
        const char *s = NULL;
        json_t *lbl;
        names[i] = xdup(as_str(json_get(model_ids, i)));
        if (suffixes && suffixes->type == JSON_OBJECT)
            s = as_str(json_obj_get(suffixes, names[i]));   /* non-str suffix -> seam "" */
        sfx[i] = xdup(s ? s : "");
        lbl = w24_telegrampi_acbcc8_model_button_label(json_get(model_ids, i), prefix_node, json_null());
        base[i] = xdup(as_str(lbl));
        json_free(lbl);
        lbl = w24_telegrampi_acbcc8_model_button_label(json_get(model_ids, i), prefix_node,
                                                      json_string(sfx[i]));
        labels[i] = xdup(as_str(lbl));
        json_free(lbl);
    }

    /* Pass 1: clashes on the labels WITHOUT the plan word, resolved on full ids. */
    for (i = 0; i < n; i++) {
        size_t first = n;
        int count = 0;
        for (j = 0; j < n; j++)
            if (!strcmp(base[j], base[i])) { if (first == n) first = j; count++; }
        if (count < 2 || first != i) continue;      /* one group, at its first index */
        for (j = 0; j < n; j++) {
            size_t n_others = 0, k;
            char *shortened, *final;
            if (strcmp(base[j], base[i])) continue;
            for (k = 0; k < n; k++) {
                if (strcmp(base[k], base[i])) continue;
                if (strcmp(names[k], names[j])) others[n_others++] = names[k];
            }
            if (n_others == 0) others[n_others++] = "";
            shortened = around_difference(names[j], others, n_others, room_signed(sfx[j]));
            final = with_suffix(shortened ? shortened : "", sfx[j]);
            free(shortened);
            free(labels[j]);
            labels[j] = final;
        }
    }

    /* Pass 2: still clashing -> an ordinal, the catalog position (1-based). */
    for (i = 0; i < n; i++) {
        size_t first = n;
        int count = 0;
        for (j = 0; j < n; j++)
            if (!strcmp(labels[j], labels[i])) { if (first == n) first = j; count++; }
        if (count < 2 || first != i) continue;
        {
            char *orig = xdup(labels[i]);
            if (!orig) continue;
            for (j = 0; j < n; j++) {
                char tag[32];
                char *shortened, *with_tag, *final;
                if (strcmp(labels[j], orig)) continue;
                snprintf(tag, sizeof(tag), " #%lu", (unsigned long)(j + 1));
                shortened = around_difference(names[j], empty, 1,
                                              room_signed(sfx[j]) - (long)cp_len(tag));
                with_tag = join3(shortened ? shortened : "", tag, "");
                final = with_suffix(with_tag ? with_tag : "", sfx[j]);
                free(shortened);
                free(with_tag);
                free(labels[j]);
                labels[j] = final;
            }
            free(orig);
        }
    }

    for (i = 0; i < n; i++) json_append(out, json_string(labels[i] ? labels[i] : ""));
    for (i = 0; i < n; i++) { free(names[i]); free(sfx[i]); free(base[i]); free(labels[i]); }
    free(names); free(sfx); free(base); free(labels); free((void *)others);
    free(prefix);
    json_free(prefix_node);
    return out;
}

json_t *w24_telegrampi_acbcc8_constants(void) {
    json_t *o = json_object();
    json_set(o, "model_label_budget", json_number(LABEL_BUDGET));
    return o;
}
