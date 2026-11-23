#ifndef NOC_W8_SESSIONIMAG_8B2BC8_H
#define NOC_W8_SESSIONIMAG_8B2BC8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_sessionimag_8b2bc8_attach @ gateway/session_images.py:attach_bytes
 * PoP: w8_sessionimag_8b2bc8_attach @ gateway/session_images.py:_stage
 *
 * Mirrors the attach_bytes validation gate and the _stage pure core:
 *   input  = whole fixture object (session_id, params, subscribed,
 *            authorize_error, storage_ok)
 *   output = {"ok":true,"result":{...}} on success, or
 *            {"ok":false,"error":"<code>"} for an AuthorityRefusal.
 *
 * The real data:image/<subtype>;base64, decode (with the 25MiB cap), strict
 * base64 decode, image extension sniff (filename hint else magic bytes) and
 * the .ext -> mime map are ported in-full. stage_image_bytes is injected with
 * the deterministic stand-in `upload_stub<ext>` under `/stage/` so the staged
 * path/name are reproducible (the real staging uses a random mkstemp name).
 */
struct json_t *w8_sessionimag_8b2bc8_attach(const struct json_t *input);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SESSIONIMAG_8B2BC8_H */
