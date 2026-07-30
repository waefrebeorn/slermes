/* Slermes C11 port of gateway/dead_targets.py — implementation.
 * PoP: exact port. Semantic source of truth = gateway/dead_targets.py. */
#include "dead_targets.h"
#include "hermes_gateway_core.h"
#include "slermes_home.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Error kinds meaning the *whole chat* is unreachable. */
static const char *DEAD_ERROR_KINDS[] = {"forbidden", "not_found"};
#define N_DEAD_ERROR_KINDS 2

struct dead_target_registry {
    pthread_mutex_t lock;   /* RLock in Python; our critical sections never nest */
    json_t *dead;           /* JSON object: key -> {platform,chat_id,reason,marked_at} */
    char path[PATH_MAX];
};

/* Lowercase + strip a string into `out` (bufsz). Mirrors str.strip().lower(). */
static void strip_lower(const char *s, char *out, size_t bufsz) {
    if (!s) { if (bufsz) out[0] = '\0'; return; }
    /* strip leading */
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    /* strip trailing */
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    size_t j = 0;
    for (size_t i = 0; i < len && j + 1 < bufsz; i++)
        out[j++] = (char)tolower((unsigned char)s[i]);
    out[j] = '\0';
}

/* Strip only (no lowercase) into out. */
static void strip_only(const char *s, char *out, size_t bufsz) {
    if (!s) { if (bufsz) out[0] = '\0'; return; }
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    if (len >= bufsz) len = bufsz - 1;
    memcpy(out, s, len);
    out[len] = '\0';
}

/* PoP: dead_target_normalize @ gateway/dead_targets.py:_normalize */
static void dead_target_normalize(const char *platform, const char *chat_id,
                                  char *out, size_t bufsz) {
    char p[256], c[256];
    strip_lower(platform, p, sizeof(p));
    strip_only(chat_id, c, sizeof(c));
    snprintf(out, bufsz, "%s:%s", p, c);
}

/* PoP: dead_target_registry_load @ gateway/dead_targets.py:_load */
static void dead_target_registry_load(dead_target_registry_t *r) {
    FILE *f = fopen(r->path, "rb");
    if (!f) return; /* missing file -> stay empty */
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return; }
    char *raw = malloc((size_t)n + 1);
    if (!raw) { fclose(f); return; }
    size_t rd = fread(raw, 1, (size_t)n, f);
    raw[rd] = '\0';
    fclose(f);

    json_t *parsed = json_parse(raw, NULL);
    free(raw);
    if (parsed && parsed->type == JSON_OBJECT) {
        /* Only keep well-shaped entries (values that are objects). */
        json_t *kept = json_new_object();
        for (size_t i = 0; i < parsed->c.count; i++) {
            const char *k = parsed->c.keys[i];
            json_t *v = parsed->c.items[i];
            if (k && v && v->type == JSON_OBJECT)
                json_object_set(kept, k, json_copy(v));
        }
        json_free(r->dead);
        r->dead = kept;
    }
    if (parsed) json_free(parsed);
}

/* Best-effort atomic persist of the dead set to disk. Caller holds the lock. */
/* PoP: dead_target_flush_locked @ gateway/dead_targets.py:_flush_locked */
void dead_target_flush_locked(dead_target_registry_t *r) {
    /* mkdir -p parent */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", r->path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        /* create up to two levels (home + gateway) best-effort */
        char *slash2 = strrchr(dir, '/');
        if (slash2) { *slash2 = '\0'; mkdir(dir, 0700); *slash2 = '/'; }
        mkdir(dir, 0700);
    }

    char tmp[PATH_MAX + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", r->path);
    char *out = json_serialize_pretty(r->dead, 2);
    if (!out) return;
    FILE *f = fopen(tmp, "wb");
    if (f) {
        fwrite(out, 1, strlen(out), f);
        fclose(f);
        rename(tmp, r->path); /* atomic replace */
    }
    free(out);
}

dead_target_registry_t *dead_target_registry_create(const char *path) {
    dead_target_registry_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    pthread_mutex_init(&r->lock, NULL);
    r->dead = json_new_object();
    if (path && path[0]) {
        snprintf(r->path, sizeof(r->path), "%s", path);
    } else {
        snprintf(r->path, sizeof(r->path), "%s/gateway/dead_targets.json",
                 slermes_home());
    }
    dead_target_registry_load(r);
    return r;
}

void dead_target_registry_free(dead_target_registry_t *r) {
    if (!r) return;
    json_free(r->dead);
    pthread_mutex_destroy(&r->lock);
    free(r);
}

/* PoP: dead_target_is_dead_error_kind @ gateway/dead_targets.py:is_dead_error_kind */
bool dead_target_is_dead_error_kind(const char *error_kind) {
    if (!error_kind || !error_kind[0]) return false;
    for (int i = 0; i < N_DEAD_ERROR_KINDS; i++)
        if (strcmp(error_kind, DEAD_ERROR_KINDS[i]) == 0) return true;
    return false;
}

/* PoP: dead_target_is_dead @ gateway/dead_targets.py:is_dead */
bool dead_target_is_dead(dead_target_registry_t *r, const char *platform,
                         const char *chat_id) {
    if (!chat_id || !chat_id[0]) return false;
    char key[512];
    dead_target_normalize(platform, chat_id, key, sizeof(key));
    pthread_mutex_lock(&r->lock);
    bool found = json_object_get(r->dead, key) != NULL;
    pthread_mutex_unlock(&r->lock);
    return found;
}

/* PoP: dead_target_mark_dead @ gateway/dead_targets.py:mark_dead */
bool dead_target_mark_dead(dead_target_registry_t *r, const char *platform,
                           const char *chat_id, const char *reason) {
    if (!chat_id || !chat_id[0]) return false;
    char key[512];
    dead_target_normalize(platform, chat_id, key, sizeof(key));

    char plat[256];
    strip_lower(platform, plat, sizeof(plat));
    char reason_trunc[201];
    snprintf(reason_trunc, sizeof(reason_trunc), "%s", reason ? reason : "");

    pthread_mutex_lock(&r->lock);
    bool existed = json_object_get(r->dead, key) != NULL;
    json_t *entry = json_new_object();
    json_object_set(entry, "platform", json_new_string(plat));
    json_object_set(entry, "chat_id", json_new_string(chat_id));
    json_object_set(entry, "reason", json_new_string(reason_trunc));
    json_object_set(entry, "marked_at", json_new_number((double)time(NULL)));
    json_object_set(r->dead, key, entry);
    dead_target_flush_locked(r);
    pthread_mutex_unlock(&r->lock);

    return !existed;
}

/* Key deletion: canonical json_obj_del() lives in libjson. */
#define json_object_delete(obj, key) ((void)json_obj_del((obj), (key)))

/* PoP: dead_target_clear @ gateway/dead_targets.py:clear */
bool dead_target_clear(dead_target_registry_t *r, const char *platform,
                       const char *chat_id) {
    if (!chat_id || !chat_id[0]) return false;
    char key[512];
    dead_target_normalize(platform, chat_id, key, sizeof(key));
    pthread_mutex_lock(&r->lock);
    bool was_set = json_object_get(r->dead, key) != NULL;
    if (was_set) {
        json_object_delete(r->dead, key);
        dead_target_flush_locked(r);
    }
    pthread_mutex_unlock(&r->lock);
    return was_set;
}

/* PoP: dead_target_all_dead @ gateway/dead_targets.py:all_dead */
json_t *dead_target_all_dead(dead_target_registry_t *r) {
    pthread_mutex_lock(&r->lock);
    json_t *snap = json_copy(r->dead);
    pthread_mutex_unlock(&r->lock);
    return snap;
}
