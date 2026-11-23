/*
 * media_fetch.c — C home for upstream Hermes gateway/media_fetch.py.
 * Ported behavior: the pure remote-path denylist screen. The environment
 * fetch, realpath and cache-copy path stay in Python.
 * PoP: w6_mediafetch_remote_path_is_denied @ gateway/media_fetch.py:remote_path_is_denied
 */
#include "noc_w6_mediafetch_d0155c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARTS 128

typedef struct {
    char *v[MAX_PARTS];
    int n;
} parts_t;

static void parts_free(parts_t *p) {
    int i;
    for (i = 0; i < p->n; i++) free(p->v[i]);
    p->n = 0;
}

static void parts_push(parts_t *p, const char *s, size_t len) {
    char *x;
    if (p->n >= MAX_PARTS) return;
    x = (char *)malloc(len + 1);
    if (!x) return;
    memcpy(x, s, len);
    x[len] = '\0';
    p->v[p->n++] = x;
}

/* posixpath.normpath() followed by PurePosixPath.parts(). A leading '/'
 * becomes the first part (as PurePosixPath does for absolute paths). */
static void path_parts(const char *path, int *absolute, parts_t *out) {
    char buf[32768];
    size_t len = 0;
    const char *p = path ? path : "";
    char *tok;
    *absolute = (*p == '/');
    while (*p && len + 1 < sizeof(buf)) buf[len++] = *p++;
    buf[len] = '\0';
    if (*absolute) parts_push(out, "/", 1);
    tok = strtok(buf, "/");
    while (tok) {
        if (!strcmp(tok, ".")) {
            /* PurePosixPath drops '.' components. */
        } else if (!strcmp(tok, "..")) {
            if (out->n > (*absolute ? 1 : 0) && strcmp(out->v[out->n - 1], "..") != 0) {
                free(out->v[--out->n]);
            } else if (!*absolute) {
                parts_push(out, "..", 2);
            }
        } else {
            parts_push(out, tok, strlen(tok));
        }
        tok = strtok(NULL, "/");
    }
}

static int parts_equal(const parts_t *a, const parts_t *b) {
    int i;
    if (a->n != b->n) return 0;
    for (i = 0; i < a->n; i++)
        if (strcmp(a->v[i], b->v[i])) return 0;
    return 1;
}

/* target == root or root in target.parents  <=>  root is a prefix of target. */
static int under(const parts_t *root, const parts_t *target) {
    int i;
    if (root->n > target->n) return 0;
    for (i = 0; i < root->n; i++)
        if (strcmp(root->v[i], target->v[i])) return 0;
    return 1;
}

/* Unknown home: any component window equal to rel.parts. */
static int contains_seq(const parts_t *target, const parts_t *rel) {
    int i, j;
    if (rel->n == 0 || rel->n > target->n) return 0;
    for (i = 0; i + rel->n <= target->n; i++) {
        int ok = 1;
        for (j = 0; j < rel->n; j++) {
            if (strcmp(target->v[i + j], rel->v[j])) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

/* Constant sets mirrored from gateway.platforms.base (see the oracle stub). */
static const char *const DENIED_PREFIXES[] = {
    "/etc", "/proc", "/sys", "/dev", "/root", "/boot", "/var/log", "/var/lib", "/var/run"
};
#define N_DENIED_PREFIXES ((int)(sizeof(DENIED_PREFIXES) / sizeof(DENIED_PREFIXES[0])))

static const char *const DENIED_HOME_SUBPATHS[] = {
    ".ssh", ".aws", ".gnupg", ".kube", ".docker", ".config", ".azure", ".gcloud",
    "Library/Keychains"
};
#define N_HOME_SUBPATHS ((int)(sizeof(DENIED_HOME_SUBPATHS) / sizeof(DENIED_HOME_SUBPATHS[0])))

static const char *const ROOT_CREDENTIAL_PATHS[] = {
    "auth.json", "auth.lock", "credentials", "config.yaml", ".anthropic_oauth.json",
    "google_token.json", "google_client_secret.json", "google_oauth_pending.json",
    "webhook_subscriptions.json", "sessions", "browser-profile", "state.db",
    "state.db-wal", "state.db-shm", "state.db-journal", "kanban.db", "kanban.db-wal",
    "kanban.db-shm", "kanban.db-journal", "webapp", "auth/google_oauth.json"
};
#define N_ROOT_CREDENTIALS ((int)(sizeof(ROOT_CREDENTIAL_PATHS) / sizeof(ROOT_CREDENTIAL_PATHS[0])))

static void build_rel(const char *spec, int hermes_prefix, parts_t *out) {
    parts_t t = {0};
    int abs = 0, k;
    path_parts(spec, &abs, &t);
    if (hermes_prefix) parts_push(out, ".hermes", 7);
    for (k = 0; k < t.n; k++) parts_push(out, t.v[k], strlen(t.v[k]));
    parts_free(&t);
}

static int rel_denies(const parts_t *rel, const parts_t *target,
                      const parts_t *home, int have_home) {
    if (have_home) {
        parts_t joined = {0};
        int k, r;
        for (k = 0; k < home->n; k++) parts_push(&joined, home->v[k], strlen(home->v[k]));
        for (k = 0; k < rel->n; k++) parts_push(&joined, rel->v[k], strlen(rel->v[k]));
        r = under(&joined, target);
        parts_free(&joined);
        return r;
    }
    return contains_seq(target, rel);
}

int w6_mediafetch_remote_path_is_denied(const char *path, const char *remote_home) {
    parts_t target = {0}, home = {0}, pp = {0}, rel = {0};
    int absolute = 0, home_abs = 0, have_home = 0, denied = 0;
    int i;

    path_parts(path, &absolute, &target);
    if (!absolute) return 1; /* target.is_absolute() is False -> denied */

    if (remote_home && *remote_home) {
        path_parts(remote_home, &home_abs, &home);
        have_home = 1;
    }

    /* any(_under(p) for p in _DENIED_PREFIXES if p != home) */
    for (i = 0; i < N_DENIED_PREFIXES; i++) {
        path_parts(DENIED_PREFIXES[i], &home_abs, &pp);
        if (!(have_home && parts_equal(&pp, &home)) && under(&pp, &target))
            denied = 1;
        parts_free(&pp);
        if (denied) goto done;
    }

    /* home is known: any(_under(home / rel) for rel in _DENIED_HOME_RELATIVE) */
    /* home unknown: any component window matches rel.parts */
    for (i = 0; i < N_HOME_SUBPATHS && !denied; i++) {
        rel.n = 0;
        build_rel(DENIED_HOME_SUBPATHS[i], 0, &rel);
        denied = rel_denies(&rel, &target, &home, have_home);
        parts_free(&rel);
    }
    for (i = 0; i < N_ROOT_CREDENTIALS && !denied; i++) {
        rel.n = 0;
        build_rel(ROOT_CREDENTIAL_PATHS[i], 1, &rel);
        denied = rel_denies(&rel, &target, &home, have_home);
        parts_free(&rel);
    }

done:
    parts_free(&target);
    parts_free(&home);
    return denied ? 1 : 0;
}
