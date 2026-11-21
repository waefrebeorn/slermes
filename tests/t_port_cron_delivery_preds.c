/* t_port_cron_delivery_preds.c — focused harness for two re-ported cron
 * delivery predicates in src/cron/scheduler_gaps.c.
 *
 * Reads JSON lines:
 *   {"fn":"seed","is_dm":0|1,"user_id":<string|null>}
 *   {"fn":"notify","cfg":<json-string>}   (cfg string is a JSON object or "null")
 * Emits {"fn":..., "in":<echo>, "out":0|1}.
 * The oracle ast-extracts the LIVE Python defs and replays on the echoed input.
 *
 * Gate:
 *   bash tests/run_one_oracle.sh cron_delivery_preds \
 *     oracle_cron_delivery_preds.py "" "lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the two functions under test, declared with their new signatures */
int inchannel_seed_allowed(int is_dm, const char *user_id);
int cron_delivery_notify_enabled(const json_t *cfg);

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

int main(void)
{
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *fn = json_get_str(in, "fn", "");
        if (strcmp(fn, "seed") == 0) {
            int is_dm = json_get_bool(in, "is_dm", 0);
            const char *uid = json_get_str(in, "user_id", NULL);
            int out = inchannel_seed_allowed(is_dm, uid);
            printf("{\"fn\":\"seed\",\"in\":{\"is_dm\":%d,\"user_id\":", is_dm);
            if (uid) jstr(uid); else fputs("null", stdout);
            printf("},\"out\":%d}\n", out);
        } else if (strcmp(fn, "notify") == 0) {
            json_t *cfg = json_obj_get(in, "cfg");  /* borrowed */
            int out = cron_delivery_notify_enabled(cfg);
            printf("{\"fn\":\"notify\",\"in\":{\"cfg\":");
            if (cfg) { char *s = json_dumps(cfg, 0); if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout); }
            else fputs("null", stdout);
            printf("},\"out\":%d}\n", out);
        }
        json_free(in);
    }
    return 0;
}
