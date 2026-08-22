#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
/*
 * plugin_dev.c — faithful port of the deterministic decision surface of
 * hermes_cli/plugin_dev.py (runtime-backed validation behind
 * "hermes plugins doctor"). See plugin_dev.h for the PoP mapping.
 */
#include "plugin_dev.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

plugin_dev_dist_version_fn plugin_dev_dist_version_hook = NULL;

#define PD_SUPPORTED_MANIFEST_VERSION 2

/* VALID_HOOKS from hermes_cli/plugins.py (27 hooks). */
static const char *const PD_VALID_HOOKS[] = {
    "pre_tool_call", "post_tool_call", "transform_terminal_output",
    "transform_tool_result", "transform_llm_output", "pre_llm_call",
    "post_llm_call", "on_stream_start", "on_stream_delta", "on_stream_end",
    "on_interim_message", "pre_verify", "pre_api_request",
    "post_api_request", "api_request_error",
    "transform_api_error_classification", "on_session_start",
    "on_session_end", "on_session_finalize", "on_session_reset",
    "on_skill_lifecycle", "subagent_start", "subagent_stop",
    "pre_gateway_dispatch", "pre_approval_request", "post_approval_response",
    "pre_transcription",
};

bool plugin_dev_is_valid_hook(const char *name) {
    if (!name) return false;
    for (size_t i = 0; i < sizeof(PD_VALID_HOOKS) / sizeof(PD_VALID_HOOKS[0]); i++)
        if (strcmp(name, PD_VALID_HOOKS[i]) == 0) return true;
    return false;
}

/* _CONFIG_SCHEMA_TYPES keys. */
static const char *const PD_CONFIG_SCHEMA_TYPES[] = {
    "str", "string", "int", "integer", "float", "number",
    "bool", "boolean", "list", "array", "dict", "object",
};

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* ------------------------------------------------------------------ */
/* DoctorReport / DoctorFinding                                        */
/* ------------------------------------------------------------------ */
pd_report_t *pd_report_new(const char *path) {
    pd_report_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    const char *ppath = path ? path : ".";
    if (ppath[0] == '.' && ppath[1] == '/') ppath += 2; /* str(Path("./x")) */
    r->path = xstrdup(ppath);
    return r;
}

void pd_report_free(pd_report_t *r) {
    if (!r) return;
    free(r->path);
    free(r->manifest_name);
    free(r->manifest_version);
    free(r->manifest_kind);
    for (size_t i = 0; i < r->n_findings; i++) {
        free(r->findings[i].level);
        free(r->findings[i].message);
    }
    free(r->findings);
    for (size_t i = 0; i < r->n_registered_tools; i++) free(r->registered_tools[i]);
    free(r->registered_tools);
    for (size_t i = 0; i < r->n_registered_hooks; i++) free(r->registered_hooks[i]);
    free(r->registered_hooks);
    free(r);
}

static void pd_report_add(pd_report_t *r, const char *level, const char *msg) {
    r->findings = realloc(r->findings, (r->n_findings + 1) * sizeof(pd_finding_t));
    r->findings[r->n_findings].level = xstrdup(level);
    r->findings[r->n_findings].message = xstrdup(msg);
    r->n_findings++;
}

void pd_report_error(pd_report_t *r, const char *msg) {
    pd_report_add(r, "error", msg);
}

void pd_report_warning(pd_report_t *r, const char *msg) {
    pd_report_add(r, "warning", msg);
}

bool pd_report_ok(const pd_report_t *r) {
    for (size_t i = 0; i < r->n_findings; i++)
        if (strcmp(r->findings[i].level, "error") == 0) return false;
    return true;
}

char *plugin_dev_format_text(const pd_report_t *r) {
    size_t cap = 256;
    for (size_t i = 0; i < r->n_findings; i++) cap += strlen(r->findings[i].message) + 32;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t off = 0;
#define PD_APPEND(...) do { \
        int wr = snprintf(out + off, cap - off, __VA_ARGS__); \
        if (wr > 0 && (size_t)wr < cap - off) off += (size_t)wr; \
    } while (0)
    PD_APPEND("Plugin Doctor: %s", r->path ? r->path : "");
    if (r->has_manifest) {
        PD_APPEND("\n  manifest: %s %s (%s)", r->manifest_name ? r->manifest_name : "",
                  (r->manifest_version && r->manifest_version[0])
                      ? r->manifest_version : "(no version)",
                  r->manifest_kind ? r->manifest_kind : "");
    }
    for (size_t i = 0; i < r->n_findings; i++) {
        PD_APPEND("\n  %s: %s",
                  strcmp(r->findings[i].level, "error") == 0 ? "ERROR" : "WARN",
                  r->findings[i].message);
    }
    if (pd_report_ok(r)) {
        PD_APPEND("\n  OK: runtime discovery, manifest parsing, import, and registration passed");
    }
    PD_APPEND("\n  registrations: %zu tool(s), %zu hook(s)",
              r->n_registered_tools, r->n_registered_hooks);
#undef PD_APPEND
    return out;
}

/* ------------------------------------------------------------------ */
/* resolve_plugin_path                                                 */
/* ------------------------------------------------------------------ */
static bool is_dir(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static char *resolve_real(const char *p) {
    char buf[4096];
    if (!realpath(p, buf)) return strdup(p); /* best effort like Path.resolve */
    return strdup(buf);
}

static char *expanduser_path(const char *raw, const char *cwd) {
    if (raw && raw[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            size_t n = snprintf(NULL, 0, "%s%s", home, raw + 1);
            char *s = malloc(n + 1);
            snprintf(s, n + 1, "%s%s", home, raw + 1);
            return s;
        }
    }
    if (raw && raw[0] != '/') {
        size_t n = snprintf(NULL, 0, "%s/%s", cwd ? cwd : ".", raw);
        char *s = malloc(n + 1);
        snprintf(s, n + 1, "%s/%s", cwd ? cwd : ".", raw);
        return s;
    }
    return strdup(raw ? raw : ".");
}

char *plugin_dev_resolve_plugin_path(const char *target,
                                     const char *hermes_home,
                                     const char *bundled_dir,
                                     const char *cwd,
                                     char **out_error) {
    *out_error = NULL;
    const char *raw = (target && target[0]) ? target : ".";
    char *direct = expanduser_path(raw, cwd);
    if (is_dir(direct)) {
        char *resolved = resolve_real(direct);
        free(direct);
        return resolved;
    }
    free(direct);

    char *candidates[5];
    size_t nc = 0;
    {
        size_t n = snprintf(NULL, 0, "%s/plugins/%s", hermes_home ? hermes_home : "", raw);
        candidates[nc] = malloc(n + 1);
        snprintf(candidates[nc], n + 1, "%s/plugins/%s", hermes_home ? hermes_home : "", raw);
        nc++;
    }
    if (bundled_dir) {
        const char *subs[] = { "/", "/platforms/", "/model-providers/" };
        for (size_t i = 0; i < 3; i++) {
            size_t n = snprintf(NULL, 0, "%s%s%s", bundled_dir, subs[i], raw);
            candidates[nc] = malloc(n + 1);
            snprintf(candidates[nc], n + 1, "%s%s%s", bundled_dir, subs[i], raw);
            nc++;
        }
    }
    {
        size_t n = snprintf(NULL, 0, "%s/.hermes/plugins/%s", cwd ? cwd : ".", raw);
        candidates[nc] = malloc(n + 1);
        snprintf(candidates[nc], n + 1, "%s/.hermes/plugins/%s", cwd ? cwd : ".", raw);
        nc++;
    }
    for (size_t i = 0; i < nc; i++) {
        if (is_dir(candidates[i])) {
            char *resolved = resolve_real(candidates[i]);
            for (size_t k = 0; k < nc; k++) free(candidates[k]);
            return resolved;
        }
    }
    size_t n = snprintf(NULL, 0,
                        "Plugin '%s' was not found as a path or installed plugin id",
                        raw);
    *out_error = malloc(n + 1);
    snprintf(*out_error, n + 1,
             "Plugin '%s' was not found as a path or installed plugin id", raw);
    for (size_t k = 0; k < nc; k++) free(candidates[k]);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Python-repr fidelity helpers                                        */
/* ------------------------------------------------------------------ */
static void repr_append(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t l = strlen(s);
    while (*len + l + 1 > *cap) { *cap = *cap ? *cap * 2 : 256; *buf = realloc(*buf, *cap); }
    memcpy(*buf + *len, s, l + 1);
    *len += l;
}

static void repr_json(char **buf, size_t *len, size_t *cap, const json_t *v);

static void repr_string(char **buf, size_t *len, size_t *cap, const char *s) {
    repr_append(buf, len, cap, "'");
    for (const char *p = s; *p; p++) {
        if (*p == '\'' || *p == '\\') {
            char esc[3] = { '\\', *p, '\0' };
            repr_append(buf, len, cap, esc);
        } else {
            char c[2] = { *p, '\0' };
            repr_append(buf, len, cap, c);
        }
    }
    repr_append(buf, len, cap, "'");
}

static void repr_json(char **buf, size_t *len, size_t *cap, const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) { repr_append(buf, len, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING: repr_string(buf, len, cap, v->str_val ? v->str_val : ""); break;
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (long long)d)
            snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
        else
            snprintf(tmp, sizeof(tmp), "%g", d);
        repr_append(buf, len, cap, tmp);
        break;
    }
    case JSON_BOOL: repr_append(buf, len, cap, v->bool_val ? "True" : "False"); break;
    case JSON_ARRAY: {
        repr_append(buf, len, cap, "[");
        for (size_t i = 0; i < json_len(v); i++) {
            if (i) repr_append(buf, len, cap, ", ");
            repr_json(buf, len, cap, json_get(v, i));
        }
        repr_append(buf, len, cap, "]");
        break;
    }
    case JSON_OBJECT: {
        repr_append(buf, len, cap, "{");
        size_t n = json_object_size(v);
        for (size_t i = 0; i < n; i++) {
            if (i) repr_append(buf, len, cap, ", ");
            repr_string(buf, len, cap, json_object_get_key_at(v, i));
            repr_append(buf, len, cap, ": ");
            repr_json(buf, len, cap, json_object_get_at(v, i));
        }
        repr_append(buf, len, cap, "}");
        break;
    }
    default: break;
    }
}

static char *json_repr(const json_t *v) {
    char *buf = NULL;
    size_t len = 0, cap = 0;
    repr_json(&buf, &len, &cap, v);
    if (!buf) buf = strdup("");
    return buf;
}

static char *json_repr_str(const char *s) {
    json_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.type = JSON_STRING;
    tmp.str_val = (char *)s;
    return json_repr(&tmp);
}

/* ------------------------------------------------------------------ */
/* _check_manifest_v2                                                  */
/* ------------------------------------------------------------------ */
static char *msg_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char stackbuf[1024];
    vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap);
    va_end(ap);
    return strdup(stackbuf);
}

void plugin_dev_check_manifest_v2(pd_report_t *report, const json_t *manifest) {
    /* mv = getattr(manifest, "manifest_version", 1) */
    long mv = 1;
    const json_t *mvv = json_obj_get((json_t *)manifest, "manifest_version");
    if (mvv && mvv->type == JSON_NUMBER) mv = (long)mvv->num_val;
    if (mv > PD_SUPPORTED_MANIFEST_VERSION) {
        char *m = msg_printf(
            "manifest_version %ld is newer than this Hermes supports (%d); unknown fields are ignored",
            mv, PD_SUPPORTED_MANIFEST_VERSION);
        pd_report_warning(report, m);
        free(m);
    }

    const json_t *avv = json_obj_get((json_t *)manifest, "api_version");
    if (avv && avv->type == JSON_NUMBER && avv->num_val < 1) {
        char *m = msg_printf("api_version %ld is not a valid API generation (>= 1)",
                             (long)avv->num_val);
        pd_report_warning(report, m);
        free(m);
    }

    const json_t *rp = json_obj_get((json_t *)manifest, "requires_plugins");
    if (rp && rp->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rp); i++) {
            const json_t *dep = json_get(rp, i);
            if (!dep) continue;
            if (dep->type != JSON_OBJECT) {
                char *repr = json_repr(dep);
                char *m = msg_printf("requires_plugins entry %s has no plugin id", repr);
                pd_report_warning(report, m);
                free(m); free(repr);
                continue;
            }
            const json_t *idv = json_obj_get((json_t *)dep, "id");
            if (!idv || idv->type != JSON_STRING || !idv->str_val || !idv->str_val[0]) {
                char *repr = json_repr(dep);
                char *m = msg_printf("requires_plugins entry %s has no plugin id", repr);
                pd_report_warning(report, m);
                free(m); free(repr);
                continue;
            }
            const json_t *vrv = json_obj_get((json_t *)dep, "version_range");
            if (vrv && ((vrv->type == JSON_STRING && vrv->str_val && vrv->str_val[0]) ||
                        (vrv->type == JSON_NUMBER))) {
                const char *vr = (vrv->type == JSON_STRING) ? vrv->str_val : "";
                char numbuf[32] = "";
                if (vrv->type == JSON_NUMBER)
                    snprintf(numbuf, sizeof(numbuf), "%g", vrv->num_val);
                if (vrv->type == JSON_NUMBER) vr = numbuf;
                char *repr = json_repr(idv);
                char *m = msg_printf(
                    "requires plugin %s (%s) — version ranges are advisory; "
                    "a missing dependency logs a warning at load", repr, vr);
                pd_report_warning(report, m);
                free(m); free(repr);
            }
        }
    }

    const json_t *pydeps = json_obj_get((json_t *)manifest, "python_dependencies");
    char **missing = NULL, **unpinned = NULL;
    size_t n_missing = 0, n_unpinned = 0;
    if (pydeps && pydeps->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pydeps); i++) {
            const json_t *reqv = json_get(pydeps, i);
            if (!reqv || reqv->type != JSON_STRING) continue;
            const char *req = reqv->str_val;
            /* dist = re.split(r"[<>=!~\[\;\s]", req, maxsplit=1)[0].strip() */
            char dist[256];
            size_t dl = 0;
            for (const char *p = req; *p && dl < sizeof(dist) - 1; p++) {
                if (*p == '<' || *p == '>' || *p == '=' || *p == '!' ||
                    *p == '~' || *p == '[' || *p == ']' || *p == ';' ||
                    isspace((unsigned char)*p))
                    break;
                dist[dl++] = *p;
            }
            dist[dl] = '\0';
            /* .strip() */
            char *ds = dist;
            char *de = dist + dl;
            while (de > ds && isspace((unsigned char)de[-1])) de--;
            *de = '\0';
            while (*ds && isspace((unsigned char)*ds)) ds++;
            /* unpinned iff no '<', '==', or '~=' in req */
            bool has_lt = strchr(req, '<') != NULL;
            const char *eq = strstr(req, "==");
            bool has_tilde = strstr(req, "~=") != NULL;
            if (!has_lt && !eq && !has_tilde) {
                unpinned = realloc(unpinned, (n_unpinned + 1) * sizeof(char *));
                unpinned[n_unpinned++] = strdup(req);
            }
            if (!*ds) continue;
            int rc;
            if (plugin_dev_dist_version_hook) {
                char *ver = NULL;
                rc = plugin_dev_dist_version_hook(ds, &ver);
                free(ver);
            } else {
                rc = -1; /* no runtime: skip like a non-finding exception */
            }
            if (rc == 0) {
                missing = realloc(missing, (n_missing + 1) * sizeof(char *));
                missing[n_missing++] = strdup(req);
            }
        }
    }
    for (size_t i = 0; i < n_unpinned; i++) {
        char *repr = json_repr_str(unpinned[i]);
        char *m = msg_printf(
            "python_dependencies entry %s has no upper bound — pin an upper "
            "bound (e.g. 'pkg>=1.0,<2') per the dependency policy", repr);
        pd_report_warning(report, m);
        free(m); free(repr);
    }
    if (n_missing > 0) {
        size_t len = 200;
        for (size_t i = 0; i < n_missing; i++) len += strlen(missing[i]) * 3 + 8;
        char *m = malloc(len + 1);
        size_t off = (size_t)snprintf(m, len,
            "declared python_dependencies not installed: ");
        for (size_t i = 0; i < n_missing; i++)
            off += (size_t)snprintf(m + off, len - off, "%s%s",
                                    i ? ", " : "", missing[i]);
        off += (size_t)snprintf(m + off, len - off,
            " — Hermes never auto-installs plugin dependencies; "
            "install manually: pip install ");
        for (size_t i = 0; i < n_missing; i++)
            off += (size_t)snprintf(m + off, len - off, "%s'%s'",
                                    i ? " " : "", missing[i]);
        pd_report_warning(report, m);
        free(m);
    }
    for (size_t i = 0; i < n_missing; i++) free(missing[i]);
    free(missing);
    for (size_t i = 0; i < n_unpinned; i++) free(unpinned[i]);
    free(unpinned);

    const json_t *schema = json_obj_get((json_t *)manifest, "config_schema");
    if (schema && schema->type == JSON_OBJECT) {
        size_t n = json_object_size(schema);
        for (size_t i = 0; i < n; i++) {
            const char *skey = json_object_get_key_at(schema, i);
            const json_t *spec = json_object_get_at(schema, i);
            if (!spec || spec->type != JSON_OBJECT) continue;
            const json_t *stypev = json_obj_get((json_t *)spec, "type");
            if (!stypev || stypev->type == JSON_NULL) continue;
            const char *stype = (stypev->type == JSON_STRING && stypev->str_val)
                                    ? stypev->str_val : "";
            char low[64];
            size_t ll = 0;
            for (const char *p = stype; *p && ll < sizeof(low) - 1; p++)
                low[ll++] = (char)tolower((unsigned char)*p);
            low[ll] = '\0';
            bool known = false;
            for (size_t k = 0;
                 k < sizeof(PD_CONFIG_SCHEMA_TYPES) / sizeof(PD_CONFIG_SCHEMA_TYPES[0]);
                 k++)
                if (strcmp(low, PD_CONFIG_SCHEMA_TYPES[k]) == 0) known = true;
            if (!known) {
                char *skey_repr = json_repr_str(skey);
                char *stype_repr = json_repr(stypev);
                char *m = msg_printf(
                    "config_schema key %s declares unknown type %s",
                    skey_repr, stype_repr);
                pd_report_warning(report, m);
                free(m); free(skey_repr); free(stype_repr);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Registration diff ladders                                           */
/* ------------------------------------------------------------------ */
static int cmp_strp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool str_in(char *const *arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(arr[i], s) == 0) return true;
    return false;
}

static void diff_ladder(pd_report_t *report,
                        char *const *declared, size_t nd,
                        char *const *registered, size_t nr,
                        const char *noun) {
    size_t n = nd + nr;
    char **work = malloc((n ? n : 1) * sizeof(char *));
    size_t k = 0;
    /* declared hooks must be valid names */
    for (size_t i = 0; i < nd; i++)
        if (strcmp(noun, "hook") == 0 && !plugin_dev_is_valid_hook(declared[i])) {
            char *repr = json_repr_str(declared[i]);
            char *m = msg_printf("unknown hook %s in provides_hooks", repr);
            pd_report_error(report, m);
            free(m); free(repr);
        }
    for (size_t i = 0; i < nd; i++)
        if (!str_in(registered, nr, declared[i])) work[k++] = (char *)declared[i];
    qsort(work, k, sizeof(char *), cmp_strp);
    for (size_t i = 0; i < k; i++) {
        char *repr = json_repr_str(work[i]);
        char *m = msg_printf("manifest declares %s %s but registration did not add it",
                             noun, repr);
        pd_report_warning(report, m);
        free(m); free(repr);
    }
    k = 0;
    for (size_t i = 0; i < nr; i++)
        if (!str_in(declared, nd, registered[i])) work[k++] = (char *)registered[i];
    qsort(work, k, sizeof(char *), cmp_strp);
    for (size_t i = 0; i < k; i++) {
        char *repr = json_repr_str(work[i]);
        char *m = msg_printf("registration adds %s %s not listed in provides_%ss",
                             noun, repr, noun);
        pd_report_warning(report, m);
        free(m); free(repr);
    }
    free(work);
}

void plugin_dev_check_registrations(pd_report_t *report,
                                    char *const *declared_hooks, size_t nh_d,
                                    char *const *registered_hooks, size_t nh_r,
                                    char *const *declared_tools, size_t nt_d,
                                    char *const *registered_tools, size_t nt_r) {
    diff_ladder(report, declared_hooks, nh_d, registered_hooks, nh_r, "hook");
    diff_ladder(report, declared_tools, nt_d, registered_tools, nt_r, "tool");
}
