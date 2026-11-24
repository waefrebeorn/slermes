/*
 * core.c - C11 port of upstream hermes_platform/resolver/core.py
 * (gate w23_core_082b31).
 *
 * PoP: w23_core_082b31_effective_path @ hermes_platform/resolver/core.py:LookupContext.effective_path
 * PoP: w23_core_082b31_resolution @ hermes_platform/resolver/core.py:Resolution
 * PoP: w23_core_082b31_locate @ hermes_platform/resolver/core.py:locate_command
 *
 * probe_candidates() is locate_command's ladder: an explicit path is trusted
 * only when it is absolute and executable, otherwise PATH is probed first and
 * every known dir after it, and one Candidate is recorded per probe in order.
 * Everything ambient (os.name/sep/altsep/pathsep, the env mapping, shutil.which
 * and the file/executable answers) arrives in the request; this file decides
 * only the ladder and the derived views.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_core_082b31.h"

#define CORE_MAX_CANDIDATES 32
#define CORE_MAX_PROBE 512

/* ── growable string ─────────────────────────────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} core_sbuf;

static int core_sb_init(core_sbuf *s)
{
    s->cap = 64;
    s->len = 0;
    s->data = (char *)malloc(s->cap);
    if (!s->data) return 0;
    s->data[0] = '\0';
    return 1;
}

static int core_sb_addn(core_sbuf *s, const char *t, size_t n)
{
    if (s->len + n + 1 > s->cap) {
        size_t cap = s->cap;
        char *grown;
        while (cap < s->len + n + 1) cap *= 2;
        grown = (char *)realloc(s->data, cap);
        if (!grown) return 0;
        s->data = grown;
        s->cap = cap;
    }
    if (n) memcpy(s->data + s->len, t, n);
    s->len += n;
    s->data[s->len] = '\0';
    return 1;
}

static int core_sb_add(core_sbuf *s, const char *t)
{
    return core_sb_addn(s, t, strlen(t));
}

/* ── small value helpers ─────────────────────────────────────────────────── */

static int core_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static char *core_dup(const char *s)
{
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* str.strip() */
static char *core_strip_dup(const char *s)
{
    const char *start = s ? s : "";
    const char *end;
    size_t n;
    char *out;
    while (*start && core_space(*start)) start++;
    end = start + strlen(start);
    while (end > start && core_space(end[-1])) end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

static const char *core_str(const json_t *obj, const char *key, const char *def)
{
    const json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static const char *core_env_get(const json_t *env, const char *name)
{
    const json_t *v;
    if (!env || env->type != JSON_OBJECT || !name || !name[0]) return NULL;
    v = json_obj_get(env, name);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return NULL;
}

static int core_in_list(const json_t *list, const char *text)
{
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return 0;
    n = json_len(list);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(list, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, text) == 0) return 1;
    }
    return 0;
}

/* ── path expansion (the injected env mapping) ───────────────────────────── */

/* "$NAME" / "${NAME}" from the env mapping; an unknown name stays literal. */
static char *core_expand_vars(const char *in, const json_t *env)
{
    core_sbuf out;
    size_t i = 0, len;
    if (!in) return core_dup("");
    if (!core_sb_init(&out)) return NULL;
    len = strlen(in);
    while (i < len) {
        if (in[i] == '$') {
            size_t j = i + 1, n = 0;
            char name[128];
            int braced = 0;
            if (j < len && in[j] == '{') { braced = 1; j++; }
            while (j < len && n + 1 < sizeof(name)) {
                char ch = in[j];
                int ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                         (ch >= '0' && ch <= '9') || ch == '_';
                if (!ok) break;
                name[n++] = ch;
                j++;
            }
            name[n] = '\0';
            if (braced) {
                if (j < len && in[j] == '}') j++;
                else { core_sb_addn(&out, in + i, 1); i++; continue; }
            }
            if (n == 0) { core_sb_addn(&out, in + i, 1); i++; continue; }
            {
                const char *value = core_env_get(env, name);
                if (value) core_sb_add(&out, value);
                else core_sb_addn(&out, in + i, j - i);
            }
            i = j;
        } else {
            core_sb_addn(&out, in + i, 1);
            i++;
        }
    }
    return out.data;
}

/* A leading "~" from USERPROFILE (nt) / HOME (other); "~user" is left alone. */
static char *core_expand_user(const char *in, const json_t *env, int is_nt)
{
    const char *home;
    core_sbuf out;
    if (!in || in[0] != '~') return core_dup(in);
    if (in[1] != '\0' && in[1] != '/' && in[1] != '\\') return core_dup(in);
    home = is_nt ? core_env_get(env, "USERPROFILE") : core_env_get(env, "HOME");
    if (!home) home = is_nt ? core_env_get(env, "HOME") : core_env_get(env, "USERPROFILE");
    if (!home) return core_dup(in);
    if (!core_sb_init(&out)) return NULL;
    core_sb_add(&out, home);
    core_sb_add(&out, in + 1);
    return out.data;
}

static char *core_expand(const char *in, const json_t *env, int is_nt)
{
    char *user = core_expand_user(in, env, is_nt);
    char *vars;
    if (!user) return NULL;
    vars = core_expand_vars(user, env);
    free(user);
    return vars;
}

/* ── path predicates ─────────────────────────────────────────────────────── */

static int core_is_alpha(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static int core_is_abs(const char *p, int is_nt)
{
    if (!p || !p[0]) return 0;
    if (is_nt) {
        if (p[0] == '/' || p[0] == '\\') return 1;
        if (core_is_alpha(p[0]) && p[1] == ':' && (p[2] == '/' || p[2] == '\\')) return 1;
        return 0;
    }
    return p[0] == '/';
}

static char *core_join(const char *base, const char *name, int is_nt, const char *sep)
{
    core_sbuf out;
    size_t bl;
    if (!base) base = "";
    if (!name) name = "";
    if (core_is_abs(name, is_nt)) return core_dup(name);
    if (!base[0]) return core_dup(name);
    if (!core_sb_init(&out)) return NULL;
    core_sb_add(&out, base);
    bl = strlen(base);
    if (!(base[bl - 1] == '/' || base[bl - 1] == '\\')) core_sb_add(&out, sep);
    core_sb_add(&out, name);
    return out.data;
}

/* ── the passive probe ───────────────────────────────────────────────────── */

static const char *core_which_lookup(const json_t *map, const char *probe)
{
    const json_t *v;
    if (!map || map->type != JSON_OBJECT) return NULL;
    v = json_obj_get(map, probe);
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) return v->str_val;
    return NULL;
}

/* _which(name, ctx): an explicit "" path searches nothing, and an explicit
 * pathext on nt is tried after the bare name. */
static const char *core_which(const char *name, const json_t *ctx, int is_nt, char pathsep_ch,
                              const json_t *which_map)
{
    const json_t *path_node = NULL, *pathext = NULL;
    const char *hit;
    if (ctx && ctx->type == JSON_OBJECT) {
        path_node = json_obj_get(ctx, "path");
        pathext = json_obj_get(ctx, "pathext");
    }
    if (path_node && path_node->type == JSON_STRING && path_node->str_val && !path_node->str_val[0])
        return NULL;
    if (pathext && pathext->type != JSON_STRING) pathext = NULL;
    if (pathext && is_nt) {
        const char *p = pathext->str_val ? pathext->str_val : "";
        hit = core_which_lookup(which_map, name);
        if (hit) return hit;
        while (p && *p) {
            const char *end = strchr(p, pathsep_ch);
            size_t n = end ? (size_t)(end - p) : strlen(p);
            if (n > 0 && strlen(name) + n + 1 < CORE_MAX_PROBE) {
                char probe[CORE_MAX_PROBE];
                memcpy(probe, name, strlen(name));
                memcpy(probe + strlen(name), p, n);
                probe[strlen(name) + n] = '\0';
                hit = core_which_lookup(which_map, probe);
                if (hit) return hit;
            }
            if (!end) break;
            p = end + 1;
        }
        return NULL;
    }
    return core_which_lookup(which_map, name);
}

/* ── Resolution views ────────────────────────────────────────────────────── */

static json_t *core_emit(const char *kind, const json_t *candidates)
{
    json_t *out = json_object();
    json_t *command = json_array();
    json_t *present = json_array();
    char source[CORE_MAX_PROBE];
    size_t i, n = (candidates && candidates->type == JSON_ARRAY) ? json_len(candidates) : 0;
    int have_first = 0;

    source[0] = '\0';
    for (i = 0; i < n; i++) {
        const json_t *c = json_get(candidates, i);
        if (!c || c->type != JSON_OBJECT) continue;
        if (!json_get_bool(c, "present", 0)) continue;
        json_append(present, json_copy(c));
        if (!have_first) {
            const json_t *value = json_obj_get(c, "value");
            const char *src = core_str(c, "source", "none");
            size_t sl = strlen(src);
            have_first = 1;
            json_append(command, value ? json_copy(value) : json_null());
            if (sl >= sizeof(source)) sl = sizeof(source) - 1;
            memcpy(source, src, sl);
            source[sl] = '\0';
        }
    }
    json_set(out, "kind", json_string(kind ? kind : "missing"));
    json_set(out, "candidates", candidates ? json_copy(candidates) : json_array());
    json_set(out, "command", command);
    json_set(out, "source", json_string(have_first ? source : "none"));
    json_set(out, "present", present);
    json_set(out, "found", json_bool(kind && strcmp(kind, "missing") != 0));
    return out;
}

json_t *w23_core_082b31_resolution(const json_t *req)
{
    const char *kind = "missing";
    const json_t *candidates;
    if (!req || req->type != JSON_OBJECT) return core_emit("missing", NULL);
    kind = core_str(req, "kind", "missing");
    candidates = json_obj_get(req, "candidates");
    return core_emit(kind, candidates);
}

json_t *w23_core_082b31_effective_path(const json_t *ctx)
{
    const json_t *path;
    if (!ctx || ctx->type != JSON_OBJECT) return json_null();
    path = json_obj_get(ctx, "path");
    if (path && path->type == JSON_STRING) return json_copy(path);
    return json_null();
}

/* ── locate_command ──────────────────────────────────────────────────────── */

static void core_cand_add(json_t *arr, const char *value, const char *source, int present)
{
    json_t *c = json_object();
    json_set(c, "value", json_string(value ? value : ""));
    json_set(c, "source", json_string(source ? source : ""));
    json_set(c, "present", json_bool(present != 0));
    json_append(arr, c);
}

json_t *w23_core_082b31_locate(const json_t *req)
{
    char *name = NULL;
    const json_t *ctx = NULL, *known_dirs = NULL, *env = NULL, *which_map = NULL;
    const json_t *files = NULL, *xok = NULL;
    const char *os_name, *sep, *altsep, *pathsep;
    int is_nt, has_sep, has_altsep;
    json_t *candidates, *out;

    if (!req || req->type != JSON_OBJECT) return core_emit("missing", NULL);
    os_name = core_str(req, "os_name", "nt");
    is_nt = os_name && strcmp(os_name, "nt") == 0;
    sep = core_str(req, "sep", is_nt ? "\\" : "/");
    altsep = core_str(req, "altsep", is_nt ? "/" : NULL);
    pathsep = core_str(req, "pathsep", is_nt ? ";" : ":");
    ctx = json_obj_get(req, "ctx");
    known_dirs = json_obj_get(req, "known_dirs");
    env = json_obj_get(req, "env");
    which_map = json_obj_get(req, "which");
    files = json_obj_get(req, "files");
    xok = json_obj_get(req, "xok");

    name = core_strip_dup(core_str(req, "name", ""));
    if (!name) return core_emit("missing", NULL);

    has_sep = sep && sep[0] && strstr(name, sep) != NULL;
    has_altsep = altsep && altsep[0] && strstr(name, altsep) != NULL;

    candidates = json_array();
    if (has_sep || has_altsep) {
        char *expanded = core_expand(name, env, is_nt);
        int present = expanded && core_is_abs(expanded, is_nt) &&
                      core_in_list(files, expanded) && core_in_list(xok, expanded);
        core_cand_add(candidates, expanded ? expanded : name, "explicit", present);
        out = core_emit(present ? "explicit_path" : "missing", candidates);
        json_free(candidates);
        free(expanded);
        free(name);
        return out;
    }

    {
        const char *hit = core_which(name, ctx, is_nt, (pathsep && pathsep[0]) ? pathsep[0] : ';', which_map);
        core_cand_add(candidates, hit ? hit : name, "PATH", hit != NULL);
    }

    if (known_dirs && known_dirs->type == JSON_ARRAY) {
        size_t i, n = json_len(known_dirs);
        for (i = 0; i < n && json_len(candidates) < CORE_MAX_CANDIDATES; i++) {
            const json_t *d = json_get(known_dirs, i);
            char source[256];
            char *dir_exp, *base, *found = NULL;
            if (!d || d->type != JSON_STRING || !d->str_val) continue;
            dir_exp = core_expand(d->str_val, env, is_nt);
            base = core_join(dir_exp ? dir_exp : d->str_val, name, is_nt, sep);
            free(dir_exp);
            if (base) {
                if (is_nt) {
                    const char *raw = core_env_get(env, "PATHEXT");
                    const char *p = raw ? raw : ".EXE;.CMD;.BAT";
                    char probe[CORE_MAX_PROBE];
                    size_t bl = strlen(base);
                    if (core_in_list(files, base) && core_in_list(xok, base)) found = core_dup(base);
                    while (!found && p && *p) {
                        const char *end = strchr(p, ';');
                        size_t en = end ? (size_t)(end - p) : strlen(p);
                        if (en > 0 && bl + en + 1 < sizeof(probe)) {
                            memcpy(probe, base, bl);
                            memcpy(probe + bl, p, en);
                            probe[bl + en] = '\0';
                            if (core_in_list(files, probe) && core_in_list(xok, probe)) found = core_dup(probe);
                        }
                        if (!end) break;
                        p = end + 1;
                    }
                } else if (core_in_list(files, base) && core_in_list(xok, base)) {
                    found = core_dup(base);
                }
            }
            snprintf(source, sizeof(source), "known_dir:%s", d->str_val);
            core_cand_add(candidates, found ? found : base, source, found != NULL);
            free(found);
            free(base);
        }
    }

    {
        size_t i, n = json_len(candidates);
        const char *kind = "missing";
        for (i = 0; i < n; i++) {
            const json_t *c = json_get(candidates, i);
            if (!json_get_bool(c, "present", 0)) continue;
            kind = strcmp(core_str(c, "source", ""), "PATH") == 0 ? "path_executable" : "known_path";
            break;
        }
        out = core_emit(kind, candidates);
    }
    json_free(candidates);
    free(name);
    return out;
}
