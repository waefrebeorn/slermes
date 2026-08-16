#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/monitoring/policy.c — C11 port of agent/monitoring/policy.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/monitoring/port_agent_monitoring_policy.c ── */
/*
 * port_agent_monitoring_policy.c — C11 port of agent/monitoring/policy.py
 *
 * ensure_install_id: return a stable install id, minting and persisting one
 * when empty. The id must survive gateway restarts (it becomes
 * service.instance.id on exported signals), so a freshly minted UUID is
 * written back to config.yaml immediately. The write is fail-open: if
 * persisting fails (read-only home, managed scope), the ephemeral id is still
 * returned and a new one is minted next start.
 *
 * Clearing monitoring.install_id (e.g. `hermes config set
 * monitoring.install_id ""`) rotates the id on the next gateway start.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "json.h"
#include "libuuid/uuid.h"              /* uuid_v4 */
#include "port_config_py_helpers.h"    /* config_py_load_config_readonly,
                                          config_py_save_value */

/* PoP: config @ agent/monitoring/policy.py:ensure_install_id */
/* Return a malloc'd stable install id. If config (a json object) already has a
 * non-blank monitoring.install_id string, return it. Otherwise mint uuid4,
 * persist it (fail-open), and update the in-memory config. Caller frees. */
char *monitoring_ensure_install_id(json_t *config)
{
    /* Existing non-blank id in the passed-in config? */
    if (config && config->type == JSON_OBJECT) {
        json_t *mon = json_obj_get(config, "monitoring");
        if (mon && mon->type == JSON_OBJECT) {
            json_t *id = json_obj_get(mon, "install_id");
            if (id && id->type == JSON_STRING && id->str_val[0]) {
                const char *s = id->str_val;
                while (*s == ' ' || *s == '\t') s++;
                if (*s) return strdup(id->str_val);
            }
        }
    }

    char *minted = uuid_v4();          /* str(uuid.uuid4()) — 36 chars, hyphens */
    if (!minted) minted = strdup("00000000-0000-0000-0000-000000000000");
    if (!minted) return NULL;

    /* Fail-open persist: reload fresh, set slot if still empty, save. */
    int persisted = 0;
    json_t *fresh = config_py_load_config_readonly();
    if (fresh && fresh->type == JSON_OBJECT) {
        json_t *slot = json_obj_get(fresh, "monitoring");
        int slot_empty = 1;
        if (slot && slot->type == JSON_OBJECT) {
            json_t *id = json_obj_get(slot, "install_id");
            if (id && id->type == JSON_STRING) {
                const char *s = id->str_val;
                while (*s == ' ' || *s == '\t') s++;
                if (*s) slot_empty = 0;
            }
        }
        if (slot_empty) {
            /* ensure monitoring is a dict, then set install_id */
            json_t *mon = json_obj_get(fresh, "monitoring");
            if (!mon || mon->type != JSON_OBJECT) {
                mon = json_object();
                json_set(fresh, "monitoring", mon);
            }
            json_set(mon, "install_id", json_string(minted));
            persisted = (config_py_save_value("monitoring.install_id", json_string(minted)) == 0);
            (void)persisted;  /* fail-open: ignore errors */
        }
    }
    if (fresh) json_free(fresh);

    /* Keep the in-memory config consistent for this process either way. */
    if (config && config->type == JSON_OBJECT) {
        json_t *mon = json_obj_get(config, "monitoring");
        if (!mon || mon->type != JSON_OBJECT) {
            mon = json_object();
            json_set(config, "monitoring", mon);
        }
        json_set(mon, "install_id", json_string(minted));
    }
    return minted;
}

