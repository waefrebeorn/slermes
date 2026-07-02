/*
 * pet_manifest.c — Fetch the public petdex manifest
 *
 * Port of Python: agent/pet/manifest.py
 * Fetches https://petdex.dev/api/manifest, caches in-process.
 * Uses libhttp for HTTP and libjson for JSON parsing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "pet.h"
#include "hermes.h"
#include "hermes_logger.h"

/* ── In-process cache ───────────────────────────────────────────────── */
static pet_manifest_cache_t g_manifest_cache = {NULL, 0, 0.0};
static pthread_mutex_t g_manifest_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_prefetching = false;

/* ════════════════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════════════════ */

static double now_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9;
}

/* PoP: cache_is_warm @ agent/pet/manifest.py:_cache_is_warm */
static bool cache_is_warm(void) {
    if (!g_manifest_cache.entries || g_manifest_cache.count <= 0)
        return false;
    return (now_monotonic() - g_manifest_cache.fetched_at) < PET_MANIFEST_TTL;
}

/* PoP: pet_clear_manifest_cache @ agent/pet/manifest.py:clear_cache */
void pet_clear_manifest_cache(void) {
    pthread_mutex_lock(&g_manifest_lock);
    if (g_manifest_cache.entries) {
        free(g_manifest_cache.entries);
        g_manifest_cache.entries = NULL;
    }
    g_manifest_cache.count = 0;
    g_manifest_cache.fetched_at = 0.0;
    pthread_mutex_unlock(&g_manifest_lock);
}

/* PoP: pet_fetch_manifest @ agent/pet/manifest.py:fetch_manifest */
int pet_fetch_manifest(pet_manifest_entry_t *out, int max_count, bool force) {
    if (!out || max_count <= 0) return 0;

    pthread_mutex_lock(&g_manifest_lock);
    /* Return cached if warm and not forced */
    if (!force && cache_is_warm()) {
        int n = (g_manifest_cache.count < max_count) ? g_manifest_cache.count : max_count;
        for (int i = 0; i < n; i++)
            out[i] = g_manifest_cache.entries[i];
        pthread_mutex_unlock(&g_manifest_lock);
        return n;
    }
    pthread_mutex_unlock(&g_manifest_lock);

    /* Fetch from network using libhttp */
    http_t *client = http_new(30);
    if (!client) return 0;

    http_resp_t *resp = http_get(client, PET_MANIFEST_URL, NULL);
    if (!resp || resp->status != 200 || !resp->body) {
        hermes_log(LOG_WARNING, "pet", "manifest fetch failed: HTTP %d",
                   resp ? resp->status : -1);
        if (resp) http_resp_free(resp);
        http_free(client);
        return 0;
    }

    /* Parse JSON */
    json_t *root = json_parse(resp->body, NULL);
    char *body_copy = strdup(resp->body); /* keep copy for later if needed */
    http_resp_free(resp);
    http_free(client);

    if (!root) {
        hermes_log(LOG_WARNING, "pet", "manifest parse failed: invalid JSON");
        free(body_copy);
        return 0;
    }

    json_t *pets = json_obj_get(root, "pets");
    if (!pets || pets->type != JSON_ARRAY) {
        hermes_log(LOG_WARNING, "pet", "manifest has no 'pets' array");
        json_free(root);
        free(body_copy);
        return 0;
    }

    int count = json_len(pets);
    if (count > max_count) count = max_count;

    for (int i = 0; i < count; i++) {
        json_t *entry = json_get(pets, i);
        if (!entry) continue;

        const char *slug = json_node_get_string(json_obj_get(entry, "slug"));
        const char *display_name = json_node_get_string(json_obj_get(entry, "displayName"));
        const char *kind = json_node_get_string(json_obj_get(entry, "kind"));
        const char *submitted_by = json_node_get_string(json_obj_get(entry, "submittedBy"));
        const char *spritesheet_url = json_node_get_string(json_obj_get(entry, "spritesheetUrl"));
        const char *pet_json_url = json_node_get_string(json_obj_get(entry, "petJsonUrl"));
        const char *zip_url = json_node_get_string(json_obj_get(entry, "zipUrl"));

        if (!slug || !*slug) continue;
        if (!spritesheet_url || !*spritesheet_url) continue;

        /* Anti-SSRF: only petdex hosts */
        if (strstr(spritesheet_url, "petdex.dev") == NULL &&
            strstr(spritesheet_url, "assets.petdex.dev") == NULL) {
            continue;
        }

        snprintf(out[i].slug, sizeof(out[i].slug), "%s", slug);
        snprintf(out[i].display_name, sizeof(out[i].display_name), "%s",
                 display_name ? display_name : slug);
        snprintf(out[i].kind, sizeof(out[i].kind), "%s", kind ? kind : "pet");
        snprintf(out[i].submitted_by, sizeof(out[i].submitted_by), "%s",
                 submitted_by ? submitted_by : "");
        snprintf(out[i].spritesheet_url, sizeof(out[i].spritesheet_url), "%s", spritesheet_url);
        snprintf(out[i].pet_json_url, sizeof(out[i].pet_json_url), "%s",
                 pet_json_url ? pet_json_url : "");
        snprintf(out[i].zip_url, sizeof(out[i].zip_url), "%s", zip_url ? zip_url : "");
    }

    /* Update cache */
    pthread_mutex_lock(&g_manifest_lock);
    if (g_manifest_cache.entries) {
        free(g_manifest_cache.entries);
        g_manifest_cache.entries = NULL;
    }
    g_manifest_cache.entries = malloc(sizeof(pet_manifest_entry_t) * count);
    if (g_manifest_cache.entries) {
        for (int i = 0; i < count; i++)
            g_manifest_cache.entries[i] = out[i];
        g_manifest_cache.count = count;
        g_manifest_cache.fetched_at = now_monotonic();
    }
    pthread_mutex_unlock(&g_manifest_lock);

    json_free(root);
    free(body_copy);
    return count;
}

/* PoP: pet_find_entry @ agent/pet/manifest.py:find_entry */
bool pet_find_entry(const char *slug, pet_manifest_entry_t *out) {
    if (!slug || !*slug || !out) return false;

    pet_manifest_entry_t entries[128];
    int count = pet_fetch_manifest(entries, 128, false);
    if (count <= 0) return false;

    /* Build lowercase slug for comparison */
    char slug_lower[PET_MAX_SLUG];
    int si = 0;
    for (const char *p = slug; *p && si < (int)sizeof(slug_lower) - 1; p++, si++)
        slug_lower[si] = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
    slug_lower[si] = '\0';

    for (int i = 0; i < count; i++) {
        const char *es = entries[i].slug;
        bool match = true;
        for (const char *p = es, *q = slug_lower; ; p++, q++) {
            char ec = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
            if (ec != *q) { match = false; break; }
            if (*p == '\0') break;
        }
        if (match) {
            *out = entries[i];
            return true;
        }
    }
    return false;
}

/* PoP: pet_prefetch_manifest @ agent/pet/manifest.py:prefetch */
void pet_prefetch_manifest(void) {
    if (cache_is_warm()) return;

    pthread_mutex_lock(&g_manifest_lock);
    if (g_prefetching) {
        pthread_mutex_unlock(&g_manifest_lock);
        return;
    }
    g_prefetching = true;
    pthread_mutex_unlock(&g_manifest_lock);

    pet_manifest_entry_t entries[128];
    int count = pet_fetch_manifest(entries, 128, false);
    if (count <= 0) {
        hermes_log(LOG_DEBUG, "pet", "manifest prefetch returned %d entries", count);
    }

    pthread_mutex_lock(&g_manifest_lock);
    g_prefetching = false;
    pthread_mutex_unlock(&g_manifest_lock);
}
