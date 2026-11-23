#ifndef NOC_W9_BOTLIVEDEL_DAD0DE_H
#define NOC_W9_BOTLIVEDEL_DAD0DE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_botlivedel_dad0de_delivery_id @ tools/bot_live_delivery.py:_delivery_id
 * re.fullmatch(r"[0-9a-f]{32,64}", value).  Returns the JSON string on
 * success, or {"error": "delivery id must be 32 to 64 lowercase hex
 * characters"} on failure (the oracle maps the live ValueError to the same
 * shape). */
struct json_t *w9_botlivedel_dad0de_delivery_id(const char *value);

/* PoP: w9_botlivedel_dad0de_ticket_shape_error @ tools/bot_live_delivery.py:_ticket_shape_error
 * Pure validity check on a parsed ticket.  `stem` is path.stem (the injected
 * filename), `record` is the JSON object.  Returns a JSON string with the
 * reason, or JSON null when the ticket is well-formed. */
struct json_t *w9_botlivedel_dad0de_ticket_shape_error(const char *stem,
                                                      const struct json_t *record);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_BOTLIVEDEL_DAD0DE_H */
