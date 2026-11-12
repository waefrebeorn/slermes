/*
 * gateway_escape.c — gateway text-escape helpers.
 *
 * gw_utf16_len is the unit that Discord/Slack-style platform limits are
 * measured in: UTF-16 code units, not bytes and not UTF-8 code points.
 */

#include "hermes_gateway.h"

#include <stddef.h>

/* PoP: gw_utf16_len @ gateway/platforms/base.py:utf16_len */
/*
 * Count UTF-16 code units in a NUL-terminated UTF-8 string.
 *
 *   1-byte (ASCII)     -> 1 unit
 *   2-byte (U+0080..7FF)   -> 1 unit
 *   3-byte (U+0800..FFFF)  -> 1 unit
 *   4-byte (U+10000+)      -> 2 units (surrogate pair)
 *
 * Malformed lead bytes are counted as one unit and advance one byte, so a
 * truncated sequence cannot loop forever or underflow the count.
 */
size_t gw_utf16_len(const char *s)
{
    if (!s) return 0;

    size_t units = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ) {
        unsigned char c = *p;
        size_t adv;
        if      (c < 0x80)         { adv = 1; units += 1; }
        else if ((c & 0xE0) == 0xC0) { adv = 2; units += 1; }
        else if ((c & 0xF0) == 0xE0) { adv = 3; units += 1; }
        else if ((c & 0xF8) == 0xF0) { adv = 4; units += 2; }
        else                       { adv = 1; units += 1; }

        /* Never step past the terminator: a truncated sequence must stop
         * here rather than read beyond the buffer. */
        size_t stepped = 0;
        while (stepped < adv && p[stepped]) stepped++;
        p += stepped ? stepped : 1;
    }
    return units;
}
