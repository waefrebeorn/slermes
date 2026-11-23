/*
 * data_cleanup.c — faithful C11 port of upstream Hermes hermes_cli/data_cleanup.py.
 *
 * PoP: w9_datacleanup_07fe4b_plan @ hermes_cli/data_cleanup.py:plan_data_removal
 *
 * The virtual filesystem (paths, dir/file/symlink/junction flags, resolved
 * targets) and the external path helpers (hermes root, venv/store/installs
 * roots, bundle layout, store lock) are fixture seams; the live module's own
 * plan_data_removal runs unmodified in the oracle against a fake pathlib.Path.
 */
#define _GNU_SOURCE
#include "noc_w9_datacleanup_07fe4b.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *path;
    int dir, link, junction, exists;
    char *resolved;
} Ent;

typedef struct {
    const json_t *spec;
    char *home, *machine, *user_home, *project, *base_venv, *store_root;
    char *installs_root, *store_lock, *bundle, *userdata;
    int canonical_present;
    Ent *ents; size_t nent;
    char **prot; size_t nprot;
    char **keep; size_t nkeep;
    json_t *remove;
} Ctx;

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* posixpath.normpath over virtual (forward-slash) paths. */
static char *norm(const char *p) {
    if (!p || !*p) return xstrdup("/");
    char *tok[128];
    int n = 0;
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", p);
    char *save = NULL, *t = strtok_r(buf, "/", &save);
    while (t && n < 128) {
        if (!strcmp(t, ".")) { t = strtok_r(NULL, "/", &save); continue; }
        if (!strcmp(t, "..")) { if (n > 0) n--; t = strtok_r(NULL, "/", &save); continue; }
        tok[n++] = xstrdup(t);
        t = strtok_r(NULL, "/", &save);
    }
    char *out = malloc(4096);
    if (!out) return xstrdup("/");
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        strcat(out, "/");
        strcat(out, tok[i]);
        free(tok[i]);
    }
    if (!*out) strcpy(out, "/");
    return out;
}

static char *joinp(const char *a, const char *b) {
    char buf[4096];
    snprintf(buf, sizeof buf, "%s/%s", a ? a : "", b ? b : "");
    return norm(buf);
}

static char *parentdir(const char *p) {
    if (!p) return xstrdup("/");
    const char *slash = strrchr(p, '/');
    if (!slash) return xstrdup("/");
    if (slash == p) return xstrdup("/");
    size_t n = (size_t)(slash - p);
    char buf[4096];
    snprintf(buf, sizeof buf, "%.*s", (int)n, p);
    return norm(buf);
}

static const char *basenamep(const char *p) {
    if (!p) return "";
    const char *slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}

/* pathlib.PurePath.is_relative_to — lexical, component-boundary aware. */
static int relto(const char *a, const char *b) {
    if (!a || !b) return 0;
    if (!strcmp(a, b)) return 1;
    if (!strcmp(b, "/")) return a[0] == '/';
    size_t n = strlen(b);
    return !strncmp(a, b, n) && a[n] == '/';
}

static const char *sfield(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int bfield(const json_t *obj, const char *key, int def) {
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_BOOL) ? (v->bool_val ? 1 : 0) : def;
}

static Ent *find_ent(Ctx *c, const char *path) {
    for (size_t i = 0; i < c->nent; i++)
        if (!strcmp(c->ents[i].path, path)) return &c->ents[i];
    return NULL;
}

static char *resolve_virtual(Ctx *c, const char *path) {
    Ent *e = find_ent(c, path);
    return e && e->resolved ? norm(e->resolved) : norm(path);
}

static void add_prot(Ctx *c, const char *path) {
    char *n = norm(path);
    for (size_t i = 0; i < c->nprot; i++)
        if (!strcmp(c->prot[i], n)) { free(n); return; }
    c->prot = realloc(c->prot, (c->nprot + 1) * sizeof(char *));
    c->prot[c->nprot++] = n;
}

static void add_keep(Ctx *c, const char *path) {
    for (size_t i = 0; i < c->nkeep; i++)
        if (!strcmp(c->keep[i], path)) return;
    c->keep = realloc(c->keep, (c->nkeep + 1) * sizeof(char *));
    c->keep[c->nkeep++] = xstrdup(path);
}

static int cmp_ent(const void *a, const void *b) {
    const Ent *x = *(const Ent *const *)a, *y = *(const Ent *const *)b;
    return strcmp(x->path, y->path);
}

/* sorted(path.iterdir()): direct, existing children only. */
static Ent **children_of(Ctx *c, const char *dir, size_t *count) {
    Ent **out = calloc(c->nent + 1, sizeof(Ent *));
    size_t n = 0;
    for (size_t i = 0; i < c->nent; i++) {
        Ent *e = &c->ents[i];
        if (!e->exists) continue;
        char *par = parentdir(e->path);
        int direct = !strcmp(par, dir) && strcmp(e->path, dir) != 0;
        free(par);
        if (direct) out[n++] = e;
    }
    qsort(out, n, sizeof(Ent *), cmp_ent);
    *count = n;
    return out;
}

static void visit(Ctx *c, const char *path, int depth) {
    if (depth > 64) return;
    Ent *e = find_ent(c, path);
    if (!e || !e->exists) return;
    char *resolved = resolve_virtual(c, path);
    int keep_it = 0;
    for (size_t i = 0; i < c->nprot; i++)
        if (!strcmp(path, c->prot[i]) || !strcmp(resolved, c->prot[i]) || relto(path, c->prot[i])) {
            keep_it = 1; break;
        }
    free(resolved);
    if (keep_it) { add_keep(c, path); return; }
    if (e->link || e->junction) { json_append(c->remove, json_string(path)); return; }
    int under = 0;
    for (size_t i = 0; i < c->nprot; i++)
        if (relto(c->prot[i], path)) { under = 1; break; }
    if (!under) { json_append(c->remove, json_string(path)); return; }
    if (!e->dir) { add_keep(c, path); return; }
    size_t n = 0;
    Ent **kids = children_of(c, path, &n);
    for (size_t i = 0; i < n; i++) visit(c, kids[i]->path, depth + 1);
    free(kids);
}

static json_t *plan_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}

static int cmp_strptr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w9_datacleanup_07fe4b_plan(const json_t *spec) {
    if (!json_is_object(spec)) return plan_error("spec must be an object");
    Ctx c;
    memset(&c, 0, sizeof c);
    c.spec = spec;
    c.remove = json_array();
    json_t *entries = json_obj_get(spec, "entries");
    if (entries && entries->type == JSON_ARRAY) {
        c.ents = calloc(json_len(entries) + 1, sizeof(Ent));
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            const char *p = sfield(e, "path");
            if (!p) continue;
            c.ents[c.nent].path = norm(p);
            c.ents[c.nent].dir = bfield(e, "dir", 0);
            c.ents[c.nent].link = bfield(e, "link", 0);
            c.ents[c.nent].junction = bfield(e, "junction", 0);
            json_t *ex = json_obj_get(e, "exists");
            c.ents[c.nent].exists = (!ex || ex->type != JSON_BOOL || ex->bool_val) ? 1 : 0;
            const char *rs = sfield(e, "resolved");
            c.ents[c.nent].resolved = rs ? norm(rs) : NULL;
            c.nent++;
        }
    }
    const char *home_s = sfield(spec, "home");
    if (!home_s) { json_free(c.remove); return plan_error("home is required"); }
    c.home = resolve_virtual(&c, home_s);
    c.user_home = norm(sfield(spec, "user_home") ? sfield(spec, "user_home") : "/home/user");
    c.machine = resolve_virtual(&c, sfield(spec, "machine") ? sfield(spec, "machine") : c.home);
    c.project = resolve_virtual(&c, sfield(spec, "project") ? sfield(spec, "project") : "/project");
    c.base_venv = resolve_virtual(&c, sfield(spec, "base_venv") ? sfield(spec, "base_venv") : "/project/.venv");
    c.store_root = resolve_virtual(&c, sfield(spec, "store_root") ? sfield(spec, "store_root") : "/project/.store");
    c.installs_root = resolve_virtual(&c, sfield(spec, "installs_root") ? sfield(spec, "installs_root") : "/opt/hermes/installs");
    c.store_lock = resolve_virtual(&c, sfield(spec, "store_lock") ? sfield(spec, "store_lock") : "/store/.lock");
    c.bundle = sfield(spec, "bundle_app_root") ? xstrdup(sfield(spec, "bundle_app_root")) : NULL;
    c.userdata = sfield(spec, "userdata") ? xstrdup(sfield(spec, "userdata")) : NULL;
    c.canonical_present = bfield(spec, "canonical_present", 0);

    if (!strcmp(c.home, "/") || !strcmp(c.home, c.user_home)) {
        char msg[512];
        snprintf(msg, sizeof msg, "refusing to erase an entire filesystem or user home: %s", c.home);
        return plan_error(msg);
    }

    /* protected = {...} in plan_data_removal, in source order. */
    add_prot(&c, c.project);
    add_prot(&c, c.base_venv);
    add_prot(&c, c.store_root);
    add_prot(&c, c.installs_root);
    add_prot(&c, joinp(c.machine, "tools"));
    add_prot(&c, joinp(c.machine, "bin"));
    add_prot(&c, joinp(c.machine, "cache"));
    add_prot(&c, joinp(c.machine, "spawn-ledger.json"));
    add_prot(&c, joinp(c.machine, "spawn-ledger.json.lock"));
    add_prot(&c, joinp(c.home, "gateway.lock"));
    add_prot(&c, joinp(c.home, ".backup.lock"));
    add_prot(&c, joinp(c.home, "runtime/active_sessions.lock"));
    add_prot(&c, joinp(c.home, "cron/.tick.lock"));
    add_prot(&c, c.store_lock);
    if (!strcmp(c.home, c.machine)) add_prot(&c, joinp(c.machine, "profiles"));
    if (c.bundle) add_prot(&c, resolve_virtual(&c, c.bundle));
    if (c.canonical_present) add_prot(&c, joinp(c.home, "hermes-agent"));

    Ent *home_ent = find_ent(&c, c.home);
    if (home_ent && home_ent->dir) {
        size_t n = 0;
        Ent **kids = children_of(&c, c.home, &n);
        for (size_t i = 0; i < n; i++) visit(&c, kids[i]->path, 0);
        free(kids);
    }

    if (strcmp(c.home, c.machine) != 0) { free(c.userdata); c.userdata = NULL; }
    if (c.userdata) {
        char *ud = resolve_virtual(&c, c.userdata);
        if (!relto(ud, c.home)) {
            if (!strcmp(ud, c.user_home) || relto(c.home, ud)) {
                char msg[512];
                snprintf(msg, sizeof msg, "desktop data directory must not contain the Hermes or user home: %s",
                         c.userdata);
                free(ud);
                return plan_error(msg);
            }
            char *par = parentdir(c.userdata);
            char *pr = resolve_virtual(&c, par);
            char *target = joinp(pr, basenamep(c.userdata));
            visit(&c, target, 0);
            free(par); free(pr); free(target);
        }
        free(ud);
    }

    qsort(c.keep, c.nkeep, sizeof(char *), cmp_strptr);
    json_t *out = json_object();
    json_t *ka = json_array();
    for (size_t i = 0; i < c.nkeep; i++) json_append(ka, json_string(c.keep[i]));
    json_set(out, "remove", c.remove);
    json_set(out, "keep", ka);
    return out;
}
