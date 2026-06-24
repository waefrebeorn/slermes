/*
 * port_tools_browser_camofox_state.c — C port of tools/browser_camofox_state.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_browser_camofox_state_get_camofox_identity @ tools/browser_camofox_state.py:get_camofox_identity */

/*
 * Camofox identity result.
 */
typedef struct {
    char user_id[32];
    char session_key[32];
} camofox_identity_t;

/*
 * get_camofox_identity: Return stable Hermes-managed Camofox identity.
 *
 * Generates deterministic user_id and session_key from scope_root and task_id
 * using hash-based UUID-like generation.
 *
 * p1 = task_id string (NULL for "default")
 * p2 = pointer to camofox_identity_t for result
 *
 * Returns: pointer to camofox_identity_t.
 */
void* cli_tools_browser_camofox_state_get_camofox_identity(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *task_id = (const char *)p1;
    camofox_identity_t *identity = (camofox_identity_t *)p2;

    if (!identity) return NULL;
    if (!task_id) task_id = "default";

    /* Generate deterministic hashes from scope_root and task_id.
     * In Python this uses uuid.uuid5(NAMESPACE_URL, ...).
     * In C we use a simple but deterministic hash. */
    const char *scope_root = NULL;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    char scope_buf[512];
    snprintf(scope_buf, sizeof(scope_buf), "%s/.hermes/browser_auth/camofox", home);
    scope_root = scope_buf;

    /* Hash for user_id: camofox-user:{scope_root} */
    unsigned long user_hash = 5381;
    const char *user_prefix = "camofox-user:";
    for (const char *c = user_prefix; *c; c++)
        user_hash = ((user_hash << 5) + user_hash) + *c;
    for (const char *c = scope_root; *c; c++)
        user_hash = ((user_hash << 5) + user_hash) + *c;

    /* Hash for session_key: camofox-session:{scope_root}:{task_id} */
    unsigned long sess_hash = 5381;
    const char *sess_prefix = "camofox-session:";
    for (const char *c = sess_prefix; *c; c++)
        sess_hash = ((sess_hash << 5) + sess_hash) + *c;
    for (const char *c = scope_root; *c; c++)
        sess_hash = ((sess_hash << 5) + sess_hash) + *c;
    sess_hash = ((sess_hash << 5) + sess_hash) + ':';
    for (const char *c = task_id; *c; c++)
        sess_hash = ((sess_hash << 5) + sess_hash) + *c;

    snprintf(identity->user_id, sizeof(identity->user_id),
             "hermes_%010lx", user_hash & 0x3FFFFFFFFFF);
    snprintf(identity->session_key, sizeof(identity->session_key),
             "task_%016lx", sess_hash & 0xFFFFFFFFFFFF);

    hermes_log(LOG_DEBUG, "port",
               "camofox_identity: user_id=%s, session_key=%s (task=%s)",
               identity->user_id, identity->session_key, task_id);

    return identity;
}
