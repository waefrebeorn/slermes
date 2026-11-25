/*
 * fernet.h — Fernet tokens (the format agent.vault_store.VaultStore writes).
 *
 * A Fernet token is base64url (padded) of:
 *
 *   0x80 || timestamp(8, big-endian) || IV(16) || AES-128-CBC ciphertext || HMAC-SHA256(32)
 *
 * The HMAC covers everything before it and is keyed with the *signing* half of the 32-byte key; the
 * AES key is the other half. Both halves come from the same base64url secret.
 *
 * This exists because the vault file is Fernet-encrypted: without it the C port can only read a vault
 * it wrote itself, which is not parity, it is a second format.
 */
#ifndef HERMES_FERNET_H
#define HERMES_FERNET_H

#include <stddef.h>

#define FERNET_IV_LEN 16

/* Split a Fernet secret into its halves. Returns 0 on success, -1 if the secret is not 32 raw bytes
 * of base64url. */
int fernet_key_split(const char *secret,
                     unsigned char signing[16], unsigned char encryption[16]);

/* Encrypt with an explicit IV and timestamp so the output is reproducible (the callers that matter
 * want the current time and a random IV; the tests want neither). Returns a malloc'd padded-base64url
 * token, or NULL with a malloc'd *err. */
char *fernet_encrypt(const char *secret, const unsigned char *plain, size_t len,
                     const unsigned char iv[FERNET_IV_LEN], long long ts, char **err);

/* Decrypt and verify. ttl < 0 disables the age check. Returns malloc'd plaintext (NUL-terminated for
 * convenience, but trust *out_len) or NULL with a malloc'd *err: "invalid token" for a bad HMAC, a bad
 * version, a bad length or bad padding, "expired" when the token is older than ttl. */
unsigned char *fernet_decrypt(const char *secret, const char *token,
                              long long now, long long ttl, size_t *out_len, char **err);

/* A fresh 32-byte key as a base64url secret, for vault creation. */
char *fernet_generate_key(void);

#endif /* HERMES_FERNET_H */
