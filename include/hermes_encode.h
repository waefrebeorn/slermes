/*
 * hermes_encode.h — canonical hex/base64 encode/decode helpers for the
 * gateway layer.
 *
 * One definition each, shared across gateway platforms; the private copies
 * that used to live in email.c, pairing.c and webhook.c are gone. Callers
 * free() the *alloc() results.
 *
 * Minimal includes: this header pulls only <stddef.h>.
 */
#ifndef HERMES_ENCODE_H
#define HERMES_ENCODE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lowercase hex of data[0..len). Returns malloc'd buffer (len*2+1), caller
 * free()s. NULL on allocation failure. */
char *hex_encode_alloc(const unsigned char *data, size_t len);

/* Lowercase hex of bin[0..n) written into out (out must hold 2*n+1 bytes),
 * NUL-terminated. Always writes; returns out. */
void hex_encode_buf(const unsigned char *bin, int n, char *out);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ENCODE_H */
