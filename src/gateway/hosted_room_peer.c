#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: gateway_hosted_room_peer_u_b64encode @ gateway/hosted_room_peer.py:_b64encode */
/* base64.urlsafe_b64encode(value).rstrip(b"=").decode("ascii") over the input's
 * UTF-8 bytes. */
char *gateway_hosted_room_peer_u_b64encode(const char *arg) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const unsigned char *p = (const unsigned char *)(arg ? arg : "");
    size_t n = strlen((const char *)p);
    size_t cap = ((n + 2) / 3) * 4 + 8, w = 0;
    char *out = malloc(cap);
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)p[i] << 16;
        int have = 1;
        if (i + 1 < n) { v |= (unsigned)p[i+1] << 8; have = 2; }
        if (i + 2 < n) { v |= (unsigned)p[i+2]; have = 3; }
        out[w++] = T[(v >> 18) & 63];
        out[w++] = T[(v >> 12) & 63];
        if (have > 1) out[w++] = T[(v >> 6) & 63];
        if (have > 2) out[w++] = T[v & 63];
    }
    out[w] = 0;
    return out;
}
