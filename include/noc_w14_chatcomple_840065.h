#ifndef NOC_W14_CHATCOMPLE_840065_H
#define NOC_W14_CHATCOMPLE_840065_H

#include <stdbool.h>
#include "hermes_json.h"

/* Faithful C11 port of upstream agent/chat_completion_local.py. */

int     w14_chatcomple_840065_stale_giveup_ex(const char *env_value);
bool    w14_chatcomple_840065_streams_llamacpp_ex(const char *api_mode, const char *provider,
                                                 const char *base_url, bool managed);
json_t *w14_chatcomple_840065_retry_recovered_local_server_ex(long long giveup, long long streak,
                                                             bool streams, bool answers);

#endif /* NOC_W14_CHATCOMPLE_840065_H */
