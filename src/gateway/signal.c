/*
 * port_signal.c — Port of Python gateway/platforms/signal.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_gateway_signal.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Maximum number of sent message timestamps to track */
#define MAX_SENT_TIMESTAMPS 256

/* Simple timestamp cache entry */
typedef struct {
    char key[64];
    bool active;
} sent_timestamp_entry_t;

static sent_timestamp_entry_t sent_timestamps[MAX_SENT_TIMESTAMPS];
static int sent_timestamp_count = 0;

/*
 * _extract_quote_author — Return the best available Signal sender identifier from quote metadata.
 *
 * Python: def _extract_quote_author(quote_data: Any) -> Optional[str]:
 *   if not isinstance(quote_data, dict): return None
 *   for key in ("author", "authorNumber", "authorUuid", "authorAci", ...):
 *       val = quote_data.get(key)
 *       if val: return str(val)
 *   return None
 */
/* Port of Python: _extract_quote_author */
const char* _extract_quote_author(json_t* quote_data)
{
    if (!quote_data || !json_node_is_object(quote_data)) {
        return NULL;
    }

    static const char* keys[] = {
        "author", "authorNumber", "authorUuid", "authorAci",
        "authorServiceId", "authorServiceIdString", NULL
    };

    for (int i = 0; keys[i]; i++) {
        json_t* val = json_object_get(quote_data, keys[i]);
        if (val) {
            const char* s = json_node_get_string(val);
            if (s && s[0]) {
                return s;
            }
        }
    }

    return NULL;
}

/*
 * _remember_sent_message_timestamp — Keep a bounded cache of outbound Signal timestamps.
 *
 * Python: def _remember_sent_message_timestamp(self, timestamp: Any) -> None:
 *   if timestamp is None: return
 *   key = str(timestamp)
 *   self._sent_message_timestamps.pop(key, None)
 *   self._sent_message_timestamps[key] = None
 *   while len(...) > cap: pop oldest
 */
/* Port of Python: _remember_sent_message_timestamp */
void _remember_sent_message_timestamp(const char* timestamp)
{
    if (!timestamp || !timestamp[0]) return;

    /* Check if already in cache — if so, just mark as recent */
    for (int i = 0; i < sent_timestamp_count; i++) {
        if (sent_timestamps[i].active && strcmp(sent_timestamps[i].key, timestamp) == 0) {
            /* Move to end (most recently used) */
            sent_timestamp_entry_t tmp = sent_timestamps[i];
            memmove(&sent_timestamps[i], &sent_timestamps[i+1],
                    (sent_timestamp_count - i - 1) * sizeof(sent_timestamp_entry_t));
            sent_timestamps[sent_timestamp_count - 1] = tmp;
            return;
        }
    }

    /* Add new entry */
    if (sent_timestamp_count < MAX_SENT_TIMESTAMPS) {
        strncpy(sent_timestamps[sent_timestamp_count].key, timestamp, 63);
        sent_timestamps[sent_timestamp_count].key[63] = '\0';
        sent_timestamps[sent_timestamp_count].active = true;
        sent_timestamp_count++;
    } else {
        /* FIFO-evict oldest */
        memmove(&sent_timestamps[0], &sent_timestamps[1],
                (MAX_SENT_TIMESTAMPS - 1) * sizeof(sent_timestamp_entry_t));
        strncpy(sent_timestamps[MAX_SENT_TIMESTAMPS - 1].key, timestamp, 63);
        sent_timestamps[MAX_SENT_TIMESTAMPS - 1].key[63] = '\0';
        sent_timestamps[MAX_SENT_TIMESTAMPS - 1].active = true;
    }
}

/*
 * _consume_sent_timestamp — Pop a timestamp if it matches one we sent.
 *
 * Python: def _consume_sent_timestamp(self, ts) -> bool:
 *   if ts and ts in self._recent_sent_timestamps:
 *       self._recent_sent_timestamps.pop(ts, None)
 *       return True
 *   return False
 */
/* Port of Python: _consume_sent_timestamp */
bool _consume_sent_timestamp(const char* ts)
{
    if (!ts || !ts[0]) return false;

    for (int i = 0; i < sent_timestamp_count; i++) {
        if (sent_timestamps[i].active && strcmp(sent_timestamps[i].key, ts) == 0) {
            /* Remove from cache */
            sent_timestamps[i].active = false;
            memmove(&sent_timestamps[i], &sent_timestamps[i+1],
                    (sent_timestamp_count - i - 1) * sizeof(sent_timestamp_entry_t));
            sent_timestamp_count--;
            return true;
        }
    }

    return false;
}
