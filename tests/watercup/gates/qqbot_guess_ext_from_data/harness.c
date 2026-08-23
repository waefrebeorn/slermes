/* gate: qqbot_guess_ext_from_data — traced copy of
 * src/gateway/platforms/qqbot/qqbot_wrappers.c::qqbot_u_guess_ext_from_data
 * (PoP: gateway/platforms/qqbot/adapter.py::QQAdapter._guess_ext_from_data).
 * Input: hex-encoded bytes. Output V event: guessed extension.
 * The ||-disjuncts are split so every rung is ONE simple predicate, walked
 * in order until the first hit (RUNG macro below).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static int hexval(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }

static void run_one(const char *hex) {
    unsigned char data[512]; size_t n = 0;
    for (const char *p = hex; p[0] && p[1] && n < sizeof data; p += 2)
        data[n++] = (unsigned char)((hexval(p[0]) << 4) | hexval(p[1]));
    const char *ext = NULL;
    int m = 0;
#define RUNG(cond, e) do { if (!m) { WCUP_BRANCH(cond); if (cond) { m = 1; if (e) ext = (e); } } } while (0)
    WCUP_BEGIN("guess_ext_from_data");
    RUNG(n >= 9 && memcmp(data, "#!SILK_V3", 9) == 0, ".silk");
    RUNG(n >= 6 && memcmp(data, "#!SILK", 6) == 0, ".silk");
    RUNG(n >= 2 && data[0] == 0x02 && data[1] == '!', ".silk");
    RUNG(n >= 4 && memcmp(data, "RIFF", 4) == 0, ".wav");
    RUNG(n >= 4 && memcmp(data, "fLaC", 4) == 0, ".flac");
    RUNG(n >= 2 && data[0] == 0xff &&
         (data[1] == 0xfb || data[1] == 0xf3 || data[1] == 0xf2), ".mp3");
    RUNG(n >= 4 && memcmp(data, "\x30\x26\xb2\x75", 4) == 0, ".ogg");
    RUNG(n >= 4 && memcmp(data, "OggS", 4) == 0, ".ogg");
    RUNG(n >= 4 && memcmp(data, "\x00\x00\x00\x20", 4) == 0, NULL);
    RUNG(n >= 4 && memcmp(data, "\x00\x00\x00\x1c", 4) == 0, ".amr");
#undef RUNG
    WCUP_VALSTR(ext ? ext : ".amr");
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("guess_ext_from_data");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s guess_ext_from_data <hex>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
