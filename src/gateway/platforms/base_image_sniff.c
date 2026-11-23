/*
 * base_image_sniff.c — C port of upstream Hermes gateway/platforms/base_image_sniff.py.
 * PoP: imagesniff_looks_like_image @ gateway/platforms/base_image_sniff.py:_looks_like_image
 *
 * The ISO-BMFF branch calls agent.image_routing._sniff_mime_from_bytes(data[:16])
 * and keeps only image/heic + image/avif. For a box with "ftyp" at offset 4, the
 * only matching magic in that sniffer is the major brand at offset 8; this file
 * reproduces that brand -> MIME map (mirrors _FTYP_BRANDS).
 */
#include "noc_imagesniff.h"
#include <string.h>

static unsigned int be32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8)  | (unsigned int)p[3];
}

static int iso_brand_is_image(const unsigned char *b) {
    static const char *const AVIF[] = { "avif", "avis", NULL };
    static const char *const HEIC[] = { "heic", "heix", "hevc", "hevx",
                                        "mif1", "msf1", "heim", "heis", NULL };
    char brand[5] = { (char)b[0], (char)b[1], (char)b[2], (char)b[3], 0 };
    for (int i = 0; AVIF[i]; i++) if (!strcmp(brand, AVIF[i])) return 1;
    for (int i = 0; HEIC[i]; i++) if (!strcmp(brand, HEIC[i])) return 1;
    return 0;
}

int imagesniff_looks_like_image(const unsigned char *data, size_t len) {
    if (len >= 4) {
        if (len >= 8 && !memcmp(data, "\x89PNG\r\n\x1a\n", 8)) return 1;
        if (len >= 3 && !memcmp(data, "\xff\xd8\xff", 3)) return 1;
        if (len >= 6 && (!memcmp(data, "GIF87a", 6) || !memcmp(data, "GIF89a", 6))) return 1;
        if (len >= 2 && !memcmp(data, "BM", 2)) return 1;
        if (!memcmp(data, "RIFF", 4) && len >= 12 && !memcmp(data + 8, "WEBP", 4)) return 1;
    }
    /* ftyp branch: data[4:8] == b"ftyp" and len >= int.from_bytes(data[:4],"big") >= 16 */
    if (len < 8 || memcmp(data + 4, "ftyp", 4)) return 0;
    unsigned int sz = be32(data);
    if (!(len >= sz && sz >= 16)) return 0;
    if (len < 12) return 0;
    return iso_brand_is_image(data + 8);
}
