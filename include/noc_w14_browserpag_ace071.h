#ifndef NOC_W14_BROWSERPAG_ACE071_H
#define NOC_W14_BROWSERPAG_ACE071_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/browser_page_registry.py. */

char *w14_browserpag_ace071_backend_for_task(const char *task_id, const json_t *providers);

#endif /* NOC_W14_BROWSERPAG_ACE071_H */
