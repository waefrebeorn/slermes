#ifndef NOC_W8_TRANSPORT_607BB2_H
#define NOC_W8_TRANSPORT_607BB2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_transport_607bb2_serialize_frame @ tui_gateway/transport.py:serialize_frame
 * Happy path of serialize_frame(): json.dumps(obj, ensure_ascii=False) using
 * Python's default ", " / ": " separators.  Caller frees the returned string. */
char *w8_transport_607bb2_serialize_frame(const struct json_t *obj);

/* PoP: w8_transport_607bb2_tee_write @ tui_gateway/transport.py:TeeTransport.write
 * TeeTransport.write(): the primary's return value decides; secondary failures
 * are swallowed.  `secondaries` is an array of {"ok":bool,"raise":bool} seams
 * (ignored by the result, present only to exercise the live oracle path). */
struct json_t *w8_transport_607bb2_tee_write(int primary_ok,
                                             const struct json_t *secondaries);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_TRANSPORT_607BB2_H */
