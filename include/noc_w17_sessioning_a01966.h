#ifndef NOC_W17_SESSIONING_A01966_H
#define NOC_W17_SESSIONING_A01966_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessioning_a01966_sniff_image_mime @ gateway/session_ingress_media.py:sniff_image_mime
 * data_hex is the hex encoding of the raw bytes; returns a MIME string or NULL (None). */
const char *w17_sessioning_a01966_sniff_image_mime(const char *data_hex);
/* PoP: w17_sessioning_a01966_admission_media_references @ gateway/session_ingress_media.py:admission_media_references */
struct json_t *w17_sessioning_a01966_admission_media_references(const struct json_t *payload);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_SESSIONING_A01966_H */
