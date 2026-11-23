#ifndef NOC_W7_SESSIONAPI_B56B61_H
#define NOC_W7_SESSIONAPI_B56B61_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_sessionapi_b56b61_api_generation_id @ gateway/session_api.py:api_generation_id
 * generation 0 -> session_id; else 'api-' + sha256(f'{sid}\0{g}').hexdigest()[:16]. */
struct json_t *w7_sessionapi_b56b61_api_generation_id(const char *session_id, int generation);

/* PoP: w7_sessionapi_b56b61_room_session_id @ gateway/session_api.py:room_session_id
 * 'room_' + sha256('\0'.join(identity) + optional '\0'+generation).hexdigest()[:32]. */
struct json_t *w7_sessionapi_b56b61_room_session_id(const struct json_t *identity, int generation);

/* PoP: w7_sessionapi_b56b61_api_storage_source @ gateway/session_api.py:api_storage_source
 * fallback unless fallback=='api_server', then the binding's storage_source (or fallback). */
struct json_t *w7_sessionapi_b56b61_api_storage_source(const struct json_t *store,
        const char *session_id, const char *fallback);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SESSIONAPI_B56B61_H */
