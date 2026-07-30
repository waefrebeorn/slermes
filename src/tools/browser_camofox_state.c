/* browser_camofox_state.c
 *
 * Faithful C port of tools/browser_camofox_state.py.
 *
 * Provides the stable Hermes-managed Camofox identity for a profile: a
 * user_id (profile-scoped) and a session_key (task-scoped). Mirrors the
 * Python uuid5(NAMESPACE_URL, ...) digests.
 */

/* PoP: browser_camofox_state_get_camofox_identity @ tools/browser_camofox_state.py:get_camofox_identity */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>

/* Resolve the profile-scoped Camofox persistence root.
 * Returns heap-allocated "<HERMES_HOME>/browser_auth/camofox" (caller frees),
 * or NULL on failure. Mirrors get_camofox_state_dir(). */
/* PoP: _state_dir @ hermes_cli/active_sessions.py:_state_dir */
static char *camofox_state_dir(void)
{
    char home[4096];
    const char *env = getenv("HERMES_HOME");
    if (env && env[0]) {
        snprintf(home, sizeof(home), "%s", env);
    } else {
        const char *h = getenv("HOME");
        if (!h || !h[0]) return NULL;
        snprintf(home, sizeof(home), "%s/.hermes", h);
    }
    size_t len = strlen(home) + 32;
    char *out = malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s/browser_auth/camofox", home);
    return out;
}

/*
 * Compute a uuid5-style digest: SHA1(namespace + ':' + name), take .hex[:n].
 * Returns a heap-allocated string (caller frees).
 */
static char *uuid5_hex(const char *ns, const char *name, size_t nchars)
{
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s:%s", ns, name);
    unsigned char md[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)buf, strlen(buf), md);
    static const char *hexd = "0123456789abcdef";
    char *out = malloc(nchars + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < nchars; i++) {
        out[i] = hexd[(md[i >> 1] >> ((i & 1) ? 0 : 4)) & 0xf];
    }
    out[nchars] = '\0';
    return out;
}

/* Port of get_camofox_identity(task_id=None).
 * Returns a heap-allocated JSON object {"user_id":..., "session_key":...}
 * (caller frees). */
char *browser_camofox_state_get_camofox_identity(const char *task_id)
{
    char *scope_root = camofox_state_dir();
    if (!scope_root) return strdup("{\"user_id\":\"hermes_unknown\",\"session_key\":\"task_unknown\"}");
    const char *logical_scope = (task_id && task_id[0]) ? task_id : "default";

    char user_ns[2048];
    snprintf(user_ns, sizeof(user_ns), "camofox-user:%s", scope_root);
    char *user_digest = uuid5_hex("urn:uuid:6ba7b811-9dad-11d1-80b4-00c04fd430c8", user_ns, 10);

    char session_ns[2048];
    snprintf(session_ns, sizeof(session_ns), "camofox-session:%s:%s", scope_root, logical_scope);
    char *session_digest = uuid5_hex("urn:uuid:6ba7b811-9dad-11d1-80b4-00c04fd430c8", session_ns, 16);

    free(scope_root);

    char *result = malloc(strlen(user_digest) + strlen(session_digest) + 64);
    if (result) {
        snprintf(result, strlen(user_digest) + strlen(session_digest) + 64,
                 "{\"user_id\":\"hermes_%s\",\"session_key\":\"task_%s\"}",
                 user_digest, session_digest);
    }
    free(user_digest);
    free(session_digest);
    return result;
}
