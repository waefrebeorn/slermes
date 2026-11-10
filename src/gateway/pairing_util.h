/*
 * pairing_util.h - pairing string helpers.
 *
 * Module-internal header (pairing.c <-> pairing_util.c).
 */
#ifndef PAIRING_UTIL_H
#define PAIRING_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void hash_code(const char *code, const unsigned char *salt, int salt_len, char *out_hex /*65*/);
void skip_ws(const char **pp);

#endif /* PAIRING_UTIL_H */
