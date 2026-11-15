/*
 * port_left_core_migration.c -- see port_left_core_migration.h.
 */

#include "port_left_core_migration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* utils.base_url_host_matches, already ported. */
extern int provider_base_url_host_matches(const char *base_url, const char *domain);

/* ===========================================================================
 *  _toolset_listed  (left_core_migration.py:_toolset_listed)
 *
 *      rows = config.get("platform_toolsets")
 *      selections = [sel for platform, sel in rows.items()
 *                   if platform != own_platform] if isinstance(rows, dict) else []
 *      selections.append(config.get("toolsets"))
 *      return any(isinstance(sel, list) and any(str(item) in names for item in sel)
 *                 for sel in selections)
 * =========================================================================== */

/* PoP: lcm_toolset_listed @ hermes_cli/left_core_migration.py:_toolset_listed */
int lcm_toolset_listed(const json_t *config, const char *const *names, size_t n_names,
                       const char *own_platform)
{
    if (!config || config->type != JSON_OBJECT)
        return 0;

    json_t *rows = json_obj_get(config, "platform_toolsets");
    if (rows && rows->type == JSON_OBJECT) {
        for (size_t i = 0; i < rows->c.count; i++) {
            if (own_platform && rows->c.keys[i]
                && strcmp(rows->c.keys[i], own_platform) == 0)
                continue;                       /* own platform's row scopes itself */
            json_t *sel = rows->c.items[i];
            if (!sel || sel->type != JSON_ARRAY)
                continue;
            for (size_t j = 0; j < sel->c.count; j++) {
                const json_t *item = sel->c.items[j];
                char buf[128];
                const char *s = NULL;
                if (item && item->type == JSON_STRING && item->str_val)
                    s = item->str_val;
                else if (item && item->type == JSON_NUMBER) {
                    snprintf(buf, sizeof(buf), "%.17g", item->num_val);
                    s = buf;
                }
                if (!s)
                    continue;
                for (size_t k = 0; k < n_names; k++) {
                    if (names[k] && strcmp(s, names[k]) == 0)
                        return 1;
                }
            }
        }
    }

    json_t *top = json_obj_get(config, "toolsets");
    if (top && top->type == JSON_ARRAY) {
        for (size_t j = 0; j < top->c.count; j++) {
            const json_t *item = top->c.items[j];
            char buf[128];
            const char *s = NULL;
            if (item && item->type == JSON_STRING && item->str_val)
                s = item->str_val;
            else if (item && item->type == JSON_NUMBER) {
                snprintf(buf, sizeof(buf), "%.17g", item->num_val);
                s = buf;
            }
            if (!s)
                continue;
            for (size_t k = 0; k < n_names; k++) {
                if (names[k] && strcmp(s, names[k]) == 0)
                    return 1;
            }
        }
    }
    return 0;
}

/* ===========================================================================
 *  _delegation_backend_routes  (left_core_migration.py:_delegation_backend_routes)
 *
 *      backends = delegation.get("backends")
 *      blocks = [b for b in backends.values() if isinstance(b, dict)] \
 *               if isinstance(backends, dict) else []
 *      return [o for b in blocks if isinstance(b.get("options"), list)
 *              for o in b["options"] if isinstance(o, dict)]
 * =========================================================================== */

/* PoP: lcm_delegation_backend_routes @ hermes_cli/left_core_migration.py:_delegation_backend_routes */
json_t *lcm_delegation_backend_routes(const json_t *delegation)
{
    json_t *out = json_array();
    if (!out)
        return NULL;
    json_t *backends = (delegation && delegation->type == JSON_OBJECT)
                           ? json_obj_get(delegation, "backends") : NULL;
    if (!backends || backends->type != JSON_OBJECT)
        return out;
    for (size_t i = 0; i < backends->c.count; i++) {
        json_t *b = backends->c.items[i];
        if (!b || b->type != JSON_OBJECT)
            continue;
        json_t *options = json_obj_get(b, "options");
        if (!options || options->type != JSON_ARRAY)
            continue;
        for (size_t j = 0; j < options->c.count; j++) {
            json_t *o = options->c.items[j];
            if (o && o->type == JSON_OBJECT)
                json_append(out, json_copy(o));
        }
    }
    return out;
}

/* ===========================================================================
 *  _at_host  (left_core_migration.py:_at_host)
 *
 *      try: return base_url_host_matches(url, host)
 *      except ValueError: return False
 *
 *  The C host matcher returns 0 for a host it cannot parse, which is the same
 *  verdict Python reaches by catching urlparse's ValueError.
 * =========================================================================== */

/* PoP: lcm_at_host @ hermes_cli/left_core_migration.py:_at_host */
int lcm_at_host(const char *url, const char *host)
{
    if (!url || !host)
        return 0;
    return provider_base_url_host_matches(url, host) ? 1 : 0;
}
