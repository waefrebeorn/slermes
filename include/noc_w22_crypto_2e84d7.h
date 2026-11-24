#ifndef NOC_W22_CRYPTO_2E84D7_H
#define NOC_W22_CRYPTO_2E84D7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/qqbot/crypto.py (gate w22_crypto_2e84d7).
 *
 * Two functions, both real work:
 *   generate_bind_key() -- 32 random bytes from the OS RNG, base64 encoded.
 *   decrypt_secret()    -- base64 -> IV(12) || ciphertext || tag(16), then
 *                          AES-256-GCM open and a strict UTF-8 decode.
 *
 * AES-GCM is OpenSSL EVP (the same primitive `cryptography` uses), so any
 * nonce length AESGCM accepts (8..128 bytes) works, not only 96-bit ones.
 * The only seam is the OS RNG.
 *
 * Error classification mirrors the Python exception types the CLI can see:
 *   binascii.Error      "why": "padding" | "length_mod1"
 *   ValueError          "why": "key_length" | "nonce_length"
 *   InvalidTag          "why": "tag"
 *   UnicodeDecodeError  "why": "utf8"
 *
 * base64 decoding follows base64.b64decode(validate=False): characters
 * outside the alphabet are ignored, and padding must end the input.
 */

/* PoP: w22_crypto_2e84d7_bind_key @ plugins/platforms/qqbot/crypto.py:generate_bind_key
 * 32 fresh random bytes, standard base64 (44 chars, one '='). malloc'd. */
char *w22_crypto_2e84d7_bind_key(void);

/* PoP: w22_crypto_2e84d7_bind_key_shape @ plugins/platforms/qqbot/crypto.py:generate_bind_key
 * The observable shape of generate_bind_key()'s output:
 * {"bytes": 32, "chars": 44, "padding": 1, "alphabet": true}. */
json_t *w22_crypto_2e84d7_bind_key_shape(void);

/* PoP: w22_crypto_2e84d7_b64_encode_hex @ plugins/platforms/qqbot/crypto.py:generate_bind_key
 * base64 of the bytes named by a hex string (the encoder generate_bind_key
 * uses, driven by fixture bytes): {"ok": true, "value": "<b64>"}
 * or {"ok": false, "error": "ValueError"} for a malformed hex string. */
json_t *w22_crypto_2e84d7_b64_encode_hex(const char *hex);

/* PoP: w22_crypto_2e84d7_b64_decode_len @ plugins/platforms/qqbot/crypto.py:decrypt_secret
 * The length base64.b64decode() produces, or its error:
 * {"ok": true, "len": n} | {"ok": false, "error": "binascii.Error", "why": ...}. */
json_t *w22_crypto_2e84d7_b64_decode_len(const char *text);

/* PoP: w22_crypto_2e84d7_decrypt @ plugins/platforms/qqbot/crypto.py:decrypt_secret
 * -> {"ok": true, "value": "<plaintext>"}
 *  | {"ok": false, "error": "<exception type>", "why": "<detail>"}. */
json_t *w22_crypto_2e84d7_decrypt(const char *encrypted_base64, const char *key_base64);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_CRYPTO_2E84D7_H */
