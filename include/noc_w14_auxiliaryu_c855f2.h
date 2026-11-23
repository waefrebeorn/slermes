#ifndef NOC_W14_AUXILIARYU_C855F2_H
#define NOC_W14_AUXILIARYU_C855F2_H

#include <stdbool.h>
#include "hermes_json.h"

/* Faithful C11 port of upstream agent/auxiliary_unavailable.py. */

char *w14_auxiliaryu_c855f2_sentence(const json_t *text);
char *w14_auxiliaryu_c855f2_missing_provider_credentials_message_ex(
    const char *provider_id, const json_t *cooldown, const json_t *pconfig, bool llama_alias);

#endif /* NOC_W14_AUXILIARYU_C855F2_H */
