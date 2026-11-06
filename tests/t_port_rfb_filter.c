/* t_port_rfb_filter.c — oracle harness for _message_length.
 * Emits {"tag":"rfb","case":..,"hex":<buf hex>,"out":<len|-1|-2|-3>}. */
#include "port_rfb_filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const cases[] = {
        /* fixed types */
        "00",              /* SetPixelFormat -> 20 */
        "03",              /* FramebufferUpdateRequest -> 10 */
        "04",              /* KeyEvent -> 8 */
        "05",              /* PointerEvent -> 6 */
        "96",              /* EnableContinuousUpdates 150 -> 10 */
        "ff",              /* QEMU 255 -> 12 */
        /* set encodings */
        "02",              /* too short -> -1 */
        "02000200",        /* n=2 -> 4+8=12 */
        "02000201",        /* n=1 -> 4+4=8 */
        /* client cut text */
        "0600",            /* too short -> -1 */
        "06bee20000010000",/* len field 8, n=0 -> 8 */
        "06bee20000020000",/* n=2 -> 10 */
        /* oversized clipboard -> -3 */
        "06bee20020000000",/* n=32 -> 8+32 */
        "06bee20000000000",/* n=0 */
        "06bee200ffffff7f",/* n=INT_MAX -> 8 + INT_MAX (valid, huge) */
        "06bee200ff000001",/* n=511 */
        "06bee200ffffff80",/* n=INT_MIN -> abs = 2147483648 > cap -> -3 */
        /* fence */
        "f8",              /* too short -> -1 */
        "f800000003",      /* 9 + 3 = 12 */
        "f800000000",      /* 9 */
        /* set desktop size */
        "fb",              /* too short -> -1 */
        "fb0000beef00",    /* nScreens=0 -> 8 */
        "fb0000beef01",    /* nScreens=1 -> 8+16=24 */
        "fb0000beef02",    /* nScreens=2 -> 8+32=40 */
        /* unknown type -> -2 */
        "07",
        "01",
        "99",
    };
    size_t n = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < n; i++) {
        const char *hx = cases[i];
        size_t hlen = strlen(hx);
        size_t blen = 0;
        /* buffers can contain NULs when a hex pair is "00" */
        static unsigned char buf[8]; /* max needed: 8 bytes */
        for (size_t j = 0; j + 1 < hlen; j += 2) {
            int hi = hexval(hx[j]), lo = hexval(hx[j + 1]);
            if (hi < 0 || lo < 0) continue;
            buf[blen++] = (unsigned char)((hi << 4) | lo);
        }
        int out = rfb_msg_length(buf, blen);
        printf("{\"tag\":\"rfb\",\"case\":%zu,\"hex\":\"%s\",\"out\":%d}\n",
               i, hx, out);
    }
    return 0;
}
