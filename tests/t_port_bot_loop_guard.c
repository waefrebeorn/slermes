/* t_port_bot_loop_guard.c — oracle harness for gateway/bot_loop_guard.py
 * pure coercion + settings helpers. */
#include "port_bot_loop_guard.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _as_bool: raw JSON scalar + default. */
    static const char *const braws[] = {
        "null", "true", "false", "\"yes\"", "\"  ON \"", "\"off\"",
        "\"1\"", "\"0\"", "\"maybe\"", "5", "0", "\"trUe\"", "1.5",
    };
    for (size_t i = 0; i < sizeof(braws) / sizeof(braws[0]); i++) {
        printf("{\"family\":\"bool\",\"case\":%zu,\"raw\":%s,\"def\":0,\"out\":%d}\n",
               i, braws[i], blg_as_bool(braws[i], 0));
    }

    /* _as_positive: raw + default 300.0. */
    static const char *const praws[] = {
        "\"5\"", "5", "null", "true", "\"3.5\"", "\"abc\"", "0", "-2",
        "\" 7 \"", "7.0",
    };
    for (size_t i = 0; i < sizeof(praws) / sizeof(praws[0]); i++) {
        printf("{\"family\":\"pos\",\"case\":%zu,\"raw\":%s,\"def\":300.0,\"out\":%.17g}\n",
               i, praws[i], blg_as_positive(praws[i], 300.0));
    }

    /* _as_positive_int: raw + default 20. */
    static const char *const iraws[] = {
        "\"5\"", "5", "\"5.5\"", "null", "true", "\"3\"", "0.5", "-1",
        "\"20\"", "5.0",
    };
    for (size_t i = 0; i < sizeof(iraws) / sizeof(iraws[0]); i++) {
        printf("{\"family\":\"pint\",\"case\":%zu,\"raw\":%s,\"def\":20,\"out\":%lld}\n",
               i, iraws[i], (long long)blg_as_positive_int(iraws[i], 20));
    }

    /* settings_from_config: bot_loop_guard block. */
    static const char *const blocks[] = {
        "null", "\"x\"", "{}", "{\"enabled\":true,\"max_events\":50}",
        "{\"enabled\":\"off\",\"max_events\":\"7\",\"window_seconds\":120,\"cooldown_seconds\":0}",
        "{\"max_events\":3}",
    };
    for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
        char *out = blg_settings_from_config(blocks[i]);
        printf("{\"family\":\"set\",\"case\":%zu,\"block\":%s,\"out\":%s}\n",
               i, blocks[i], out ? out : "null");
        free(out);
    }
    return 0;
}
