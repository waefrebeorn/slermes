/*
 * manifest.c - C11 port of upstream Hermes agent/pet/manifest.py (gate w23_manifest_27beb4).
 * The public HTTP fetch (httpx + the SSRF-safe client), the process-wide cache and the wall
 * clock are seams; the row coercion, the eligibility filter, the cache decision and the slug
 * lookup are ported here and differentially verified against live Python.
 * PoP: agent_pet_manifest_pending @ agent/pet/manifest.py:<module>
 * PoP: w23_manifest_27beb4_entry_from_dict @ agent/pet/manifest.py:ManifestEntry.from_dict
 * PoP: w23_manifest_27beb4_fetch @ agent/pet/manifest.py:fetch_manifest
 * PoP: w23_manifest_27beb4_find_entry @ agent/pet/manifest.py:find_entry
 * PoP: w23_manifest_27beb4_consts @ agent/pet/manifest.py:<module constants>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const agent_pet_manifest_CONST_MANIFEST_URL = "https://petdex.dev/api/manifest";

void agent_pet_manifest_port_pending(void) { (void)0; }

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_manifest_27beb4.h"

/* ---- Python str()/repr() of a fixture value ------------------------------ */

static char *mn_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s ? s : "", n + 1);
    return out;
}

static char *mn_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = (char *)malloc(na + nb + 1);
    if (!out) return NULL;
    memcpy(out, a ? a : "", na);
    memcpy(out + na, b ? b : "", nb + 1);
    return out;
}

static char *mn_num(double v) {
    char tmp[64];
    int prec;
    if (v != v) return mn_dup("nan");
    if (v > -9.007199254740992e15 && v < 9.007199254740992e15) {
        double iv = (double)(long long)v;
        if (iv == v) {
            snprintf(tmp, sizeof tmp, "%lld", (long long)v);
            return mn_dup(tmp);
        }
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    return mn_dup(tmp);
}

/* Python repr of a string: single quotes unless that needs more escaping. */
static char *mn_repr_str(const char *s) {
    const char q = (s && strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
    const char *p;
    size_t n = 2, i = 0;
    char *out;
    for (p = s ? s : ""; *p; p++) {
        if (*p == '\\' || *p == '\n' || *p == '\r' || *p == '\t' || *p == q) n += 2;
        else n++;
    }
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    out[i++] = q;
    for (p = s ? s : ""; *p; p++) {
        if (*p == '\\') { out[i++] = '\\'; out[i++] = '\\'; }
        else if (*p == '\n') { out[i++] = '\\'; out[i++] = 'n'; }
        else if (*p == '\r') { out[i++] = '\\'; out[i++] = 'r'; }
        else if (*p == '\t') { out[i++] = '\\'; out[i++] = 't'; }
        else if (*p == q) { out[i++] = '\\'; out[i++] = *p; }
        else out[i++] = *p;
    }
    out[i++] = q;
    out[i] = '\0';
    return out;
}

static char *mn_py_repr(const json_t *v);

static char *mn_py_str(const json_t *v) {
    if (!v) return mn_dup("");
    switch (v->type) {
    case JSON_NULL:
        return mn_dup("None");
    case JSON_BOOL:
        return mn_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        return mn_num(v->num_val);
    case JSON_STRING:
        return mn_dup(v->str_val ? v->str_val : "");
    default:
        return mn_py_repr(v);
    }
}

static char *mn_py_repr(const json_t *v) {
    size_t i, n;
    char *out, *piece, *next;
    if (!v) return mn_dup("None");
    switch (v->type) {
    case JSON_NULL:
        return mn_dup("None");
    case JSON_BOOL:
        return mn_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        return mn_num(v->num_val);
    case JSON_STRING:
        return mn_repr_str(v->str_val);
    case JSON_ARRAY:
        out = mn_dup("[");
        n = v->c.count;
        for (i = 0; i < n; i++) {
            piece = mn_py_repr(v->c.items[i]);
            if (i) { next = mn_join(out, ", "); free(out); out = next; }
            next = mn_join(out, piece ? piece : "");
            free(out);
            free(piece);
            out = next;
            if (!out) return NULL;
        }
        next = mn_join(out, "]");
        free(out);
        return next;
    case JSON_OBJECT:
        out = mn_dup("{");
        n = v->c.count;
        for (i = 0; i < n; i++) {
            char *key = mn_repr_str(v->c.keys[i]);
            piece = mn_py_repr(v->c.items[i]);
            if (i) { next = mn_join(out, ", "); free(out); out = next; }
            next = mn_join(out, key ? key : "");
            free(out);
            out = next;
            next = mn_join(out, ": ");
            free(out);
            out = next;
            next = mn_join(out, piece ? piece : "");
            free(out);
            free(piece);
            free(key);
            out = next;
            if (!out) return NULL;
        }
        next = mn_join(out, "}");
        free(out);
        return next;
    }
    return mn_dup("");
}

/* Python truthiness of a fixture value. */
static int mn_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY: return v->c.count > 0 ? 1 : 0;
    case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static char *mn_lower(const char *s) {
    size_t i, n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    out[n] = '\0';
    return out;
}

static char *mn_strip(const char *s) {
    const char *a = s ? s : "";
    size_t start = 0, end = strlen(a), n;
    char *out;
    while (start < end && (a[start] == ' ' || a[start] == '\t' || a[start] == '\n' ||
                           a[start] == '\r' || a[start] == '\f' || a[start] == '\v'))
        start++;
    while (end > start && (a[end - 1] == ' ' || a[end - 1] == '\t' || a[end - 1] == '\n' ||
                           a[end - 1] == '\r' || a[end - 1] == '\f' || a[end - 1] == '\v'))
        end--;
    n = end - start;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a + start, n);
    out[n] = '\0';
    return out;
}

/* data.get(key, "") -- a missing key and a present empty value read alike. */
static const json_t *mn_get(const json_t *data, const char *key) {
    return (data && data->type == JSON_OBJECT) ? json_obj_get(data, key) : NULL;
}

/* str(data.get(key, "") or fallback_value). */
static char *mn_field(const json_t *data, const char *key, const json_t *fallback) {
    const json_t *v = mn_get(data, key);
    if (mn_truthy(v)) return mn_py_str(v);
    return mn_py_str(fallback);
}

/* ---- the ported row coercion ------------------------------------------- */

json_t *w23_manifest_27beb4_entry_from_dict(const json_t *data) {
    json_t *o;
    char *slug, *display, *kind, *submitted, *sheet, *petjson, *zip;
    const json_t *raw_slug, *k;

    raw_slug = mn_get(data, "slug");
    slug = mn_strip(mn_py_str(raw_slug));
    display = mn_field(data, "displayName", raw_slug);
    k = mn_get(data, "kind");
    kind = mn_truthy(k) ? mn_py_str(k) : mn_dup("pet");
    submitted = mn_field(data, "submittedBy", NULL);
    sheet = mn_field(data, "spritesheetUrl", NULL);
    petjson = mn_field(data, "petJsonUrl", NULL);
    zip = mn_field(data, "zipUrl", NULL);

    o = json_object();
    json_set(o, "slug", json_string(slug ? slug : ""));
    json_set(o, "display_name", json_string(display ? display : ""));
    json_set(o, "kind", json_string(kind ? kind : ""));
    json_set(o, "submitted_by", json_string(submitted ? submitted : ""));
    json_set(o, "spritesheet_url", json_string(sheet ? sheet : ""));
    json_set(o, "pet_json_url", json_string(petjson ? petjson : ""));
    json_set(o, "zip_url", json_string(zip ? zip : ""));
    free(slug);
    free(display);
    free(kind);
    free(submitted);
    free(sheet);
    free(petjson);
    free(zip);
    return o;
}

/* ---- fetch_manifest over a scripted transport --------------------------- */

static json_t *mn_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}

static json_t *mn_fetch_error(const char *wrapped) {
    char *msg = mn_join("could not fetch petdex manifest: ", wrapped);
    json_t *o = mn_err(msg ? msg : "could not fetch petdex manifest: ");
    free(msg);
    return o;
}

/* The eligibility filter: a row needs both a slug and a spritesheet URL. */
static void mn_add_entries(json_t *out, const json_t *pets) {
    size_t i, n;
    if (!pets || pets->type != JSON_ARRAY) return;
    n = json_len(pets);
    for (i = 0; i < n; i++) {
        const json_t *raw = json_get(pets, i);
        json_t *entry;
        const char *slug, *sheet;
        if (!raw || raw->type != JSON_OBJECT) continue;
        entry = w23_manifest_27beb4_entry_from_dict(raw);
        slug = json_get_str(entry, "slug", "");
        sheet = json_get_str(entry, "spritesheet_url", "");
        if (slug && sheet && slug[0] && sheet[0]) json_append(out, entry);
        else json_free(entry);
    }
}

json_t *w23_manifest_27beb4_fetch(const json_t *world) {
    const json_t *cached, *payload, *pets, *err_node, *list;
    json_t *out, *entries;
    int force, warm;

    if (world && world->type != JSON_OBJECT) world = NULL;
    force = json_get_bool(world, "force", 0) ? 1 : 0;
    cached = world ? json_obj_get(world, "cached") : NULL;
    warm = (cached && json_get_bool(cached, "warm", 0)) ? 1 : 0;

    if (warm && !force) {
        size_t i, n;
        out = json_object();
        json_set(out, "ok", json_bool(true));
        json_set(out, "from_cache", json_bool(true));
        entries = json_array();
        list = json_obj_get(cached, "entries");
        n = (list && list->type == JSON_ARRAY) ? json_len(list) : 0;
        /* the cache holds ManifestEntry rows, so they come back through the
         * same coercion a network body would. */
        for (i = 0; i < n; i++) {
            const json_t *raw = json_get(list, i);
            if (!raw || raw->type != JSON_OBJECT) continue;
            json_append(entries, w23_manifest_27beb4_entry_from_dict(raw));
        }
        json_set(out, "entries", entries);
        return out;
    }

    /* import httpx -- a missing dependency fails outside the transport try. */
    if (!json_get_bool(world, "import_ok", 1))
        return mn_err("httpx is required to fetch the petdex manifest");

    /* The SSRF refusal is raised inside the try, so it comes back wrapped. */
    if (!json_get_bool(world, "safe", 1)) {
        char *wrapped = mn_join("Pet manifest URL failed the SSRF safety check: ",
                                W23_MANIFEST_27BEB4_URL);
        json_t *o = mn_fetch_error(wrapped ? wrapped : "");
        free(wrapped);
        return o;
    }

    err_node = world ? json_obj_get(world, "error") : NULL;
    if (err_node && err_node->type != JSON_NULL) {
        const char *text = (err_node->type == JSON_STRING && err_node->str_val)
                               ? err_node->str_val : "";
        return mn_fetch_error(text);
    }

    payload = world ? json_obj_get(world, "payload") : NULL;
    pets = (payload && payload->type == JSON_OBJECT) ? json_obj_get(payload, "pets") : NULL;
    if (!pets || pets->type != JSON_ARRAY)
        return mn_err("petdex manifest had no 'pets' array");

    out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "from_cache", json_bool(false));
    entries = json_array();
    mn_add_entries(entries, pets);
    json_set(out, "entries", entries);
    return out;
}

/* ---- find_entry over a supplied catalogue ------------------------------- */

json_t *w23_manifest_27beb4_find_entry(const json_t *req) {
    char *stripped, *needle;
    const json_t *list;
    json_t *entry = NULL, *out;
    size_t i, n;

    if (req && req->type != JSON_OBJECT) req = NULL;
    stripped = mn_strip(json_get_str(req, "slug", ""));
    needle = mn_lower(stripped ? stripped : "");
    free(stripped);

    list = req ? json_obj_get(req, "entries") : NULL;
    n = (list && list->type == JSON_ARRAY) ? json_len(list) : 0;
    for (i = 0; i < n && needle; i++) {
        const json_t *raw = json_get(list, i);
        json_t *candidate;
        char *slug, *low;
        if (!raw || raw->type != JSON_OBJECT) continue;
        candidate = w23_manifest_27beb4_entry_from_dict(raw);
        slug = mn_dup(json_get_str(candidate, "slug", ""));
        low = mn_lower(slug ? slug : "");
        if (low && !strcmp(low, needle)) entry = candidate;
        else json_free(candidate);
        free(slug);
        free(low);
        if (entry) break;
    }
    free(needle);

    out = json_object();
    json_set(out, "found", json_bool(entry ? true : false));
    json_set(out, "entry", entry ? entry : json_null());
    return out;
}

json_t *w23_manifest_27beb4_consts(void) {
    json_t *o = json_object();
    json_set(o, "url", json_string(W23_MANIFEST_27BEB4_URL));
    json_set(o, "ttl", json_number(W23_MANIFEST_27BEB4_TTL));
    json_set(o, "default_timeout", json_number(W23_MANIFEST_27BEB4_DEFAULT_TIMEOUT));
    return o;
}
