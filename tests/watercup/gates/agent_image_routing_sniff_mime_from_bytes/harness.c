/* gate: agent_image_routing_sniff_mime_from_bytes — traced copy of
 * src/agent/image_routing_ports.c::imgr_sniff_mime_from_bytes.
 * LIVE twin: agent/image_routing.py::_sniff_mime_from_bytes.
 *
 * Shared branch ladder of the C port (twin traces THIS order using LIVE
 * semantics; LIVE's own source order checks PNG before JPEG and has extra
 * BMP/TIFF/ICO/SVG rungs that the port lacks — see gate.json notes):
 *   B1 len < 4 -> NULL
 *   B2 JPEG FF D8          -> image/jpeg
 *   B3 PNG 89 P N G        -> image/png
 *   B4 WEBP (len>=12 RIFF..WEBP) -> image/webp
 *   B5 GIF prefix (len>=6) -> image/gif
 *   B6 ISO-BMFF ftyp (len>=12)
 *     B6a brand heic -> image/heic
 *     B6b brand avif -> image/avif
 *   B7 exhausted -> NULL
 * V-event: resulting mime string ("" for NULL).
 * Inputs are hex-encoded byte strings (magic bytes are not argv-safe).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static int unhex(const char *s, unsigned char *out, size_t max) {
    size_t n = 0;
    while (s[0] && s[1] && n + 2 <= max) {
        unsigned v;
        if (sscanf(s, "%2x", &v) != 1) break;
        out[n++] = (unsigned char)v;
        s += 2;
    }
    return (int)n;
}

static const char *wc_sniff(const unsigned char *b, int len) {
    WCUP_BEGIN("sniff_mime_from_bytes");
    const char *mime = NULL;
    if (!b || len < 4) {                                        /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    if (b[0] == 0xFF && b[1] == 0xD8) {                         /* B2 */
        WCUP_BRANCH(1); mime = "image/jpeg"; goto out;
    }
    WCUP_BRANCH(0);
    if (b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') {  /* B3 */
        WCUP_BRANCH(1); mime = "image/png"; goto out;
    }
    WCUP_BRANCH(0);
    if (len >= 12 && b[0]=='R' && b[1]=='I' && b[2]=='F' && b[3]=='F' &&
        b[8]=='W' && b[9]=='E' && b[10]=='B' && b[11]=='P') {   /* B4 */
        WCUP_BRANCH(1); mime = "image/webp"; goto out;
    }
    WCUP_BRANCH(0);
    if (len >= 6 && b[0]=='G' && b[1]=='I' && b[2]=='F') {      /* B5 */
        WCUP_BRANCH(1); mime = "image/gif"; goto out;
    }
    WCUP_BRANCH(0);
    if (len >= 12 && b[4]=='f' && b[5]=='t' && b[6]=='y' && b[7]=='p') { /* B6 */
        WCUP_BRANCH(1);
        if (b[8]=='h' && b[9]=='e' && b[10]=='i' && b[11]=='c') {  /* B6a */
            WCUP_BRANCH(1); mime = "image/heic"; goto out;
        }
        WCUP_BRANCH(0);
        if (b[8]=='a' && b[9]=='v' && b[10]=='i' && b[11]=='f') {  /* B6b */
            WCUP_BRANCH(1); mime = "image/avif"; goto out;
        }
        WCUP_BRANCH(0);
        goto out;
    }
    WCUP_BRANCH(0);                                             /* B7 */
out:
    WCUP_VALSTR(mime ? mime : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sniff_mime_from_bytes");
    return mime;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sniff_mime_from_bytes <hexbytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        unsigned char buf[4096];
        int n = unhex(argv[i], buf, sizeof buf);
        wc_sniff(buf, n);
    }
    return 0;
}
