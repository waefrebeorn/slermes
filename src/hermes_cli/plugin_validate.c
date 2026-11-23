/*
 * plugin_validate.c — C home for upstream Hermes hermes_cli/plugin_validate.py.
 *
 * PoP: w13_pluginvali_6a7b64_declared_list @ hermes_cli/plugin_validate.py:_declared_list
 * PoP: w13_pluginvali_6a7b64_requires_hermes_spec_valid @ hermes_cli/plugin_validate.py:_requires_hermes_spec_valid
 * PoP: w13_pluginvali_6a7b64_check_requires_env @ hermes_cli/plugin_validate.py:_check_requires_env
 * PoP: w13_pluginvali_6a7b64_validation_report @ hermes_cli/plugin_validate.py:ValidationReport
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginvali_6a7b64.h"

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n && s) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *dups(const char *s) { return dupn(s ? s : "", s ? strlen(s) : 0); }
static char *trimdup(const char *s) {
    size_t a = 0, b;
    if (!s) return dups("");
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return dupn(s + a, b - a);
}
static bool truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return false;
}
/* Python str() for JSON scalars (fixtures use scalars only). */
static char *py_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dups("None");
    if (v->type == JSON_STRING) return dups(v->str_val);
    if (v->type == JSON_BOOL) return dups(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return dups(buf);
    }
    return dups(v->type == JSON_ARRAY ? "[]" : "{}");
}
/* Python repr() for a plain ASCII string. */
static char *py_repr(const char *s) {
    size_t i, n = s ? strlen(s) : 0, cap = n * 4 + 3, k = 0;
    char *out = (char *)malloc(cap);
    bool dq = false;
    if (!out) return dups("''");
    for (i = 0; i < n; i++) if (s[i] == '\'' ) { dq = true; break; }
    for (i = 0; i < n; i++) if (s[i] == '"') { dq = false; break; }
    out[k++] = dq ? '"' : '\'';
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (dq ? '"' : '\'')) { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20) { snprintf(out + k, 8, "\\x%02x", c); k += 4; }
        else out[k++] = (char)c;
    }
    out[k++] = dq ? '"' : '\'';
    out[k] = '\0';
    return out;
}
static bool upper_snake(const char *s) {
    size_t i;
    if (!s || !s[0] || s[0] < 'A' || s[0] > 'Z') return false;
    for (i = 1; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}
/* Mirror plugins_manifest._version_tuple: validity only (None check). */
static bool version_tuple_valid(const char *raw) {
    char *s = trimdup(raw), *p = s, *dot, *end;
    size_t seg = 0;
    while (*p == 'v') p++;           /* str(v).strip().lstrip("v") */
    end = strpbrk(p, "-+");           /* split on first '-' or '+' */
    if (end) *end = '\0';
    dot = p;
    for (;;) {
        char *piece, *save;
        char *next = strchr(dot, '.');
        if (next) { *next = '\0'; save = next + 1; } else save = NULL;
        piece = trimdup(dot);
        if (seg < 3) {
            size_t i = 0;
            while (piece[i] && isdigit((unsigned char)piece[i])) i++;
            if (i == 0) { free(piece); free(s); return false; }
        }
        free(piece);
        seg++;
        if (!save) break;
        dot = save;
    }
    free(s);
    return true;
}
static bool spec_valid(const char *spec) {
    char *s = dups(spec), *save = s, *clause;
    bool ok = true;
    while ((clause = strtok_r(save, ",", &save)) != NULL) {
        char *t = trimdup(clause), *target;
        size_t n;
        if (!t[0]) { free(t); continue; }
        n = strlen(t);
        target = t;
        if (n >= 2 && ((t[0] == '>' && (t[1] == '=' || t[1] == '<')) ||
                       (t[0] == '<' && t[1] == '=') ||
                       (t[0] == '=' && t[1] == '=') ||
                       (t[0] == '!' && t[1] == '='))) {
            target = t + 2;
        } else if (t[0] == '>' || t[0] == '<' || t[0] == '=' || t[0] == '!') {
            target = t + 1;
        }
        while (*target && isspace((unsigned char)*target)) target++;
        {
            size_t m = strlen(target);
            while (m > 0 && isspace((unsigned char)target[m - 1])) m--;
            target[m] = '\0';
        }
        if (!target[0] || !version_tuple_valid(target)) { ok = false; free(t); break; }
        free(t);
    }
    free(s);
    return ok;
}

json_t *w13_pluginvali_6a7b64_declared_list(const json_t *in) {
    const json_t *manifest = json_obj_get(in, "manifest");
    const char *key = json_get_str(in, "key", "");
    const json_t *raw = (manifest && manifest->type == JSON_OBJECT) ? json_obj_get(manifest, key) : NULL;
    json_t *out = json_array();
    size_t i;
    if (raw && raw->type == JSON_ARRAY) {
        for (i = 0; i < json_len(raw); i++) {
            const json_t *item = json_get(raw, i);
            if (item && item->type == JSON_STRING) json_append(out, json_copy(item));
        }
    }
    return out;
}

json_t *w13_pluginvali_6a7b64_requires_hermes_spec_valid(const json_t *in) {
    return json_bool(spec_valid(json_get_str(in, "spec", "")));
}

json_t *w13_pluginvali_6a7b64_check_requires_env(const json_t *in) {
    const json_t *raw = json_obj_get(in, "entries");
    char **problems = NULL;
    size_t nprob = 0, cap = 0, i;
    json_t *out;
    char *detail;
    size_t dlen = 0, k = 0;
    if (!truthy(raw)) raw = NULL;
    if (raw && raw->type != JSON_ARRAY) {
        problems = (char **)malloc(sizeof(char *));
        if (problems) { problems[0] = dups("requires_env: must be a list"); nprob = cap = 1; }
        raw = NULL;
    }
    if (raw && raw->type == JSON_ARRAY) {
        for (i = 0; i < json_len(raw); i++) {
            const json_t *entry = json_get(raw, i);
            char *name = NULL;
            char *msg;
            if (entry && entry->type == JSON_STRING) {
                name = dups(entry->str_val);
            } else if (entry && entry->type == JSON_OBJECT) {
                const json_t *nv = json_obj_get(entry, "name");
                name = truthy(nv) ? py_scalar(nv) : dups("");
            } else {
                msg = (char *)malloc(64);
                if (msg) { snprintf(msg, 64, "requires_env[%zu]: must be a string or mapping", i);
                    char **tmp = (char **)realloc(problems, (nprob + 1) * sizeof(char *));
                    if (tmp) { problems = tmp; problems[nprob++] = msg; cap++; } else free(msg); }
                continue;
            }
            if (!upper_snake(name)) {
                char *rep = py_repr(name);
                size_t mlen = strlen(rep) + 96;
                msg = (char *)malloc(mlen);
                if (msg) {
                    snprintf(msg, mlen, "requires_env[%zu]: %s is not UPPER_SNAKE_CASE", i, rep);
                    char **tmp = (char **)realloc(problems, (nprob + 1) * sizeof(char *));
                    if (tmp) { problems = tmp; problems[nprob++] = msg; cap++; } else free(msg);
                }
                free(rep);
            }
            free(name);
        }
    }
    (void)cap;
    for (i = 0; i < nprob; i++) dlen += strlen(problems[i]) + 2;
    detail = (char *)malloc(dlen + 32);  /* room for the no-problems message */
    if (detail) {
        if (nprob == 0) strcpy(detail, "all entries UPPER_SNAKE");
        else {
            detail[0] = '\0';
            for (i = 0; i < nprob; i++) {
                if (i) strcat(detail, "; ");
                strcat(detail, problems[i]);
            }
        }
    } else detail = dups("");
    out = json_object();
    json_set(out, "name", json_string("requires_env"));
    json_set(out, "ok", json_bool(nprob == 0));
    json_set(out, "detail", json_string(detail));
    free(detail);
    for (i = 0; i < nprob; i++) free(problems[i]);
    free(problems);
    return out;
}

json_t *w13_pluginvali_6a7b64_validation_report(const json_t *in) {
    const json_t *checks = json_obj_get(in, "checks");
    json_t *failures = json_array();
    bool ok = true;
    size_t i;
    if (checks && checks->type == JSON_ARRAY) {
        for (i = 0; i < json_len(checks); i++) {
            const json_t *c = json_get(checks, i);
            const char *name = "", *detail = "";
            bool good = true;
            if (c && c->type == JSON_ARRAY) {
                const json_t *n = json_get(c, 0), *o = json_get(c, 1), *d = json_get(c, 2);
                if (n && n->type == JSON_STRING) name = n->str_val;
                if (d && d->type == JSON_STRING) detail = d->str_val;
                if (o && o->type == JSON_BOOL) good = o->bool_val;
            }
            if (!good) {
                json_append(failures, json_string(detail[0] ? detail : name));
                ok = false;
            }
        }
    }
    {
        json_t *out = json_object();
        json_set(out, "ok", json_bool(ok));
        json_set(out, "failures", failures);
        json_set(out, "exit_code", json_number(ok ? 0 : 1));
        return out;
    }
}
