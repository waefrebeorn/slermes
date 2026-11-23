#ifndef NOC_ARTMIRROR_H
#define NOC_ARTMIRROR_H
#include <stddef.h>

/* Constants from pm/artifact-mirror.json (KEY_PREFIX / origin). */
#define NOC_ARTMIRROR_KEY_PREFIX    "upstream/sha256/"
#define NOC_ARTMIRROR_ORIGIN        "https://hermes-assets.nousresearch.com"
#define NOC_ARTMIRROR_PUBLIC_PREFIX "https://hermes-assets.nousresearch.com/upstream/sha256/"

/* PoP: artmirror_object_key @ pm/artifact_mirror.py:object_key
 * Returns 1 and a malloc'd key for a full lowercase 64-hex SHA256, else 0. */
int artmirror_object_key(const char *sha256, char **out_key);
/* PoP: artmirror_mirror_url @ pm/artifact_mirror.py:mirror_url
 * Validates as object_key, then returns PUBLIC_PREFIX + sha256. */
int artmirror_mirror_url(const char *sha256, char **out_url);

typedef struct {
    char *url, *dest, *sha256, *http_mirror;
    char **fallbacks;
    size_t nfallbacks;
} artmirror_source_t;

/* PoP: artmirror_pinned_source @ pm/artifact_mirror.py:pinned_source
 * url/dest/sha256 are the arguments; mirrored is npm_registry_url(url, env)
 * and registry is npm_registry(env), both injected by the gate. Returns 0 on
 * an invalid sha256 (Python raises ValueError). */
int artmirror_pinned_source(const char *sha256, const char *dest, const char *url,
                            const char *mirrored, const char *registry,
                            artmirror_source_t *out);
void artmirror_source_free(artmirror_source_t *s);

#endif /* NOC_ARTMIRROR_H */
