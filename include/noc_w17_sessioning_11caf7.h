#ifndef NOC_W17_SESSIONING_11CAF7_H
#define NOC_W17_SESSIONING_11CAF7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessioning_11caf7_holds_busy_burst @ gateway/session_ingress_burst.py:holds_busy_burst */
int w17_sessioning_11caf7_holds_busy_burst(const struct json_t *adapter, const char *session_key);
/* PoP: w17_sessioning_11caf7_serial @ gateway/session_ingress_burst.py:_serial */
int w17_sessioning_11caf7_serial(const struct json_t *adapter);
/* PoP: w17_sessioning_11caf7_kind @ gateway/session_ingress_burst.py:_kind
 * Returns "photo", "text", or NULL (Python None). */
const char *w17_sessioning_11caf7_kind(const struct json_t *adapter, const struct json_t *event);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_SESSIONING_11CAF7_H */
