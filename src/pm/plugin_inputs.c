/*
 * plugin_inputs.c — faithful C11 port of upstream Hermes pm/plugin_inputs.py
 * (gate w6_plugininput_ee7719).
 *
 * PoP: w6_plugininput_decode    @ pm/plugin_inputs.py:decode
 * PoP: w6_plugininput_encode_ex @ pm/plugin_inputs.py:encode
 *
 * Only stdlib types are involved.  Path.absolute() is Windows-aware; an
 * explicit `cwd` seam is injected from the fixture so behaviour is
 * deterministic and does not mutate the process environment.
 */
#include "noc_w6_plugininput_ee7719.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str(Path(p)) on Windows: normalise '/' to '\\' (no cwd). */
static char *path_str(const char *p) {
    if (!p) p = "";
    char *s = strdup(p);
    for (char *q = s; *q; q++) if (*q == '/') *q = '\\';
    return s;
}

/* Python Path(p).absolute() with an injected cwd (Windows semantics). */
static char *path_abs(const char *p, const char *cwd) {
    if (!p) p = "";
    if (!cwd) cwd = "";
    size_t lp = strlen(p);
    char *out;

    /* UNC path (\\server\share...): already absolute, normalise separators. */
    if (lp >= 2 && (p[0] == '/' || p[0] == '\\') && (p[1] == '/' || p[1] == '\\')) {
        out = (char *)malloc(lp + 1);
        for (size_t i = 0; i < lp; i++) out[i] = (p[i] == '/') ? '\\' : p[i];
        out[lp] = '\0';
        return out;
    }
    /* Drive-absolute: X: followed by a separator. */
    if (lp >= 3 && isalpha((unsigned char)p[0]) && p[1] == ':' &&
        (p[2] == '/' || p[2] == '\\')) {
        out = (char *)malloc(lp + 1);
        for (size_t i = 0; i < lp; i++) out[i] = (p[i] == '/') ? '\\' : p[i];
        out[lp] = '\0';
        return out;
    }
    /* Root-relative: prepend the cwd's drive (e.g. /a -> C:\a). */
    if (lp >= 1 && (p[0] == '/' || p[0] == '\\')) {
        int have_drive = isalpha((unsigned char)cwd[0]) && cwd[1] == ':';
        size_t pre = have_drive ? 2 : 0;
        out = (char *)malloc(pre + lp + 1);
        if (have_drive) { out[0] = cwd[0]; out[1] = ':'; }
        for (size_t i = 0; i < lp; i++) out[pre + i] = (p[i] == '/') ? '\\' : p[i];
        out[pre + lp] = '\0';
        return out;
    }
    /* Empty or "." -> cwd itself. */
    if (lp == 0 || (lp == 1 && p[0] == '.')) return path_str(cwd);

    /* Relative: cwd + "\" + p. */
    size_t lc = strlen(cwd);
    out = (char *)malloc(lc + 1 + lp + 1);
    for (size_t i = 0; i < lc; i++) out[i] = (cwd[i] == '/') ? '\\' : cwd[i];
    size_t n = lc;
    if (n == 0 || out[n - 1] != '\\') out[n++] = '\\';
    for (size_t i = 0; i < lp; i++) out[n + i] = (p[i] == '/') ? '\\' : p[i];
    out[n + lp] = '\0';
    return out;
}

static json_t *json_str_owned(char *s) {
    json_t *v = json_string(s ? s : "");
    free(s);
    return v;
}

json_t *w6_plugininput_decode(const json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type != JSON_OBJECT) return json_null();

    const char *kind = json_get_str(value, "kind", "");
    if (!strcmp(kind, "members")) {
        json_t *sources = json_obj_get(value, "sources");
        if (sources && sources->type == JSON_ARRAY) {
            json_t *o = json_object();
            json_set(o, "type", json_string("members"));
            json_t *a = json_array();
            for (size_t i = 0; i < sources->c.count; i++) {
                json_t *pair = sources->c.items[i];
                char *ks, *vs;
                if (pair && pair->type == JSON_ARRAY && pair->c.count >= 2 &&
                    pair->c.items[0] && pair->c.items[1]) {
                    ks = path_str(pair->c.items[0]->type == JSON_STRING ? pair->c.items[0]->str_val : "");
                    vs = path_str(pair->c.items[1]->type == JSON_STRING ? pair->c.items[1]->str_val : "");
                } else {
                    ks = path_str(""); vs = path_str("");
                }
                json_t *pr = json_array();
                json_append(pr, json_str_owned(ks));
                json_append(pr, json_str_owned(vs));
                json_append(a, pr);
            }
            json_set(o, "sources", a);
            return o;
        }
        json_t *paths = json_obj_get(value, "paths");
        json_t *o = json_object();
        json_set(o, "type", json_string("members"));
        json_t *a = json_array();
        if (paths && paths->type == JSON_ARRAY)
            for (size_t i = 0; i < paths->c.count; i++)
                json_append(a, json_str_owned(path_str(
                    paths->c.items[i] && paths->c.items[i]->type == JSON_STRING
                    ? paths->c.items[i]->str_val : "")));
        json_set(o, "dirs", a);
        return o;
    }
    if (!strcmp(kind, "candidates")) {
        json_t *o = json_object();
        json_set(o, "type", json_string("candidates"));
        json_t *a = json_array();
        json_t *paths = json_obj_get(value, "paths");
        if (paths && paths->type == JSON_ARRAY)
            for (size_t i = 0; i < paths->c.count; i++)
                json_append(a, json_str_owned(path_str(
                    paths->c.items[i] && paths->c.items[i]->type == JSON_STRING
                    ? paths->c.items[i]->str_val : "")));
        json_set(o, "dirs", a);
        return o;
    }
    if (!strcmp(kind, "selection") || !strcmp(kind, "staged")) {
        json_t *o = json_object();
        json_set(o, "type", json_string(!strcmp(kind, "selection") ? "selection" : "staged"));
        json_t *data = json_obj_get(value, "data");
        json_set(o, "data", data ? json_copy(data) : json_object());
        return o;
    }
    /* Unknown kind -> ValueError text, matching Python str(exc). */
    char msg[512];
    snprintf(msg, sizeof(msg), "unknown plugin input kind: '%s'", kind);
    return json_string(msg);
}

json_t *w6_plugininput_encode_ex(const json_t *spec, const char *cwd) {
    if (!spec || spec->type == JSON_NULL) return json_null();
    if (spec->type != JSON_OBJECT) return json_null();
    const char *type = json_get_str(spec, "type", "");

    if (!strcmp(type, "members")) {
        json_t *sources = json_obj_get(spec, "sources");
        json_t *o = json_object();
        json_set(o, "kind", json_string("members"));
        if (sources && sources->type == JSON_ARRAY) {
            json_t *a = json_array();
            for (size_t i = 0; i < sources->c.count; i++) {
                json_t *pair = sources->c.items[i];
                char *k = path_abs(pair && pair->c.count >= 1 && pair->c.items[0] &&
                                   pair->c.items[0]->type == JSON_STRING
                                   ? pair->c.items[0]->str_val : "", cwd);
                char *v = path_abs(pair && pair->c.count >= 2 && pair->c.items[1] &&
                                   pair->c.items[1]->type == JSON_STRING
                                   ? pair->c.items[1]->str_val : "", cwd);
                json_t *pr = json_array();
                json_append(pr, json_str_owned(k));
                json_append(pr, json_str_owned(v));
                json_append(a, pr);
            }
            json_set(o, "sources", a);
        } else {
            json_t *a = json_array();
            json_t *dirs = json_obj_get(spec, "dirs");
            if (dirs && dirs->type == JSON_ARRAY)
                for (size_t i = 0; i < dirs->c.count; i++)
                    json_append(a, json_str_owned(path_abs(
                        dirs->c.items[i] && dirs->c.items[i]->type == JSON_STRING
                        ? dirs->c.items[i]->str_val : "", cwd)));
            json_set(o, "paths", a);
        }
        return o;
    }
    if (!strcmp(type, "candidates")) {
        json_t *o = json_object();
        json_set(o, "kind", json_string("candidates"));
        json_t *a = json_array();
        json_t *dirs = json_obj_get(spec, "dirs");
        if (dirs && dirs->type == JSON_ARRAY)
            for (size_t i = 0; i < dirs->c.count; i++)
                json_append(a, json_str_owned(path_abs(
                    dirs->c.items[i] && dirs->c.items[i]->type == JSON_STRING
                    ? dirs->c.items[i]->str_val : "", cwd)));
        json_set(o, "paths", a);
        return o;
    }
    if (!strcmp(type, "selection") || !strcmp(type, "staged")) {
        json_t *o = json_object();
        json_set(o, "kind", json_string(!strcmp(type, "selection") ? "selection" : "staged"));
        json_t *data = json_obj_get(spec, "data");
        json_set(o, "data", data ? json_copy(data) : json_object());
        return o;
    }
    return json_null();
}
