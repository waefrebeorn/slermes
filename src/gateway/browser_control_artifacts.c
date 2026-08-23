/*
 * browser_control_artifacts.c — C11 port of gateway/browser_control_artifacts.py
 *
 * Thread-safe, TTL-bounded, scope-bound one-shot artifact store. Ids are
 * server-minted 32-hex; paths resolve strictly inside the controlled root;
 * files are staged via "<id>.tmp" + atomic rename; load consumes one-shot
 * after SHA-256 re-verification.
 */
#define _POSIX_C_SOURCE 200809L

#include "browser_control_artifacts.h"
#include "hermes_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

const char *const BCA_DEFAULT_ALLOWED_MIME_TYPES[] = {
    "application/json", "application/pdf", "image/gif", "image/jpeg",
    "image/png", "image/webp", "text/plain", NULL,
};

typedef struct {
    bca_receipt_t receipt;
    char path[4096];
} bca_entry_t;

struct bca_store {
    char root[3072];
    double ttl_seconds;
    long max_bytes;
    const char *const *allowed_mime; /* NULL → defaults */
    double (*clock_fn)(void *);
    pthread_mutex_t lock;
    bca_entry_t *entries;
    size_t entries_n, entries_cap;
};

typedef struct {
    char key[256];
    double *hits;
    size_t hits_n, hits_cap;
} bca_hits_t;

struct bca_rate_limiter {
    double window_seconds;
    int max_requests;
    double (*clock_fn)(void *);
    pthread_mutex_t lock;
    bca_hits_t *buckets;
    size_t buckets_n, buckets_cap;
};

static double bca_default_clock(void *ctx) { (void)ctx; return 0.0; }

static double bca_now(bca_store_t *s)
{
    if (s->clock_fn != &bca_default_clock && s->clock_fn)
        return s->clock_fn(NULL);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* PoP: _sha256 @ gateway/browser_control_artifacts.py:_sha256 */
void bca_sha256_hex(const unsigned char *data, size_t len, char out[65])
{
    unsigned char digest[32];
    crypto_sha256(data, len, digest);
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out[64] = '\0';
}

/* PoP: artifact_scope_key @ gateway/browser_control_artifacts.py:artifact_scope_key */
bca_error_t bca_artifact_scope_key(const bca_scope_t *scope, char out[65])
{
    if (!scope || !scope->principal_id || !scope->principal_id[0])
        return BCA_ERR_ARTIFACT; /* fail closed: no resolved principal */
    const char *family =
        (scope->transport_family && scope->transport_family[0])
            ? scope->transport_family : "";
    size_t plen = strlen(scope->principal_id), flen = strlen(family);
    unsigned char *material = malloc(plen + 1 + flen);
    if (!material) return BCA_ERR_ARTIFACT;
    memcpy(material, scope->principal_id, plen);
    material[plen] = '\x00';
    memcpy(material + plen + 1, family, flen);
    bca_sha256_hex(material, plen + 1 + flen, out);
    free(material);
    return BCA_OK;
}

/* PoP: ArtifactReceipt.to_dict @ gateway/browser_control_artifacts.py:ArtifactReceipt.to_dict */
json_t *bca_receipt_to_dict(const bca_receipt_t *r, const char *download_path)
{
    if (!r) return NULL;
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "artifact_id", json_string(r->artifact_id));
    json_set(o, "sha256", json_string(r->sha256));
    json_set(o, "size_bytes", json_int((double)r->size_bytes));
    json_set(o, "content_type", json_string(r->content_type));
    json_set(o, "filename", json_string(r->filename ? r->filename : ""));
    json_set(o, "created_at", json_double(r->created_at));
    json_set(o, "expires_at", json_double(r->expires_at));
    json_set(o, "ttl_seconds", json_double(r->ttl_seconds));
    json_set(o, "one_shot", json_bool(true));
    if (download_path && download_path[0])
        json_set(o, "download_path", json_string(download_path));
    return o;
}

/* PoP: _normalize_content_type @ gateway/browser_control_artifacts.py:_normalize_content_type */
void bca_normalize_content_type(const char *value, char *out, size_t out_sz)
{
    if (!value || !out || out_sz == 0) { if (out && out_sz) out[0] = '\0'; return; }
    /* strip, cut at first ';', strip again, lowercase */
    while (*value == ' ' || *value == '\t' || *value == '\n' ||
           *value == '\r')
        value++;
    size_t n = strlen(value);
    while (n && (value[n - 1] == ' ' || value[n - 1] == '\t' ||
                 value[n - 1] == '\n' || value[n - 1] == '\r'))
        n--;
    for (size_t i = 0; i < n; i++)
        if (value[i] == ';') { n = i; break; }
    while (n && (value[n - 1] == ' ' || value[n - 1] == '\t')) n--;
    if (n >= out_sz) n = out_sz - 1;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)((value[i] >= 'A' && value[i] <= 'Z')
                            ? value[i] - 'A' + 'a'
                            : value[i]);
    out[n] = '\0';
}

/* PoP: _bounded_filename @ gateway/browser_control_artifacts.py:_bounded_filename */
char *bca_bounded_filename(const char *value)
{
    const size_t limit = 160;
    if (!value) return calloc(1, 1);
    size_t start = 0, end = strlen(value);
    while (start < end && (value[start] == ' ' || value[start] == '\t' ||
                           value[start] == '\n' || value[start] == '\r'))
        start++;
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\n' || value[end - 1] == '\r'))
        end--;
    char *out = malloc(end - start + 3);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = start; i < end && o < limit; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '\\') { out[o++] = '_'; continue; }   /* backslash → _ */
        if (c == '/') { out[o++] = '_'; continue; }    /* slash → _ */
        if (c < 32) continue;                          /* drop control chars */
        out[o++] = (char)c;
    }
    out[o] = '\0';
    return out;
}

static int bca_is_valid_id(const char *id)
{
    if (!id || strlen(id) != BCA_ARTIFACT_ID_HEX) return 0;
    for (const char *p = id; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
            return 0;
    return 1;
}

/* PoP: _artifact_path @ gateway/browser_control_artifacts.py:_artifact_path */
static bca_error_t bca_artifact_path(const bca_store_t *store,
                                     const char *artifact_id, char *out,
                                     size_t out_sz)
{
    if (!bca_is_valid_id(artifact_id))
        return BCA_ERR_TRAVERSAL;
    int n = snprintf(out, out_sz, "%s/%s", store->root, artifact_id);
    if (n <= 0 || (size_t)n >= out_sz) return BCA_ERR_TRAVERSAL;
    /* candidate must sit directly inside the root — no traversal. */
    if (strstr(out, "..") || out[strlen(store->root)] != '/')
        return BCA_ERR_TRAVERSAL;
    return BCA_OK;
}

static void bca_free_receipt(bca_receipt_t *r)
{
    if (r) { free(r->filename); r->filename = NULL; }
}

static void bca_copy_receipt(bca_receipt_t *dst, const bca_receipt_t *src)
{
    free(dst->filename);
    *dst = *src;
    dst->filename = src->filename ? strdup(src->filename) : NULL;
}

/* PoP: __init__ @ gateway/browser_control_artifacts.py:ArtifactStore.__init__ */
bca_store_t *bca_store_new(const char *root, double ttl_seconds, long max_bytes,
                           const char *const *allowed_mime,
                           double (*clock_fn)(void *))
{
    if (!root || !root[0]) return NULL;
    bca_store_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    snprintf(s->root, sizeof(s->root), "%s", root);
    mkdir(root, 0755); /* parents best-effort: only the leaf is required */
    s->ttl_seconds = ttl_seconds < 1.0 ? 1.0 : ttl_seconds;
    s->max_bytes = max_bytes < 1 ? 1 : max_bytes;
    s->allowed_mime = allowed_mime; /* NULL → default allowlist */
    s->clock_fn = clock_fn ? clock_fn : &bca_default_clock;
    pthread_mutex_init(&s->lock, NULL);
    bca_store_sweep_orphan_files(s);
    return s;
}

void bca_store_free(bca_store_t *store)
{
    if (!store) return;
    for (size_t i = 0; i < store->entries_n; i++)
        bca_free_receipt(&store->entries[i].receipt);
    free(store->entries);
    pthread_mutex_destroy(&store->lock);
    free(store);
}

const char *bca_store_root(const bca_store_t *store)
{ return store ? store->root : NULL; }

double bca_store_ttl_seconds(const bca_store_t *store)
{ return store ? store->ttl_seconds : 0.0; }

long bca_store_max_bytes(const bca_store_t *store)
{ return store ? store->max_bytes : 0; }

/* PoP: _sweep_orphan_files @ gateway/browser_control_artifacts.py:_sweep_orphan_files */
int bca_store_sweep_orphan_files(bca_store_t *store)
{
    if (!store) return 0;
    DIR *dir = opendir(store->root);
    if (!dir) return 0;
    int removed = 0;
    struct dirent *de;
    pthread_mutex_lock(&store->lock);
    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        size_t len = strlen(name);
        int is_temp = len >= 4 && strcmp(name + len - 4, ".tmp") == 0;
        if (!is_temp && !bca_is_valid_id(name)) continue;
        int live = 0;
        if (!is_temp) {
            for (size_t i = 0; i < store->entries_n; i++)
                if (strcmp(store->entries[i].receipt.artifact_id, name) == 0)
                    { live = 1; break; }
        }
        if (live) continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", store->root, name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (unlink(full) == 0) removed++;
    }
    pthread_mutex_unlock(&store->lock);
    closedir(dir);
    return removed;
}

static int bca_mime_allowed(const bca_store_t *s, const char *mime)
{
    const char *const *list = s->allowed_mime ? s->allowed_mime
                                              : BCA_DEFAULT_ALLOWED_MIME_TYPES;
    for (size_t i = 0; list[i]; i++)
        if (strcmp(list[i], mime) == 0) return 1;
    return 0;
}

/* PoP: store @ gateway/browser_control_artifacts.py:ArtifactStore.store */
bca_error_t bca_store_store(bca_store_t *store, const unsigned char *data,
                            size_t len, const char *filename,
                            const char *content_type, const bca_scope_t *scope,
                            bca_receipt_t *out_receipt)
{
    if (!store || !content_type || !scope) return BCA_ERR_ARTIFACT;
    if (len > (size_t)store->max_bytes)
        return BCA_ERR_TOO_LARGE;
    char normalized[128];
    bca_normalize_content_type(content_type, normalized, sizeof(normalized));
    if (!bca_mime_allowed(store, normalized))
        return BCA_ERR_MIME_REJECTED;
    char scope_key[65];
    bca_error_t err = bca_artifact_scope_key(scope, scope_key);
    if (err != BCA_OK) return err;
    double now = bca_now(store);

    bca_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    pthread_mutex_lock(&store->lock);
    for (;;) {
        /* Mint a fresh id; retry on the astronomically unlikely collision. */
        unsigned char raw[16];
        crypto_random_bytes(raw, sizeof(raw));
        static const char hex[] = "0123456789abcdef";
        for (int i = 0; i < 16; i++) {
            entry.receipt.artifact_id[i * 2] = hex[raw[i] >> 4];
            entry.receipt.artifact_id[i * 2 + 1] = hex[raw[i] & 0xF];
        }
        entry.receipt.artifact_id[32] = '\0';
        if (bca_artifact_path(store, entry.receipt.artifact_id, entry.path,
                              sizeof(entry.path)) != BCA_OK)
            continue;
        int collision = access(entry.path, F_OK) == 0;
        for (size_t i = 0; !collision && i < store->entries_n; i++)
            if (strcmp(store->entries[i].receipt.artifact_id,
                       entry.receipt.artifact_id) == 0)
                collision = 1;
        if (collision) continue;

        snprintf(entry.receipt.sha256, sizeof(entry.receipt.sha256), "%s", "");
        bca_sha256_hex(data ? data : (const unsigned char *)"", len,
                       entry.receipt.sha256);
        entry.receipt.size_bytes = len;
        snprintf(entry.receipt.content_type,
                 sizeof(entry.receipt.content_type), "%s", normalized);
        entry.receipt.filename = bca_bounded_filename(filename);
        entry.receipt.created_at = now;
        entry.receipt.expires_at = now + store->ttl_seconds;
        entry.receipt.ttl_seconds = store->ttl_seconds;
        memcpy(entry.receipt.scope_key, scope_key, 65);

        if (store->entries_n == store->entries_cap) {
            size_t nc = store->entries_cap ? store->entries_cap * 2 : 8;
            bca_entry_t *ne = realloc(store->entries, nc * sizeof(*ne));
            if (!ne) { pthread_mutex_unlock(&store->lock);
                       bca_free_receipt(&entry.receipt); return BCA_ERR_ARTIFACT; }
            store->entries = ne;
            store->entries_cap = nc;
        }
        break;
    }

    /* Stage via temp file, then atomically rename into place so a concurrent
     * reader never observes a partially written artifact. */
    char temp[4200];
    snprintf(temp, sizeof(temp), "%s.tmp", entry.path);
    FILE *fp = fopen(temp, "wb");
    if (!fp) { pthread_mutex_unlock(&store->lock);
               bca_free_receipt(&entry.receipt); return BCA_ERR_ARTIFACT; }
    size_t written = data ? fwrite(data, 1, len, fp) : 0;
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
    if (written != len || rename(temp, entry.path) != 0) {
        unlink(temp);
        for (size_t i = 0; i < store->entries_n; i++)
            if (strcmp(store->entries[i].receipt.artifact_id,
                       entry.receipt.artifact_id) == 0) {
                memmove(&store->entries[i], &store->entries[i + 1],
                        (store->entries_n - i - 1) * sizeof(bca_entry_t));
                store->entries_n--;
                break;
            }
        bca_free_receipt(&entry.receipt);
        pthread_mutex_unlock(&store->lock);
        return BCA_ERR_ARTIFACT;
    }
    store->entries[store->entries_n++] = entry;
    if (out_receipt) bca_copy_receipt(out_receipt, &entry.receipt);
    pthread_mutex_unlock(&store->lock);
    return BCA_OK;
}

/* PoP: _entry_for @ gateway/browser_control_artifacts.py:ArtifactStore._entry_for */
static bca_entry_t *bca_entry_for_locked(bca_store_t *store,
                                         const char *artifact_id,
                                         const bca_scope_t *scope, double now,
                                         bca_error_t *out_err)
{
    char path[4096];
    bca_error_t perr = bca_artifact_path(store, artifact_id, path, sizeof(path));
    if (perr != BCA_OK) { *out_err = perr; return NULL; }
    char scope_key[65];
    if (bca_artifact_scope_key(scope, scope_key) != BCA_OK) {
        *out_err = BCA_ERR_ARTIFACT;
        return NULL;
    }
    bca_entry_t *entry = NULL;
    for (size_t i = 0; i < store->entries_n; i++)
        if (strcmp(store->entries[i].receipt.artifact_id, artifact_id) == 0)
            { entry = &store->entries[i]; break; }
    /* Target's own expiry is checked BEFORE sweeping other entries so an
     * expired artifact surfaces as ArtifactExpired rather than vanishing. */
    if (!entry) {
        /* _prune_expired_locked @ ...:ArtifactStore._prune_expired_locked */
        for (size_t i = 0; i < store->entries_n;) {
            if (store->entries[i].receipt.expires_at <= now) {
                unlink(store->entries[i].path);
                bca_free_receipt(&store->entries[i].receipt);
                memmove(&store->entries[i], &store->entries[i + 1],
                        (store->entries_n - i - 1) * sizeof(bca_entry_t));
                store->entries_n--;
            } else {
                i++;
            }
        }
        for (size_t i = 0; i < store->entries_n; i++)
            if (strcmp(store->entries[i].receipt.artifact_id, artifact_id) == 0)
                { entry = &store->entries[i]; break; }
    }
    if (!entry) { *out_err = BCA_ERR_NOT_FOUND; return NULL; }
    if (entry->receipt.expires_at <= now) {
        unlink(entry->path);
        bca_free_receipt(&entry->receipt);
        ptrdiff_t idx = entry - store->entries;
        memmove(entry, entry + 1,
                (store->entries_n - (size_t)idx - 1) * sizeof(bca_entry_t));
        store->entries_n--;
        *out_err = BCA_ERR_EXPIRED;
        return NULL;
    }
    if (strcmp(entry->receipt.scope_key, scope_key) != 0) {
        *out_err = BCA_ERR_SCOPE_MISMATCH;
        return NULL;
    }
    *out_err = BCA_OK;
    return entry;
}

/* PoP: validate @ gateway/browser_control_artifacts.py:ArtifactStore.validate */
bca_error_t bca_store_validate(bca_store_t *store, const char *artifact_id,
                               const bca_scope_t *scope,
                               bca_receipt_t *out_receipt)
{
    if (!store) return BCA_ERR_ARTIFACT;
    pthread_mutex_lock(&store->lock);
    bca_error_t err;
    bca_entry_t *e = bca_entry_for_locked(store, artifact_id, scope,
                                          bca_now(store), &err);
    if (e && out_receipt) bca_copy_receipt(out_receipt, &e->receipt);
    pthread_mutex_unlock(&store->lock);
    return err;
}

/* PoP: load @ gateway/browser_control_artifacts.py:ArtifactStore.load */
bca_error_t bca_store_load(bca_store_t *store, const char *artifact_id,
                           const bca_scope_t *scope, unsigned char **out_data,
                           size_t *out_len, bca_receipt_t *out_receipt)
{
    if (!store || !out_data || !out_len) return BCA_ERR_ARTIFACT;
    *out_data = NULL;
    *out_len = 0;
    pthread_mutex_lock(&store->lock);
    double now = bca_now(store);
    bca_error_t err;
    bca_entry_t *e = bca_entry_for_locked(store, artifact_id, scope, now, &err);
    if (!e) { pthread_mutex_unlock(&store->lock); return err; }
    FILE *fp = fopen(e->path, "rb");
    if (!fp) {
        /* Gone from disk: drop the index entry and fail closed. */
        bca_free_receipt(&e->receipt);
        ptrdiff_t idx = e - store->entries;
        memmove(e, e + 1,
                (store->entries_n - (size_t)idx - 1) * sizeof(bca_entry_t));
        store->entries_n--;
        pthread_mutex_unlock(&store->lock);
        return BCA_ERR_NOT_FOUND;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *data = malloc(sz > 0 ? (size_t)sz : 1);
    if (!data) { fclose(fp); pthread_mutex_unlock(&store->lock);
                 return BCA_ERR_ARTIFACT; }
    size_t rd = fread(data, 1, sz > 0 ? (size_t)sz : 0, fp);
    fclose(fp);
    char digest[65];
    bca_sha256_hex(data, rd, digest);
    if (strcmp(digest, e->receipt.sha256) != 0) {
        free(data);
        pthread_mutex_unlock(&store->lock);
        return BCA_ERR_CHECKSUM;
    }
    bca_receipt_t receipt;
    memset(&receipt, 0, sizeof(receipt));
    bca_copy_receipt(&receipt, &e->receipt);
    /* Consume atomically: index first (concurrent load fails closed). */
    bca_free_receipt(&e->receipt);
    ptrdiff_t idx = e - store->entries;
    memmove(e, e + 1, (store->entries_n - (size_t)idx - 1) * sizeof(bca_entry_t));
    store->entries_n--;
    pthread_mutex_unlock(&store->lock);
    {
        /* Remove the artifact file; the TTL sweep retries on failure. */
        char path[4096];
        if (bca_artifact_path(store, artifact_id, path, sizeof(path)) == BCA_OK)
            unlink(path);
    }
    *out_data = data;
    *out_len = rd;
    if (out_receipt) { bca_copy_receipt(out_receipt, &receipt); }
    bca_free_receipt(&receipt);
    return BCA_OK;
}

/* PoP: prune_expired @ gateway/browser_control_artifacts.py:ArtifactStore.prune_expired */
int bca_store_prune_expired(bca_store_t *store, const double *now_in)
{
    if (!store) return 0;
    double now = now_in ? *now_in : bca_now(store);
    int removed = 0;
    pthread_mutex_lock(&store->lock);
    for (size_t i = 0; i < store->entries_n;) {
        if (store->entries[i].receipt.expires_at <= now) {
            unlink(store->entries[i].path);
            bca_free_receipt(&store->entries[i].receipt);
            memmove(&store->entries[i], &store->entries[i + 1],
                    (store->entries_n - i - 1) * sizeof(bca_entry_t));
            store->entries_n--;
            removed++;
        } else {
            i++;
        }
    }
    /* Orphaned temp files older than one sweep. */
    DIR *dir = opendir(store->root);
    if (dir) {
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            size_t len = strlen(de->d_name);
            if (len < 4 || strcmp(de->d_name + len - 4, ".tmp") != 0) continue;
            char full[4096];
            snprintf(full, sizeof(full), "%s/%s", store->root, de->d_name);
            struct stat st;
            if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            if ((double)st.st_mtime <= now - store->ttl_seconds)
                unlink(full);
        }
        closedir(dir);
    }
    pthread_mutex_unlock(&store->lock);
    return removed;
}

/* PoP: count @ gateway/browser_control_artifacts.py:ArtifactStore.count */
int bca_store_count(const bca_store_t *store)
{
    if (!store) return 0;
    return (int)store->entries_n;
}

/* ------------------------------------------------------------------ */
/* Rate limiter                                                        */
/* ------------------------------------------------------------------ */

/* PoP: __init__ @ gateway/browser_control_artifacts.py:ArtifactRateLimiter.__init__ */
bca_rate_limiter_t *bca_rate_limiter_new(double window_seconds, int max_requests,
                                         double (*clock_fn)(void *))
{
    bca_rate_limiter_t *lim = calloc(1, sizeof(*lim));
    if (!lim) return NULL;
    lim->window_seconds = window_seconds < 1.0 ? 1.0 : window_seconds;
    lim->max_requests = max_requests < 1 ? 1 : max_requests;
    lim->clock_fn = clock_fn ? clock_fn : &bca_default_clock;
    pthread_mutex_init(&lim->lock, NULL);
    return lim;
}

void bca_rate_limiter_free(bca_rate_limiter_t *lim)
{
    if (!lim) return;
    for (size_t i = 0; i < lim->buckets_n; i++) free(lim->buckets[i].hits);
    free(lim->buckets);
    pthread_mutex_destroy(&lim->lock);
    free(lim);
}

static double rl_now(bca_rate_limiter_t *lim)
{
    if (lim->clock_fn != &bca_default_clock && lim->clock_fn)
        return lim->clock_fn(NULL);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static bca_hits_t *rl_bucket(bca_rate_limiter_t *lim, const char *key,
                             int create)
{
    for (size_t i = 0; i < lim->buckets_n; i++)
        if (strcmp(lim->buckets[i].key, key) == 0)
            return &lim->buckets[i];
    if (!create) return NULL;
    if (lim->buckets_n == lim->buckets_cap) {
        size_t nc = lim->buckets_cap ? lim->buckets_cap * 2 : 8;
        bca_hits_t *nb = realloc(lim->buckets, nc * sizeof(*nb));
        if (!nb) return NULL;
        lim->buckets = nb;
        lim->buckets_cap = nc;
    }
    bca_hits_t *b = &lim->buckets[lim->buckets_n++];
    snprintf(b->key, sizeof(b->key), "%s", key);
    b->hits = NULL;
    b->hits_n = b->hits_cap = 0;
    return b;
}

/* PoP: allow @ gateway/browser_control_artifacts.py:ArtifactRateLimiter.allow */
int bca_rate_limiter_allow(bca_rate_limiter_t *lim, const char *key)
{
    if (!lim || !key || !key[0]) return 0;
    double now = rl_now(lim);
    double window_start = now - lim->window_seconds;
    pthread_mutex_lock(&lim->lock);
    bca_hits_t *b = rl_bucket(lim, key, 1);
    if (!b) { pthread_mutex_unlock(&lim->lock); return 0; }
    size_t w = 0;
    for (size_t r = 0; r < b->hits_n; r++)
        if (b->hits[r] > window_start) b->hits[w++] = b->hits[r];
    b->hits_n = w;
    if ((long)w >= lim->max_requests) {
        pthread_mutex_unlock(&lim->lock);
        return 0;
    }
    if (w == b->hits_cap) {
        size_t nc = b->hits_cap ? b->hits_cap * 2 : 8;
        double *nh = realloc(b->hits, nc * sizeof(double));
        if (!nh) { pthread_mutex_unlock(&lim->lock); return 0; }
        b->hits = nh;
        b->hits_cap = nc;
    }
    b->hits[w++] = now;
    b->hits_n = w;
    pthread_mutex_unlock(&lim->lock);
    return 1;
}

/* PoP: reset @ gateway/browser_control_artifacts.py:ArtifactRateLimiter.reset */
void bca_rate_limiter_reset(bca_rate_limiter_t *lim, const char *key)
{
    if (!lim || !key) return;
    pthread_mutex_lock(&lim->lock);
    for (size_t i = 0; i < lim->buckets_n; i++) {
        if (strcmp(lim->buckets[i].key, key) == 0) {
            free(lim->buckets[i].hits);
            memmove(&lim->buckets[i], &lim->buckets[i + 1],
                    (lim->buckets_n - i - 1) * sizeof(bca_hits_t));
            lim->buckets_n--;
            break;
        }
    }
    pthread_mutex_unlock(&lim->lock);
}
