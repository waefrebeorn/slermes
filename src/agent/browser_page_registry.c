/*
 * browser_page_registry.c — faithful C11 port of agent/browser_page_registry.py
 * (gate w14_browserpag_ace071).
 *
 * PoP: w14_browserpag_ace071_backend_for_task @ agent/browser_page_registry.py:backend_for_task
 *
 * Live _registry.list_providers() is injected as the `providers` JSON array;
 * each backend is {name, pages[], raises}. `raises` models a has_page() that
 * throws, which the live loop swallows as "no page here".
 */
#include "noc_w14_browserpag_ace071.h"

#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

char *w14_browserpag_ace071_backend_for_task(const char *task_id, const json_t *providers) {
    if (!providers || providers->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < providers->c.count; ++i) {
        json_t *p = json_get(providers, i);
        json_t *pages, *nm;
        bool has = false;
        if (!p || p->type != JSON_OBJECT) continue;
        if (json_get_bool(p, "raises", false)) continue; /* live: except Exception -> log.debug */
        pages = json_obj_get(p, "pages");
        if (pages && pages->type == JSON_ARRAY) {
            for (size_t j = 0; j < pages->c.count; ++j) {
                json_t *pg = json_get(pages, j);
                if (task_id && pg && pg->type == JSON_STRING && pg->str_val &&
                    !strcmp(pg->str_val, task_id)) { has = true; break; }
            }
        }
        if (has) {
            nm = json_obj_get(p, "name");
            if (!nm || nm->type == JSON_NULL || nm->type != JSON_STRING) return NULL;
            return dupstr(nm->str_val ? nm->str_val : "");
        }
    }
    return NULL;
}
