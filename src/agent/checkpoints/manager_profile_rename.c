/*
 * manager_profile_rename.c -- real C11 port of upstream Hermes
 *   agent/checkpoints/manager_profile_rename.py.
 *
 * PoP: w21_managerpro_68c845_rebase_ledger_paths @ agent/checkpoints/manager_profile_rename.py:_rebase_ledger_paths
 * PoP: w21_managerpro_68c845_migrate_plan @ agent/checkpoints/manager_profile_rename.py:migrate_profile_checkpoint_projects
 * PoP: w21_managerpro_68c845_env_probe @ agent/checkpoints/manager_profile_rename.py:migrate_profile_checkpoint_projects
 *
 * What is ported is every decision the module makes: which ledger keys move
 * (Windows PurePath relative_to: anchor-equal + case-insensitive part prefix,
 * "." dropped, ".." kept, "\\" when joined) and which projects the rename
 * scans, skips, migrates or counts as an error. The git store, the project
 * metadata list and Path.is_dir() are filesystem seams supplied as `world`.
 */
#include "hermes_json.h"
#include "noc_w21_managerpro_68c845.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MP_MAX_PARTS 32
#define MP_MAX_PART  256
#define MP_MAX_ANCHOR 16

typedef struct {
    char anchor[MP_MAX_ANCHOR];              /* "C:\\" / "\\" / "" */
    char parts[MP_MAX_PARTS][MP_MAX_PART];
    int n;
} mp_path;

static void mp_copy(char *dst, size_t cap, const char *src, size_t len) {
    if (len >= cap) len = cap - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';                          /* NUL-terminate after shortening */
}

static int mp_is_sep(char c) { return c == '\\' || c == '/'; }

/* PureWindowsPath(...) as this host renders it: drive/root anchor, then parts
 * with empty and "." components dropped and ".." preserved. */
static void mp_norm(const char *in, mp_path *out) {
    const char *p = in ? in : "";
    out->n = 0;
    out->anchor[0] = '\0';
    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        out->anchor[0] = p[0];
        out->anchor[1] = ':';
        out->anchor[2] = '\\';
        out->anchor[3] = '\0';
        p += 2;
        while (mp_is_sep(*p)) p++;
    } else if (mp_is_sep(p[0])) {
        out->anchor[0] = '\\';
        out->anchor[1] = '\0';
        while (mp_is_sep(*p)) p++;
    }
    while (*p) {
        const char *start = p;
        size_t len;
        while (*p && !mp_is_sep(*p)) p++;
        len = (size_t)(p - start);
        while (mp_is_sep(*p)) p++;
        if (len == 0 || (len == 1 && start[0] == '.')) continue;
        if (out->n >= MP_MAX_PARTS) break;
        mp_copy(out->parts[out->n], MP_MAX_PART, start, len);
        out->n++;
    }
}

/* str(path): anchor + parts joined with "\\". */
static void mp_str(const mp_path *path, char *buf, size_t cap) {
    size_t used = 0;
    int i;
    buf[0] = '\0';
    if (path->anchor[0]) {
        size_t a = strlen(path->anchor);
        if (a >= cap) a = cap - 1;
        memcpy(buf, path->anchor, a);
        used = a;
        buf[used] = '\0';
    }
    for (i = 0; i < path->n; i++) {
        size_t l = strlen(path->parts[i]);
        if (used && buf[used - 1] != '\\') {
            if (used + 1 >= cap) break;
            buf[used++] = '\\';
            buf[used] = '\0';
        }
        if (used + l >= cap) l = cap - 1 - used;
        memcpy(buf + used, path->parts[i], l);
        used += l;
        buf[used] = '\0';
    }
    if (used == 0) strcpy(buf, ".");
}

static int mp_ieq(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

/* PurePath.relative_to(other): parts of self start with parts of other and the
 * anchors agree (both none, or the same drive). Returns 1 and fills `rel`. */
static int mp_relative_to(const mp_path *self, const mp_path *other, mp_path *rel) {
    int i;
    if (!!self->anchor[0] != !!other->anchor[0]) return 0;
    if (self->anchor[0] && !mp_ieq(self->anchor, other->anchor)) return 0;
    if (other->n > self->n) return 0;
    for (i = 0; i < other->n; i++) {
        if (!mp_ieq(self->parts[i], other->parts[i])) return 0;
    }
    rel->n = 0;
    rel->anchor[0] = '\0';
    for (i = other->n; i < self->n; i++) {
        if (rel->n >= MP_MAX_PARTS) break;
        mp_copy(rel->parts[rel->n], MP_MAX_PART, self->parts[i], strlen(self->parts[i]));
        rel->n++;
    }
    return 1;
}

/* new_base / relative */
static void mp_join(const mp_path *base, const mp_path *rel, mp_path *out) {
    int i;
    memcpy(out->anchor, base->anchor, MP_MAX_ANCHOR);
    out->n = 0;
    for (i = 0; i < base->n && out->n < MP_MAX_PARTS; i++) {
        mp_copy(out->parts[out->n], MP_MAX_PART, base->parts[i], strlen(base->parts[i]));
        out->n++;
    }
    for (i = 0; i < rel->n && out->n < MP_MAX_PARTS; i++) {
        mp_copy(out->parts[out->n], MP_MAX_PART, rel->parts[i], strlen(rel->parts[i]));
        out->n++;
    }
}

json_t *w21_managerpro_68c845_rebase_ledger_paths(const json_t *ledger,
                                                  const char *old_workdir,
                                                  const char *new_workdir) {
    mp_path oldp, newp;
    json_t *out = json_object();
    size_t i;

    mp_norm(old_workdir, &oldp);
    mp_norm(new_workdir, &newp);
    if (!ledger || ledger->type != JSON_OBJECT) return out;

    for (i = 0; i < ledger->c.count; i++) {
        const char *key = ledger->c.keys[i];
        json_t *entry = ledger->c.items[i];
        mp_path rawp, rel, joined;
        char buf[4096];

        mp_norm(key, &rawp);
        if (!mp_relative_to(&rawp, &oldp, &rel)) {
            json_set(out, key ? key : "", entry ? json_copy(entry) : json_null());
            continue;
        }
        mp_join(&newp, &rel, &joined);
        mp_str(&joined, buf, sizeof(buf));
        json_set(out, buf, entry ? json_copy(entry) : json_null());
    }
    return out;
}

json_t *w21_managerpro_68c845_migrate_plan(const json_t *world) {
    mp_path old_root, new_root;
    json_t *out = json_object();
    json_t *projects;
    long scanned = 0, migrated = 0, errors = 0;
    size_t i;

    if (!world || world->type != JSON_OBJECT) world = NULL;
    mp_norm(world ? json_get_str(world, "old_root", "") : "", &old_root);
    mp_norm(world ? json_get_str(world, "new_root", "") : "", &new_root);

    if (!world || !json_get_bool(world, "has_head", false)) {
        /* if not cm._store_has_head(store): return result */
        json_set(out, "scanned", json_number(0));
        json_set(out, "migrated", json_number(0));
        json_set(out, "errors", json_number(0));
        return out;
    }
    if (!json_get_bool(world, "git", false)) {
        /* Without git the ref cannot move; only the metadata would be rekeyed. */
        json_set(out, "scanned", json_number(0));
        json_set(out, "migrated", json_number(0));
        json_set(out, "errors", json_number(1));
        return out;
    }

    projects = json_obj_get(world, "projects");
    if (projects && projects->type == JSON_ARRAY) {
        for (i = 0; i < projects->c.count; i++) {
            json_t *meta = projects->c.items[i];
            const char *workdir;
            mp_path wd, rel;
            if (!meta || meta->type != JSON_OBJECT) continue;
            scanned++;
            workdir = json_get_str(meta, "workdir", "");
            if (!workdir || !workdir[0]) continue;              /* not meta.get("workdir") */
            mp_norm(workdir, &wd);
            if (!mp_relative_to(&wd, &old_root, &rel)) continue;
            /* new_workdir = new_root / rel; the fixture supplies its is_dir() verdict */
            if (!json_get_bool(meta, "exists", false)) continue;
            if (json_get_bool(meta, "fail", false)) errors++;
            else migrated++;
        }
    }
    json_set(out, "scanned", json_number((double)scanned));
    json_set(out, "migrated", json_number((double)migrated));
    json_set(out, "errors", json_number((double)errors));
    return out;
}

json_t *w21_managerpro_68c845_env_probe(void) {
    const char *sler = getenv("SLERMES_HOME");
    const char *home = getenv("HOME");
    json_t *out = json_object();
    json_set(out, "sler", json_bool(sler && sler[0]));
    json_set(out, "home", json_bool(home && home[0]));
    json_set(out, "same", json_bool(sler && home && !strcmp(sler, home)));
    return out;
}
