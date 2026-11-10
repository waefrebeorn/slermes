/*
 * pairing_util.c - pairing string helpers.
 *
 * Split from pairing.c (soul.md: self-contained modules). String hash and
 * whitespace skip. No file-scope state.
 */
#include "pairing_util.h"
#include "crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/random.h>
#include <stdlib.h>
#include <dirent.h>
#include "crypto.h"
#include "hermes_encode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
 void skip_ws(const char **pp) {
    while (**pp && isspace((unsigned char)**pp)) (*pp)++;
}


/* ── hashing (hex via shared hermes_encode.h) ── */

/* PoP: hash_code @ gateway/pairing.py:_hash_code */ void hash_code(const char *code, const unsigned char *salt, int salt_len, char *out_hex /*65*/) {
    unsigned char buf[64];
    memcpy(buf, salt, salt_len);
    memcpy(buf+salt_len, code, strlen(code));
    unsigned char h[32];
    crypto_sha256(buf, salt_len + strlen(code), h);
    hex_encode_buf(h, 32, out_hex);
}
