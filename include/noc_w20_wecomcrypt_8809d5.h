#ifndef NOC_W20_WECOMCRYPT_8809D5_H
#define NOC_W20_WECOMCRYPT_8809D5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/wecom/wecom_crypto.py (gate w20_wecomcrypt_8809d5).
 *
 * Real AES-256-CBC (OpenSSL EVP), real SHA-1 and real base64: this is the whole
 * callback crypto, not a shape. The only injected seam is timestamp_fresh's
 * "now" (spec["now"]), so the replay window is testable without sleeping.
 * Every answer is {"status":"ok","value":...} or
 * {"status":"raised","type":<ExceptionClass>,"message":str(exc)}.
 */

/* PoP: w20_wecomcrypt_8809d5_init @ plugins/platforms/wecom/wecom_crypto.py:WXBizMsgCrypt.__init__
 * Validation order and texts: token, encoding_aes_key, its 43-char length,
 * receive_id. value = {"key_len":32,"iv_len":16}. */
json_t *w20_wecomcrypt_8809d5_init(const json_t *spec);

/* PoP: w20_wecomcrypt_8809d5_sha1_signature @ plugins/platforms/wecom/wecom_crypto.py:_sha1_signature
 * sha1("".join(sorted([token, timestamp, nonce, encrypt]))).hexdigest(). */
json_t *w20_wecomcrypt_8809d5_sha1_signature(const json_t *spec);

/* PoP: w20_wecomcrypt_8809d5_pkcs7_encode @ plugins/platforms/wecom/wecom_crypto.py:PKCS7Encoder.encode
 * spec = {"text": hex bytes} -> value = {"hex": padded hex, "len": n}. */
json_t *w20_wecomcrypt_8809d5_pkcs7_encode(const json_t *spec);

/* PoP: w20_wecomcrypt_8809d5_pkcs7_decode @ plugins/platforms/wecom/wecom_crypto.py:PKCS7Encoder.decode
 * spec = {"hex": hex bytes} -> value = {"hex": unpadded hex, "len": n}. */
json_t *w20_wecomcrypt_8809d5_pkcs7_decode(const json_t *spec);

/* PoP: w20_wecomcrypt_8809d5_decrypt @ plugins/platforms/wecom/wecom_crypto.py:WXBizMsgCrypt.decrypt
 * spec = {"token","key","receive_id","msg_signature","timestamp","nonce",
 *         "encrypt","now"} -> value = the decrypted callback XML text.
 * Signature, replay window, base64, PKCS7, the 16-byte prefix skip, the
 * big-endian length and the receive_id comparison are all enforced. */
json_t *w20_wecomcrypt_8809d5_decrypt(const json_t *spec);

/* PoP: w20_wecomcrypt_8809d5_encrypt @ plugins/platforms/wecom/wecom_crypto.py:WXBizMsgCrypt.encrypt
 * spec = {"token","key","receive_id","plaintext","nonce","timestamp"}
 * -> value = {"encrypt": b64, "signature": hex, "timestamp": s, "nonce": s,
 *             "xml": full callback XML}. The 16-byte prefix comes from
 * OpenSSL RAND_bytes (upstream uses os.urandom). */
json_t *w20_wecomcrypt_8809d5_encrypt(const json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_WECOMCRYPT_8809D5_H */
