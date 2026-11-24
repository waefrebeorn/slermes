/*
 * inline_picker.c - C11 port of upstream
 * plugins/platforms/telegram/inline_picker.py (gate w23_inlinepick_18ba58).
 *
 * PoP: w23_inlinepick_18ba58_page_size @ plugins/platforms/telegram/inline_picker.py:PAGE_SIZE
 * PoP: w23_inlinepick_18ba58_cache_seconds @ plugins/platforms/telegram/inline_picker.py:CACHE_TIME_SECONDS
 * PoP: w23_inlinepick_18ba58_collect @ plugins/platforms/telegram/inline_picker.py:collect_inline_catalog
 * PoP: w23_inlinepick_18ba58_filter @ plugins/platforms/telegram/inline_picker.py:filter_catalog
 * PoP: w23_inlinepick_18ba58_build @ plugins/platforms/telegram/inline_picker.py:build_inline_results
 *
 * The command/skill registry is a seam: the caller hands over the rows that
 * telegram_bot_commands() and _collect_gateway_skill_entries() would have
 * returned.  Everything else here is the module's own logic -- the
 * first-occurrence-wins merge, the three-tier ranking and the page slicing.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_inlinepick_18ba58.h"

#define IP_PAGE 50
#define IP_CACHE 10

/* ---- small string helpers (no strdup, no VLA) ---------------------------- */

static char *ip_dup_n(const char *s, size_t n)
{
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *ip_cat2(const char *a, const char *b)
{
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0;
    char *out = (char *)malloc(la + lb + 1);
    if (!out) return NULL;
    if (la) memcpy(out, a, la);
    if (lb) memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}

/* CPython str.strip() whitespace, ASCII half of it. */
static int ip_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *ip_strip(const char *s)
{
    size_t a = 0, b;
    if (!s) return ip_dup_n("", 0);
    b = strlen(s);
    while (a < b && ip_is_space(s[a])) a++;
    while (b > a && ip_is_space(s[b - 1])) b--;
    return ip_dup_n(s + a, b - a);
}

static void ip_lower(char *s)
{
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32);
}

/* str[:max_cp] counted in codepoints, exactly like a Python slice. */
static char *ip_prefix(const char *s, size_t max_cp)
{
    size_t i = 0, cp = 0, n;
    if (!s) return ip_dup_n("", 0);
    n = strlen(s);
    while (i < n && cp < max_cp) {
        unsigned char c = (unsigned char)s[i];
        size_t step = 1;
        if ((c & 0xE0) == 0xC0) step = 2;
        else if ((c & 0xF0) == 0xE0) step = 3;
        else if ((c & 0xF8) == 0xF0) step = 4;
        if (i + step > n) step = 1;
        i += step;
        cp++;
    }
    return ip_dup_n(s, i);
}

/* term = (term or "").strip().lower().lstrip("/") */
static char *ip_norm_term(const char *raw)
{
    char *t = ip_strip(raw);
    size_t i = 0;
    if (!t) return NULL;
    ip_lower(t);
    while (t[i] == '/') i++;
    if (i) memmove(t, t + i, strlen(t + i) + 1);
    return t;
}

/* item["name"].lower().replace("-", "_") */
static char *ip_norm_name(const char *name)
{
    char *n = ip_dup_n(name ? name : "", name ? strlen(name) : 0);
    size_t i;
    if (!n) return NULL;
    ip_lower(n);
    for (i = 0; n[i]; i++)
        if (n[i] == '-') n[i] = '_';
    return n;
}

/* ---- collect_inline_catalog --------------------------------------------- */

/* `desc or ""` keeps a truthy non-string description as it came in. */
static json_t *ip_desc_value(const json_t *desc)
{
    if (!desc) return json_string("");
    switch (desc->type) {
    case JSON_NULL:
        return json_string("");
    case JSON_STRING:
        return json_string(desc->str_val && desc->str_val[0] ? desc->str_val : "");
    case JSON_BOOL:
        return desc->bool_val ? json_copy(desc) : json_string("");
    case JSON_NUMBER:
        return desc->num_val != 0.0 ? json_copy(desc) : json_string("");
    case JSON_ARRAY:
    case JSON_OBJECT:
        return desc->c.count != 0 ? json_copy(desc) : json_string("");
    }
    return json_string("");
}

static int ip_name_seen(const json_t *catalog, const char *name)
{
    size_t i, n = json_len(catalog);
    for (i = 0; i < n; i++) {
        const char *have = json_get_str(json_get(catalog, i), "name", NULL);
        if (have && strcmp(have, name) == 0) return 1;
    }
    return 0;
}

static void ip_add(json_t *catalog, const json_t *name_v, const json_t *desc_v)
{
    const char *name;
    json_t *row;
    if (!name_v || name_v->type != JSON_STRING) return;
    name = name_v->str_val;
    if (!name || !name[0]) return;
    if (ip_name_seen(catalog, name)) return;
    row = json_object();
    if (!row) return;
    json_set(row, "name", json_string(name));
    json_set(row, "description", ip_desc_value(desc_v));
    json_append(catalog, row);
}

/* `for name, desc in telegram_bot_commands()`: a row that is not a 2-tuple
 * raises out of the loop, which the caller swallows -- the rows already
 * collected stay, the rest of that source is lost. */
static void ip_add_pairs(json_t *catalog, const json_t *rows)
{
    size_t i, n;
    if (!rows || rows->type != JSON_ARRAY) return;
    n = json_len(rows);
    for (i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_ARRAY || json_len(row) != 2) return;
        ip_add(catalog, json_get(row, 0), json_get(row, 1));
    }
}

/* `for entry in entries: _add(entry[0], entry[1])` -- no unpacking, but a
 * short entry raises IndexError and ends the source. */
static void ip_add_entries(json_t *catalog, const json_t *rows)
{
    size_t i, n;
    if (!rows || rows->type != JSON_ARRAY) return;
    n = json_len(rows);
    for (i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_ARRAY || json_len(row) < 2) return;
        ip_add(catalog, json_get(row, 0), json_get(row, 1));
    }
}

json_t *w23_inlinepick_18ba58_collect(const json_t *world)
{
    json_t *catalog = json_array();
    if (!catalog) return NULL;
    if (!world || world->type != JSON_OBJECT) return catalog;
    /* the registry import is inside a try: its failure leaves an empty catalog */
    if (!json_get_bool(world, "registry", true)) return catalog;
    if (!json_get_bool(world, "bot_error", false))
        ip_add_pairs(catalog, json_obj_get(world, "bot"));
    if (!json_get_bool(world, "skills_error", false))
        ip_add_entries(catalog, json_obj_get(world, "skills"));
    return catalog;
}

/* ---- filter_catalog ----------------------------------------------------- */

/* prefix > name-substring > description, each class in catalog order. */
static json_t *ip_filter(const json_t *catalog, const char *term_raw)
{
    json_t *out = json_array();
    char *term = ip_norm_term(term_raw);
    size_t i, n;
    if (!out) {
        free(term);
        return NULL;
    }
    n = json_len(catalog);
    if (!term || !term[0]) {
        for (i = 0; i < n; i++) json_append(out, json_copy(json_get(catalog, i)));
        free(term);
        return out;
    }
    {
        char *norm = ip_dup_n(term, strlen(term));
        size_t pass, tlen;
        if (!norm) {
            free(term);
            return out;
        }
        for (i = 0; norm[i]; i++)
            if (norm[i] == '-') norm[i] = '_';
        tlen = strlen(norm);
        for (pass = 0; pass < 3; pass++) {
            for (i = 0; i < n; i++) {
                json_t *item = json_get(catalog, i);
                const char *name = json_get_str(item, "name", NULL);
                const char *desc = json_get_str(item, "description", "");
                char *nname;
                int cls = -1;
                if (!name) continue;
                nname = ip_norm_name(name);
                if (!nname) continue;
                if (strncmp(nname, norm, tlen) == 0) {
                    cls = 0;
                } else if (strstr(nname, norm) != NULL) {
                    cls = 1;
                } else {
                    char *dl = ip_dup_n(desc, strlen(desc));
                    if (dl) {
                        ip_lower(dl);
                        if (strstr(dl, term) != NULL) cls = 2;
                        free(dl);
                    }
                }
                free(nname);
                if (cls == (int)pass) json_append(out, json_copy(item));
            }
        }
        free(norm);
    }
    free(term);
    return out;
}

json_t *w23_inlinepick_18ba58_filter(const json_t *world)
{
    const json_t *catalog;
    const json_t *term;
    if (!world || world->type != JSON_OBJECT) return json_array();
    catalog = json_obj_get(world, "catalog");
    term = json_obj_get(world, "term");
    return ip_filter(catalog, term && term->type == JSON_STRING ? term->str_val : NULL);
}

/* ---- build_inline_results ---------------------------------------------- */

/* `int(offset) if offset else 0`, with int()'s refusals reading as 0. */
static long long ip_int_of(const json_t *v)
{
    const char *s;
    size_t i = 0, digits = 0;
    long long acc = 0;
    int neg = 0;
    if (!v) return 0;
    if (v->type == JSON_NUMBER) {
        if (v->num_val != v->num_val) return 0;
        if (v->num_val > 9.0e18 || v->num_val < -9.0e18) return 0;
        return (long long)v->num_val;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type != JSON_STRING || !v->str_val) return 0;
    s = v->str_val;
    while (ip_is_space(s[i])) i++;
    if (s[i] == '+' || s[i] == '-') {
        neg = s[i] == '-';
        i++;
    }
    while (s[i] >= '0' && s[i] <= '9') {
        if (acc < 100000000000000000LL) acc = acc * 10 + (s[i] - '0');
        i++;
        digits++;
    }
    if (!digits) return 0;
    while (ip_is_space(s[i])) i++;
    if (s[i]) return 0;
    return neg ? -acc : acc;
}

/* "/<name>" + (" " + args when args). */
static char *ip_message(const char *name, const char *args)
{
    char *raw, *out;
    if (!args || !args[0]) {
        raw = ip_dup_n(name, strlen(name));
    } else {
        char *tmp = ip_cat2(name, " ");
        raw = tmp ? ip_cat2(tmp, args) : NULL;
        free(tmp);
    }
    if (!raw) return NULL;
    out = ip_cat2("/", raw);
    free(raw);
    return out;
}

json_t *w23_inlinepick_18ba58_build(const json_t *world)
{
    const json_t *catalog = NULL;
    const json_t *offset_v = NULL;
    const json_t *page_v = NULL;
    const char *query = "";
    char *qs, *term = NULL, *args = NULL;
    char numbuf[32];
    json_t *matches, *out, *results;
    long long start, page, n, i, end;

    out = json_object();
    results = json_array();
    if (!out || !results) {
        json_free(out);
        json_free(results);
        return NULL;
    }
    if (world && world->type == JSON_OBJECT) {
        catalog = json_obj_get(world, "catalog");
        offset_v = json_obj_get(world, "offset");
        page_v = json_obj_get(world, "page_size");
        query = json_get_str(world, "query", "");
    }

    /* parts = (query or "").strip().split(None, 1) */
    qs = ip_strip(query);
    if (!qs) {
        json_free(out);
        json_free(results);
        return NULL;
    }
    if (qs[0]) {
        size_t k = 0, a;
        while (qs[k] && !ip_is_space(qs[k])) k++;
        term = ip_dup_n(qs, k);
        a = k;
        while (qs[a] && ip_is_space(qs[a])) a++;
        args = ip_dup_n(qs + a, strlen(qs + a));
    } else {
        term = ip_dup_n("", 0);
        args = ip_dup_n("", 0);
    }
    free(qs);
    if (!term || !args) {
        free(term);
        free(args);
        json_free(out);
        json_free(results);
        return NULL;
    }

    matches = ip_filter(catalog, term);
    start = ip_int_of(offset_v);
    if (start < 0) start = 0;
    page = page_v ? ip_int_of(page_v) : (long long)IP_PAGE;
    if (page < 0) page = 0;
    n = (long long)json_len(matches);
    /* start + page on a client-controlled page size: saturate instead of UB. */
    end = (page > 0 && start > LLONG_MAX - page) ? LLONG_MAX : start + page;

    snprintf(numbuf, sizeof(numbuf), "%lld", end);
    json_set(out, "next_offset", json_string(n > end && end != LLONG_MAX ? numbuf : ""));

    for (i = start; i < end && i < n; i++) {
        json_t *item = json_get(matches, i);
        const char *name = json_get_str(item, "name", NULL);
        const char *desc = json_get_str(item, "description", "");
        char *idkey, *id, *title, *d100, *msg;
        json_t *row;
        if (!name) continue;
        snprintf(numbuf, sizeof(numbuf), "%lld:", start);
        idkey = ip_cat2(numbuf, name);
        id = ip_prefix(idkey, 64);
        title = ip_cat2("/", name);
        d100 = ip_prefix(desc, 100);
        msg = ip_message(name, args);
        {
            char *sliced = ip_prefix(msg, 4096);
            free(msg);
            msg = sliced;
        }
        row = json_object();
        if (row) {
            json_set(row, "id", json_string(id ? id : ""));
            json_set(row, "title", json_string(title ? title : ""));
            json_set(row, "description", json_string(d100 ? d100 : ""));
            json_set(row, "message_text", json_string(msg ? msg : ""));
            json_append(results, row);
        }
        free(idkey);
        free(id);
        free(title);
        free(d100);
        free(msg);
    }

    json_set(out, "results", results);
    json_free(matches);
    free(term);
    free(args);
    return out;
}

int w23_inlinepick_18ba58_page_size(void) { return IP_PAGE; }
int w23_inlinepick_18ba58_cache_seconds(void) { return IP_CACHE; }
