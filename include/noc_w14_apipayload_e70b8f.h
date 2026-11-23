#ifndef NOC_W14_APIPAYLOAD_E70B8F_H
#define NOC_W14_APIPAYLOAD_E70B8F_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_apipayload_e70b8f_data_url_bytes @ session_rules/api_payloads.py:data_url_bytes
 * Returns null or {"mime":str,"bytes":[int,...]} for a committable data:image URL. */
struct json_t *w14_apipayload_e70b8f_data_url_bytes(const struct json_t *url);
/* PoP: w14_apipayload_e70b8f_compacted_api_payload @ session_rules/api_payloads.py:compacted_api_payload
 * Returns null or the compacted terminal payload dict. */
struct json_t *w14_apipayload_e70b8f_compacted_api_payload(const struct json_t *payload,
                                                          const struct json_t *request_id);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_APIPAYLOAD_E70B8F_H */
