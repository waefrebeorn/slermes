#ifndef NOC_W17_INBOUNDREP_DAE3A2_H
#define NOC_W17_INBOUNDREP_DAE3A2_H

#include "hermes_json.h"

/* str(getattr(platform, "value", platform)).lower() + ":" + str(chat_id) */
char *w17_inboundrep_dae3a2_chat_key(json_t *platform, json_t *chat_id);

/* dict.get(key) / getattr(obj, key, None) -> JSON value or null */
json_t *w17_inboundrep_dae3a2_field(json_t *result, const char *key);

/* sanitized_text is the injected strip_internal_markup result; header is the
 * injected rich_sent_store.lookup_marker result (NULL/"" == no header). */
char *w17_inboundrep_dae3a2_reply_pointer(const char *sanitized_text, int own_message,
                                          int has_message_id, const char *header);

/* Returns {"chat_key":str,"ids":[...],"marker":str,"header":str} or null. */
json_t *w17_inboundrep_dae3a2_record_cron_delivery(json_t *platform, json_t *chat_id,
                                                    const char *text, json_t *send_result);

#endif
