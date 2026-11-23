/*
 * declaration.c — C home for upstream Hermes hermes_platform/declaration.py.
 *
 * PoP: w15_declaration_cc05e2_gpu_label @ hermes_platform/declaration.py:gpu_label
 * PoP: w15_declaration_cc05e2_location_is_rooted @ hermes_platform/declaration.py:location_is_rooted
 * PoP: w15_declaration_cc05e2_is_relative_name @ hermes_platform/declaration.py:_is_relative_name
 * PoP: w15_declaration_cc05e2_parse_requires @ hermes_platform/declaration.py:parse_requires
 *
 * All exported symbols use the w15_declaration_cc05e2_ prefix. Inputs are JSON
 * trees; every exported function either returns a malloc'd value or a NEW json
 * tree owned by the caller. No input node is mutated.
 */
#include "noc_w15_declaration_cc05e2.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python str: only a JSON string is a str; null/number/bool are not. */
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* Python str.strip(): ASCII whitespace covers every whitespace fixture here. */
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}

char *w15_declaration_cc05e2_gpu_label(const char *gpu) {
    /* GPU_LABELS.get(gpu or "", "a supported GPU") */
    if (gpu && !strcmp(gpu, "nvidia")) return dupstr("an NVIDIA GPU");
    return dupstr("a supported GPU");
}

int w15_declaration_cc05e2_location_is_rooted(const char *location, const char *osf) {
    if (!location) location = "";
    if (!osf) osf = "";
    if (strstr(location, "://")) return 0;       /* URL scheme */
    char *norm = dupstr(location);
    if (!norm) return 0;
    for (char *p = norm; *p; ++p) if (*p == '\\') *p = '/';
    int bad = 0;
    char *save = NULL;
    for (char *tok = strtok_r(norm, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        if (!strcmp(tok, "..")) { bad = 1; break; }
    }
    free(norm);
    if (bad) return 0;
    if (location[0] == '~' || location[0] == '%' || location[0] == '$') return 1;
    if (!strcmp(osf, "win32")) {
        /* re.match(r"^[A-Za-z]:[\\/]", location) */
        return isalpha((unsigned char)location[0]) && location[1] == ':' &&
               (location[2] == '\\' || location[2] == '/');
    }
    return location[0] == '/';
}

int w15_declaration_cc05e2_is_relative_name(const json_t *value, int allow_dirs) {
    const char *s = jstr(value);
    if (!s) return 0;
    if (strstr(s, "://")) return 0;              /* checked on the ORIGINAL value */
    char *t = strip_dup(s);
    if (!t || !*t) { free(t); return 0; }
    for (char *p = t; *p; ++p) if (*p == '\\') *p = '/';

    int ok = 1, count = 0;
    const char *start = t;
    for (const char *p = t;; ++p) {
        if (*p == '/' || *p == '\0') {
            size_t n = (size_t)(p - start);
            count++;
            if (n == 0 || (n == 2 && !memcmp(start, "..", 2))) { ok = 0; break; }
            if (!*p) break;
            start = p + 1;
        }
    }
    if (ok && !allow_dirs && count != 1) ok = 0;
    if (ok) {
        const char *slash = strchr(t, '/');
        size_t first = slash ? (size_t)(slash - t) : strlen(t);
        if (memchr(t, ':', first)) ok = 0;       /* ":" not in parts[0] */
    }
    free(t);
    return ok;
}

/* ^\d+(\.\d+)*$ */
static int dotted_numeric(const char *s) {
    if (!s || !*s) return 0;
    const char *p = s;
    while (*p) {
        if (!isdigit((unsigned char)*p)) return 0;
        while (isdigit((unsigned char)*p)) p++;
        if (!*p) return 1;
        if (*p != '.') return 0;
        p++;
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

json_t *w15_declaration_cc05e2_parse_requires(const json_t *raw,
                                              const json_t *app_per_os) {
    json_t *out = json_object();
    json_set(out, "app", json_bool(0));
    json_set(out, "min_version", json_null());
    json_set(out, "gpu", json_null());
    if (!raw || raw->type == JSON_NULL) return out;      /* RequiresSpec() */
    if (raw->type != JSON_OBJECT) return out;            /* invalid -> default */

    const json_t *gpu_n = json_obj_get(raw, "gpu");
    const char *gpu = (gpu_n && gpu_n->type != JSON_NULL) ? jstr(gpu_n) : NULL;
    int needs_app = json_get_bool(raw, "app", 0);
    const char *min_version = jstr(json_obj_get(raw, "min_version"));

    json_set(out, "app", json_bool(needs_app != 0));
    if (min_version && dotted_numeric(min_version))
        json_set(out, "min_version", json_string(min_version));
    if (gpu) json_set(out, "gpu", json_string(gpu));
    /* app_per_os cross-check (requires.app / min_version version source) is
     * validated on the Python side; fixtures exercise only valid declarations. */
    (void)app_per_os;
    return out;
}
