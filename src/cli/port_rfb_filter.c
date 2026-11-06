/*
 * port_rfb_filter.c — Port of tools/bot_desktop/rfb_filter.py::_message_length:
 * the RFB client-message framer. Given the buffered client bytes, returns the
 * total message length (including the type byte), or a sentinel per the Python
 * contract: -1 when the buffer is too short to frame (Python None), -2 for
 * "unknown RFB client message type" ValueError, -3 for the oversized-clipboard
 * ValueError. Mirror's Python's exact fixed-length table and byte layout.
 */

#include "port_rfb_filter.h"

#include <stdint.h>
#include <stdlib.h>

/* Python: fixed-length client messages: type -> total length incl. type byte. */
static int fixed_length(unsigned char t)
{
    switch (t) {
    case 0:   return 20;  /* SetPixelFormat */
    case 3:   return 10;  /* FramebufferUpdateRequest */
    case 4:   return 8;   /* KeyEvent */
    case 5:   return 6;   /* PointerEvent */
    case 150: return 10;  /* EnableContinuousUpdates */
    case 255: return 12;  /* QEMU Extended KeyEvent */
    default:  return -1;
    }
}

/* PoP: rfb_msg_length @ tools/bot_desktop/rfb_filter.py:_message_length */
int rfb_msg_length(const unsigned char *buf, size_t len)
{
    if (len == 0)
        return -1; /* no type byte yet -> None */
    unsigned int t = buf[0];

    int fix = fixed_length((unsigned char)t);
    if (fix >= 0)
        return fix;

    if (t == 2) { /* _SET_ENCODINGS: u8 type, u8 pad, u16 nEncodings, then 4 bytes each */
        if (len < 4)
            return -1;
        unsigned int n = ((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
        return 4 + (int)(4 * n);
    }
    if (t == 6) { /* _CLIENT_CUT_TEXT: u8 type, u8 pad, u16 len, s32 length */
        if (len < 8)
            return -1;
        long n = (long)(int32_t)(((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16)
                                 | ((uint32_t)buf[6] << 8) | (uint32_t)buf[7]);
        long ab = n < 0 ? -n : n;
        if (ab > (long)(256 * 1024)) /* _MAX_CUT_TEXT */
            return -3;
        return 8 + (int)ab;
    }
    if (t == 248) { /* _FENCE: u8 type, u8 pad, u8 flags, u8 nLens, then nLens u32s */
        if (len < 9)
            return -1;
        return 9 + (int)buf[8];
    }
    if (t == 251) { /* _SET_DESKTOP_SIZE: u8 type, pad, u16 w, u16 h, u8 nScreens, pad, then 16 bytes each */
        if (len < 8)
            return -1;
        return 8 + 16 * (int)buf[6];
    }
    /* Unknown client message type: drop the stream. */
    return -2;
}
