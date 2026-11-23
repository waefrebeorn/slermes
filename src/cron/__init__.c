/*
 * __init__.c — faithful C11 port of the package surface of cron/__init__.py.
 *
 * PoP: w5_init_7fb779_all @ cron/__init__.py:__all__
 * PoP: w5_init_7fb779_exports @ cron/__init__.py:from cron.jobs / cron.scheduler
 * PoP: w5_init_7fb779_has @ cron/__init__.py:__all__
 *
 * The upstream module's only stable, side-effect-free behaviour is its
 * bootstrap call and its re-export surface; the worker boot itself is an
 * import-time side effect owned by cron/worker_bootstrap.py.
 */
#include "noc_w5_init_7fb779.h"
#include "hermes_json.h"
#include <string.h>

/* __all__ in the exact order declared upstream. */
static const char *const W5I_ALL[] = {
    "JOBS_FILE", "create_job", "get_job", "list_jobs", "pause_job",
    "rearm_oneshot", "remove_job", "resume_job", "tick", "trigger_job",
    "update_job", NULL
};

struct json_t *w5_init_7fb779_all(void) {
    struct json_t *out = json_array();
    for (int i = 0; W5I_ALL[i]; i++) json_append(out, json_string(W5I_ALL[i]));
    return out;
}

struct json_t *w5_init_7fb779_exports(void) {
    struct json_t *out = json_object();
    for (int i = 0; W5I_ALL[i]; i++) {
        const char *src = !strcmp(W5I_ALL[i], "tick") ? "cron.scheduler" : "cron.jobs";
        json_set(out, W5I_ALL[i], json_string(src));
    }
    return out;
}

struct json_t *w5_init_7fb779_has(const char *name) {
    int found = 0;
    if (name) for (int i = 0; W5I_ALL[i]; i++) if (!strcmp(W5I_ALL[i], name)) { found = 1; break; }
    return json_bool(found ? true : false);
}

