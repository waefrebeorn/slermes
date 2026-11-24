/*
 * availability.c -- real C11 port of the version comparison and the offerability
 * state machine of upstream Hermes hermes_platform/resolver/availability.py.
 *
 * PoP: w22_availabili_9d8d88_version_parts @ hermes_platform/resolver/availability.py:_version_parts
 * PoP: w22_availabili_9d8d88_version_at_least @ hermes_platform/resolver/availability.py:version_at_least
 * PoP: w22_availabili_9d8d88_offerable @ hermes_platform/resolver/availability.py:OFFERABLE
 * PoP: w22_availabili_9d8d88_as_dict @ hermes_platform/resolver/availability.py:Availability.as_dict
 * PoP: w22_availabili_9d8d88_evaluate @ hermes_platform/resolver/availability.py:availability
 *
 * Host seams, injected by the caller: `facts.os_family()`, `facts.gpu_class()`
 * and the AppResolver locate/inspect result. Nothing here probes the host,
 * spawns, or caches, exactly like the upstream module.
 */
#include "hermes_json.h"
#include "noc_w22_availabili_9d8d88.h"

#include <stdlib.h>
#include <string.h>

#define A9_MAX_COMPONENTS 24

/* Strict `[0-9]{1,9}(\.[0-9]{1,9})*`, as _version_parts spells it. */
static int a9_parts(const char *version, long long *out, int cap) {
    const char *p;
    int count = 0;

    if (!version || !version[0]) return -1;
    p = version;
    for (;;) {
        long long value = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (digits == 9) return -1;  /* ten digits: re.fullmatch fails */
            value = value * 10 + (*p - '0');
            digits++;
            p++;
        }
        if (!digits) return -1;
        if (count == cap) return -1;
        out[count++] = value;
        if (*p == '\0') return count;
        if (*p != '.') return -1;
        p++;
    }
}

/* Python list comparison: element-wise, then the longer list wins on a prefix. */
static int a9_cmp(const long long *a, int an, const long long *b, int bn) {
    int i;
    int n = an < bn ? an : bn;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    if (an == bn) return 0;
    return an < bn ? -1 : 1;
}

static json_t *a9_parts_array(const long long *parts, int count) {
    json_t *out = json_array();
    int i;
    if (!out) return NULL;
    for (i = 0; i < count; i++) json_append(out, json_number((double)parts[i]));
    return out;
}

json_t *w22_availabili_9d8d88_version_parts(const char *version) {
    long long parts[A9_MAX_COMPONENTS];
    int count = a9_parts(version, parts, A9_MAX_COMPONENTS);
    if (count < 0) return json_null();
    return a9_parts_array(parts, count);
}

int w22_availabili_9d8d88_version_at_least(const char *found, const char *minimum) {
    long long a[A9_MAX_COMPONENTS];
    long long b[A9_MAX_COMPONENTS];
    int an = a9_parts(found, a, A9_MAX_COMPONENTS);
    int bn = a9_parts(minimum, b, A9_MAX_COMPONENTS);
    int width, i;

    if (an < 0 || bn < 0) return 0;  /* fail closed */
    width = an > bn ? an : bn;
    for (i = an; i < width; i++) a[i] = 0;
    for (i = bn; i < width; i++) b[i] = 0;
    return a9_cmp(a, width, b, width) >= 0 ? 1 : 0;
}

int w22_availabili_9d8d88_offerable(const char *state) {
    if (!state) return 0;
    return !strcmp(state, "available") || !strcmp(state, "no_requirements");
}

json_t *w22_availabili_9d8d88_as_dict(const char *state, const char *version,
                                     const char *path, const char *minimum) {
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "state", json_string(state ? state : ""));
    json_set(out, "version", version ? json_string(version) : json_null());
    json_set(out, "path", path ? json_string(path) : json_null());
    json_set(out, "min_version", minimum ? json_string(minimum) : json_null());
    return out;
}

/* Python truthiness for the fixture's decoded values. */
static int a9_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* A string field, or NULL for absent/null/non-string (Python None). */
static const char *a9_str(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

/* `str` list element at index, or NULL. */
static const char *a9_list_str(const json_t *arr, size_t index) {
    json_t *v;
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    v = json_get(arr, index);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

/* The inspect observation for one present copy: the row's "version", or NULL
 * when the fixture says the observation was not PRESENT. */
static const char *a9_copy_version(const json_t *copy) {
    if (!copy || copy->type != JSON_OBJECT) return NULL;
    return a9_str(copy, "version");
}

json_t *w22_availabili_9d8d88_evaluate(const json_t *entry, const char *os_family,
                                       const char *gpu_class, const json_t *app) {
    const char *gpu;
    const char *minimum;
    const char *looked_at = NULL;
    const json_t *definition;
    int requires_app;

    (void)os_family;  /* the caller already resolved facts.os_family() */
    if (!entry || entry->type != JSON_OBJECT) entry = NULL;

    requires_app = a9_truthy(json_obj_get(entry, "requires_app"));
    gpu = a9_str(entry, "required_gpu");
    minimum = a9_str(entry, "min_version");
    definition = (requires_app && app && app->type == JSON_OBJECT) ? app : NULL;

    if (!requires_app && !gpu)
        return w22_availabili_9d8d88_as_dict("no_requirements", NULL, NULL, NULL);
    if (requires_app && !definition)
        return w22_availabili_9d8d88_as_dict("unsupported_os", NULL, NULL, minimum);
    /* `facts.gpu_class() not in (gpu, "unknown")`: an unreadable GPU passes. */
    if (gpu && (!gpu_class || (strcmp(gpu_class, "unknown") != 0 &&
                               strcmp(gpu_class, gpu) != 0)))
        return w22_availabili_9d8d88_as_dict("unsupported_gpu", NULL, NULL, minimum);
    if (!definition)
        return w22_availabili_9d8d88_as_dict("no_requirements", NULL, NULL, NULL);

    {
        json_t *command = json_obj_get(definition, "command");
        json_t *candidates = json_obj_get(definition, "candidates");
        looked_at = a9_list_str(command, 0);
        if (!looked_at) looked_at = a9_list_str(candidates, 0);
    }
    if (!a9_truthy(json_obj_get(definition, "found")))
        return w22_availabili_9d8d88_as_dict("missing_app", NULL, looked_at, minimum);

    {
        const char *kind = a9_str(definition, "version_kind");
        if (kind && !strcmp(kind, "none"))
            return w22_availabili_9d8d88_as_dict("available", NULL, looked_at, minimum);
    }

    {
        json_t *present = json_obj_get(definition, "present");
        size_t count = (present && present->type == JSON_ARRAY) ? json_len(present) : 0;
        size_t i;

        if (!minimum || !minimum[0]) {  /* `if not entry.min_version` */
            const char *first = count ? a9_copy_version(json_get(present, 0)) : NULL;
            return w22_availabili_9d8d88_as_dict("available", first, looked_at, NULL);
        }
        for (i = 0; i < count; i++) {
            json_t *copy = json_get(present, i);
            const char *path = a9_str(copy, "path");
            const char *version = a9_copy_version(copy);
            if (version && w22_availabili_9d8d88_version_at_least(version, minimum))
                return w22_availabili_9d8d88_as_dict("available", version, path, minimum);
        }
        /* Nothing qualifies: report the highest readable copy, first wins ties. */
        {
            long long best[A9_MAX_COMPONENTS];
            int best_n = 0;
            const char *best_path = NULL;
            const char *best_version = NULL;
            for (i = 0; i < count; i++) {
                json_t *copy = json_get(present, i);
                const char *path = a9_str(copy, "path");
                const char *version = a9_copy_version(copy);
                long long parts[A9_MAX_COMPONENTS];
                int n;
                if (!version) continue;
                n = a9_parts(version, parts, A9_MAX_COMPONENTS);
                if (n < 0) continue;
                if (!best_version || a9_cmp(parts, n, best, best_n) > 0) {
                    memcpy(best, parts, sizeof(long long) * (size_t)n);
                    best_n = n;
                    best_path = path;
                    best_version = version;
                }
            }
            if (best_version)
                return w22_availabili_9d8d88_as_dict("version_too_old", best_version,
                                                     best_path, minimum);
        }
        return w22_availabili_9d8d88_as_dict("version_too_old", NULL, looked_at, minimum);
    }
}
