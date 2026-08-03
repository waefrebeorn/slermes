/*
 * port_web_server_cron_dash.c — dashboard cron adapter layer.
 * Faithful port of _normalize_dashboard_cron_script,
 * _validate_dashboard_cron_effective_job, _normalize_dashboard_cron_updates,
 * _cron_default_profile, _cron_profile_home, and _annotate_cron_job from
 * hermes_cli/web_server.py.
 *
 * Reuses: web_cron_optional_text / web_cron_string_list
 * (hermes_web_server_pure.h) and the profiles port (port_cli_profiles.c).
 */

#include "web_server_cron_dash.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

#include "hermes_json.h"
#include "hermes_web_server_pure.h"

/* From port_cli_profiles.c */
extern char *profile_normalize_name(const char *name);
extern bool profile_validate_name(const char *name, char **err);
extern int profile_dir_exists(const char *name);
extern char *profile_dir_for(const char *name);
extern char *profile_get_active_name(void);

static void set_out(int *status, char **detail, int st, const char *msg) {
    if (status) *status = st;
    if (detail) *detail = strdup(msg);
}

/* Python str(value) for scalars the dashboard sends (str/num/bool/null). */
static char *value_to_text(const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return strdup(v->str_val);
    char buf[64];
    if (v->type == JSON_BOOL)
        return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    }
    return NULL;
}

/* _cron_optional_text(value) over a json value. */
static char *opt_text(const json_t *v, bool strip_trailing_slash) {
    char *raw = value_to_text(v);
    if (!raw) return NULL;
    char *out = web_cron_optional_text(raw, strip_trailing_slash);
    free(raw);
    return out;
}

/* _cron_string_list(value) over a json value → json array or NULL. */
/* PoP: string_list @ hermes_cli.web_server.py:_string_list */
static json_t *string_list(const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    json_t *items = json_array();
    if (v->type == JSON_STRING) {
        char **arr = web_cron_string_list(v->str_val);
        if (arr) {
            for (int i = 0; arr[i]; i++) {
                json_append(items, json_string(arr[i]));
                free(arr[i]);
            }
            free(arr);
        }
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len((json_t *)v); i++) {
            char *t = value_to_text(json_get((json_t *)v, i));
            if (!t) continue;
            char *stripped = web_cron_optional_text(t, false);
            free(t);
            if (stripped) {
                json_append(items, json_string(stripped));
                free(stripped);
            }
        }
    } else {
        json_free(items);
        return NULL;
    }
    if (json_len(items) == 0) { json_free(items); return NULL; }
    return items;
}

/* realpath that tolerates non-existent leaves: resolves the parent and
 * re-appends the leaf (Python Path.resolve(strict=False) semantics for the
 * single-missing-leaf case the sandbox check needs). When realpath fails
 * entirely (e.g. a missing MIDDLE component like scripts/../escape.sh),
 * collapse ".."/"." segments lexically exactly like pathlib does, so a
 * "../escape.sh" escape is still detected. */
static char *resolve_lenient(const char *path) {
    /* Lexically collapse ".", "..", and empty segments across the WHOLE
     * path first (pathlib.resolve(strict=False) semantics): a path like
     * "<parent>/scripts/../escape.sh" must reduce to "<parent>/escape.sh"
     * even when "scripts" does not exist, so the sandbox containment check
     * still detects "../" escapes. */
    {
        size_t plen = strlen(path);
        char *nb = malloc(plen + 1);
        if (!nb) return strdup(path);
        size_t wi = 0;
        size_t base = 0;
        if (path[0] == '/') { nb[wi++] = '/'; base = 1; }
        for (size_t ri = base; ri <= plen; ) {
            const char *seg = path + ri;
            size_t sl = strcspn(seg, "/");
            if (sl == 0) { ri++; continue; }
            if (sl == 1 && seg[0] == '.') { ri += 1; continue; }
            if (sl == 2 && seg[0] == '.' && seg[1] == '.') {
                /* pop last written segment (never below root) */
                while (wi > 1 && nb[wi - 1] != '/') wi--;
                if (wi > 1) wi--;  /* drop the slash too */
                ri += 2;
                continue;
            }
            if (wi > 1 && nb[wi - 1] != '/') nb[wi++] = '/';
            memcpy(nb + wi, seg, sl);
            wi += sl;
            ri += sl;
        }
        nb[wi] = '\0';
        /* Realpath the normalized path; if the whole thing resolves (all
         * components exist), use it verbatim. Otherwise resolve the longest
         * existing parent and re-append the remainder — the single-missing-
         * leaf case. If even the parent cannot be resolved (e.g. the fixture
         * sandbox home does not exist), return the lexically-normalized
         * absolute path — pathlib.resolve(strict=False) semantics, which
         * ALWAYS yields an absolute normalized path regardless of existence. */
        char *rp = realpath(nb, NULL);
        if (rp) { free(nb); return rp; }
        char *dup = strdup(nb);
        free(nb);
        char *slash = strrchr(dup, '/');
        if (!slash || slash == dup) { free(dup); return strdup(path); }
        *slash = '\0';
        char *parent = realpath(dup, NULL);
        if (!parent) {
            /* Nothing above the leaf resolves: the normalized absolute path
             * is still the correct answer (Python would return exactly it). */
            size_t need = strlen(dup) + 1 + strlen(slash + 1) + 1;
            char *out = malloc(need);
            if (out) snprintf(out, need, "%s/%s", dup, slash + 1);
            free(dup);
            return out ? out : strdup(path);
        }
        size_t need = strlen(parent) + 1 + strlen(slash + 1) + 1;
        char *out = malloc(need);
        snprintf(out, need, "%s/%s", parent, slash + 1);
        free(parent);
        free(dup);
        return out;
    }
}

/* ── _normalize_dashboard_cron_script ───────────────────────────────────── */
/* PoP: ws_cron_normalize_script @ hermes_cli/web_server.py:_normalize_dashboard_cron_script */
char *ws_cron_normalize_script(const char *value, const char *profile_home,
                               int *status, char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    char *text = value ? web_cron_optional_text(value, false) : NULL;
    if (!text) return NULL;

    char scripts_raw[PATH_MAX];
    snprintf(scripts_raw, sizeof(scripts_raw), "%s/scripts", profile_home);
    char *scripts_root = resolve_lenient(scripts_raw);

    /* Path(text).expanduser() */
    char *expanded = strdup(text);
    if (text[0] == '~' && (text[1] == '/' || text[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) {
            size_t need = strlen(home) + strlen(text) + 1;
            free(expanded);
            expanded = malloc(need);
            snprintf(expanded, need, "%s%s", home, text + 1);
        }
    }

    char *candidate;
    if (expanded[0] == '/') {
        candidate = resolve_lenient(expanded);
    } else {
        char joined[PATH_MAX];
        snprintf(joined, sizeof(joined), "%s/%s", scripts_root, expanded);
        candidate = resolve_lenient(joined);
    }
    free(expanded);
    free(text);

    /* candidate.relative_to(scripts_root) */
    size_t rl = strlen(scripts_root);
    bool inside = strncmp(candidate, scripts_root, rl) == 0 &&
                  (candidate[rl] == '/' || candidate[rl] == '\0');
    if (!inside) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "script must be inside %s", scripts_root);
        set_out(status, detail, 400, msg);
        free(scripts_root);
        free(candidate);
        return NULL;
    }
    struct stat st;
    if (stat(candidate, &st) != 0) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "script does not exist: %s", candidate);
        set_out(status, detail, 400, msg);
        free(scripts_root);
        free(candidate);
        return NULL;
    }
    if (!S_ISREG(st.st_mode)) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "script is not a file: %s", candidate);
        set_out(status, detail, 400, msg);
        free(scripts_root);
        free(candidate);
        return NULL;
    }
    /* str(relative): strip root + '/'; equal paths → "." (Python yields
     * Path(".") for identical paths, but a scripts_root dir fails is_file
     * first, so unreachable in practice). */
    char *rel = strdup(candidate[rl] == '/' ? candidate + rl + 1 : ".");
    free(scripts_root);
    free(candidate);
    return rel;
}

/* ── _validate_dashboard_cron_effective_job ─────────────────────────────── */
/* PoP: ws_cron_validate_effective_job @ hermes_cli/web_server.py:_validate_dashboard_cron_effective_job */
bool ws_cron_validate_effective_job(const json_t *job, int *status,
                                    char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    char *prompt = opt_text(json_object_get((json_t *)job, "prompt"), false);
    char *script = opt_text(json_object_get((json_t *)job, "script"), false);
    json_t *skills = string_list(json_object_get((json_t *)job, "skills"));
    if (!skills)
        skills = string_list(json_object_get((json_t *)job, "skill"));
    json_t *na = json_object_get((json_t *)job, "no_agent");
    bool no_agent = na &&
        ((na->type == JSON_BOOL && na->bool_val) ||
         (na->type == JSON_NUMBER && na->num_val != 0.0) ||
         (na->type == JSON_STRING && na->str_val[0] != '\0') ||
         na->type == JSON_ARRAY || na->type == JSON_OBJECT);
    /* Python truthiness for containers: non-empty only. */
    if (na && na->type == JSON_ARRAY && json_len(na) == 0) no_agent = false;
    if (na && na->type == JSON_OBJECT && json_object_size(na) == 0) no_agent = false;

    bool ok = true;
    if (no_agent) {
        if (!script) {
            set_out(status, detail, 400, "no_agent=True requires a script");
            ok = false;
        }
    } else if (!prompt && !skills && !script) {
        set_out(status, detail, 400,
                "agent cron jobs require a prompt, skill, or script");
        ok = false;
    }
    free(prompt);
    free(script);
    if (skills) json_free(skills);
    return ok;
}

/* ── _normalize_dashboard_cron_updates ──────────────────────────────────── */
/* PoP: ws_cron_normalize_updates @ hermes_cli/web_server.py:_normalize_dashboard_cron_updates */
json_t *ws_cron_normalize_updates(const json_t *updates,
                                  const char *profile_home, int *status,
                                  char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    json_t *norm = updates ? json_copy((json_t *)updates) : json_object();

    static const char *text_keys[] = {"model", "provider", "workdir", NULL};
    for (size_t i = 0; text_keys[i]; i++) {
        json_t *v = json_object_get(norm, text_keys[i]);
        if (v) {
            char *t = opt_text(v, false);
            json_set(norm, text_keys[i], t ? json_string(t) : json_null());
            free(t);
        }
    }
    json_t *script_v = json_object_get(norm, "script");
    if (script_v) {
        char *raw = value_to_text(script_v);
        char *rel = ws_cron_normalize_script(raw ? raw : "", profile_home,
                                             status, detail);
        free(raw);
        if (!rel && status && *status != 0) { json_free(norm); return NULL; }
        json_set(norm, "script", rel ? json_string(rel) : json_null());
        free(rel);
    }
    json_t *burl = json_object_get(norm, "base_url");
    if (burl) {
        char *t = opt_text(burl, true);
        json_set(norm, "base_url", t ? json_string(t) : json_null());
        free(t);
    }
    json_t *deliver = json_object_get(norm, "deliver");
    if (deliver) {
        char *t = opt_text(deliver, false);
        json_set(norm, "deliver", json_string(t ? t : "local"));
        free(t);
    }
    json_t *cf = json_object_get(norm, "context_from");
    if (cf) {
        json_t *lst = string_list(cf);
        json_set(norm, "context_from", lst ? lst : json_null());
    }
    json_t *ets = json_object_get(norm, "enabled_toolsets");
    if (ets) {
        json_t *lst = string_list(ets);
        json_set(norm, "enabled_toolsets", lst ? lst : json_null());
    }
    return norm;
}

/* ── _cron_default_profile ──────────────────────────────────────────────── */
/* PoP: ws_cron_default_profile @ hermes_cli/web_server.py:_cron_default_profile */
char *ws_cron_default_profile(void) {
    char *name = profile_get_active_name();
    if (!name) return strdup("default");
    if (strcmp(name, "default") == 0 || strcmp(name, "custom") == 0) {
        free(name);
        return strdup("default");
    }
    return name;
}

/* ── _cron_profile_home ─────────────────────────────────────────────────── */
/* PoP: ws_cron_profile_home @ hermes_cli/web_server.py:_cron_profile_home */
bool ws_cron_profile_home(const char *profile, char **name_out,
                          char **home_out, int *status, char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    /* raw = (profile or _cron_default_profile()).strip() or "default" */
    char *raw;
    if (profile && *profile) raw = strdup(profile);
    else raw = ws_cron_default_profile();
    char *stripped = web_cron_optional_text(raw, false);
    free(raw);
    if (!stripped) stripped = strdup("default");

    char *canon = profile_normalize_name(stripped);
    free(stripped);
    char *err = NULL;
    if (!profile_validate_name(canon, &err)) {
        if (status) *status = 400;
        if (detail) *detail = err ? err : strdup("invalid profile name");
        else free(err);
        free(canon);
        return false;
    }
    free(err);
    if (!profile_dir_exists(canon)) {
        if (status) *status = 404;
        if (detail) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Profile '%s' does not exist.", canon);
            *detail = strdup(msg);
        }
        free(canon);
        return false;
    }
    if (home_out) *home_out = profile_dir_for(canon);
    if (name_out) *name_out = canon;
    else free(canon);
    return true;
}

/* ── _annotate_cron_job ─────────────────────────────────────────────────── */
/* PoP: ws_cron_annotate_job @ hermes_cli/web_server.py:_annotate_cron_job */
json_t *ws_cron_annotate_job(const json_t *job, const char *profile,
                             const char *home) {
    json_t *annotated = job ? json_copy((json_t *)job) : json_object();
    json_set(annotated, "profile", json_string(profile));
    json_set(annotated, "profile_name", json_string(profile));
    json_set(annotated, "hermes_home", json_string(home));
    json_set(annotated, "is_default_profile",
             json_bool(strcmp(profile, "default") == 0));
    return annotated;
}
