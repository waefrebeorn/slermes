#ifndef NOC_APID_H
#define NOC_APID_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: apid_durable_response_id @ gateway/platforms/api_server_response_identity.py:durable_response_id
 * Returns "resp_" + sha256(utf8(request_key))[:28]; caller frees. */
char *apid_durable_response_id(const char *request_key);

/* PoP: apid_next_item_id @ gateway/platforms/api_server_response_identity.py:next_item_id
 *
 * Runs next_item_id() sequentially for each call descriptor in `calls` using
 * one shared counters dict (fresh).  Each call descriptor is an object with
 *   "kind":       string (one of function_call/function_call_output/reasoning/message)
 *   "call_id":    string or null (optional, default null)
 *   "commentary": bool (optional, default false)
 * Returns a JSON array of id strings (caller frees). */
struct json_t *apid_item_id_sequence(const char *response_id,
                                     const struct json_t *calls);

/* PoP: apid_identify_output @ gateway/platforms/api_server_response_identity.py:identify_output
 *
 * Copies each item, derives its id with a shared counters dict
 * (kind=item['type'], call_id=item.get('call_id'),
 *  commentary=(item.get('phase')=='commentary')), and returns the new array
 * of objects with an added/overwritten "id" key.  Caller frees. */
struct json_t *apid_identify_output(const struct json_t *items,
                                    const char *response_id);

#ifdef __cplusplus
}
#endif

#endif /* NOC_APID_H */
