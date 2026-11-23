/*
 * artifact_mirror.c — C port of upstream Hermes pm/artifact_mirror.py.
 *
 * PoP: artmirror_object_key @ pm/artifact_mirror.py:object_key
 * PoP: artmirror_mirror_url @ pm/artifact_mirror.py:mirror_url
 * PoP: artmirror_pinned_source @ pm/artifact_mirror.py:pinned_source
 *
 * KEY_PREFIX/PUBLIC_PREFIX mirror pm/artifact-mirror.json. npm_registry* and
 * Source are injected at the gate (they are environment/IO-bound upstream);
 * the decision logic of pinned_source is reproduced faithfully here.
 */
#include "noc_artmirror.h"
#include <stdlib.h>
#include <string.h>

static int valid_sha256(const char *s) {
    if (!s || strlen(s) != 64) return 0;
    for (int i = 0; i < 64; i++)
        if (!((s[i] >= 'a' && s[i] <= 'f') || (s[i] >= '0' && s[i] <= '9'))) return 0;
    return 1;
}

int artmirror_object_key(const char *sha256, char **out_key) {
    if (!valid_sha256(sha256)) return 0;
    size_t n = strlen(NOC_ARTMIRROR_KEY_PREFIX) + 64;
    char *k = (char *)malloc(n + 1);
    if (!k) return 0;
    memcpy(k, NOC_ARTMIRROR_KEY_PREFIX, strlen(NOC_ARTMIRROR_KEY_PREFIX));
    memcpy(k + strlen(NOC_ARTMIRROR_KEY_PREFIX), sha256, 64);
    k[n] = '\0';
    *out_key = k;
    return 1;
}

int artmirror_mirror_url(const char *sha256, char **out_url) {
    char *key = NULL;
    if (!artmirror_object_key(sha256, &key)) return 0;
    size_t n = strlen(NOC_ARTMIRROR_ORIGIN) + 1 + strlen(key);
    char *u = (char *)malloc(n + 1);
    if (!u) { free(key); return 0; }
    memcpy(u, NOC_ARTMIRROR_ORIGIN, strlen(NOC_ARTMIRROR_ORIGIN));
    u[strlen(NOC_ARTMIRROR_ORIGIN)] = '/';
    memcpy(u + strlen(NOC_ARTMIRROR_ORIGIN) + 1, key, strlen(key));
    u[n] = '\0';
    free(key);
    *out_url = u;
    return 1;
}

/* Python:
 *   archive = mirror_url(sha256)                    # validates sha256
 *   mirrored = npm_registry_url(url, os.environ)    # injected
 *   registry = npm_registry(os.environ) if mirrored != url else ""
 *   fallbacks = () if mirrored == archive else (archive,)
 *   http_mirror = registry if registry.startswith("http://") else ""
 *   return Source(mirrored, dest, sha256, fallbacks=..., http_mirror=...) */
int artmirror_pinned_source(const char *sha256, const char *dest, const char *url,
                            const char *mirrored, const char *registry,
                            artmirror_source_t *out) {
    memset(out, 0, sizeof(*out));
    char *archive = NULL;
    if (!artmirror_mirror_url(sha256, &archive)) return 0;
    const char *mir = mirrored ? mirrored : "";
    const char *orig = url ? url : "";
    const char *reg = (!strcmp(mir, orig)) ? "" : (registry ? registry : "");

    out->url = strdup(mir);
    out->dest = strdup(dest ? dest : "");
    out->sha256 = strdup(sha256);
    out->http_mirror = (strncmp(reg, "http://", 7) == 0) ? strdup(reg) : strdup("");
    if (strcmp(mir, archive) == 0) {
        out->nfallbacks = 0;
        out->fallbacks = NULL;
    } else {
        out->nfallbacks = 1;
        out->fallbacks = (char **)malloc(sizeof(char *));
        out->fallbacks[0] = strdup(archive);
    }
    free(archive);
    return 1;
}

void artmirror_source_free(artmirror_source_t *s) {
    if (!s) return;
    free(s->url); free(s->dest); free(s->sha256); free(s->http_mirror);
    for (size_t i = 0; i < s->nfallbacks; i++) free(s->fallbacks[i]);
    free(s->fallbacks);
    memset(s, 0, sizeof(*s));
}
