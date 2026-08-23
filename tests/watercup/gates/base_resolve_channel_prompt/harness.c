/* gate: base_resolve_channel_prompt — traced copy of
 * src/hermes_cli/platforms_base_helpers.c::gw_base__resolve_channel_prompt
 * (LIVE twin: gateway/platforms/base.py::resolve_channel_prompt).
 *
 * Input encoding (one argv token): "<extra_json>|<channel_id>|<parent_id>"
 * ('|' never appears inside the probes' JSON or ids; empty id allowed).
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: extra missing / unparseable        -> None
 *   B2: channel_prompts missing/not object -> None
 *   loop key in {channel_id, parent_id}:
 *     B3/B6: key empty                     -> skip
 *     B4/B7: prompt present, string, nonblank -> V(prompt) return
 *   B5: exhausted                          -> None
 *   V: result prompt or "NONE"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../../../lib/libjson/json.h"
#include "watercup.h"

static void finish(wcup_t *_wc, const char *result) {
    char cup[32];
    wcup_valstr(_wc, result ? result : "NONE");
    wcup_finalize(_wc, cup, (int)sizeof cup);
    printf("%s\n", cup);
}

static void wc_resolve_channel_prompt(const char *extra_json,
                                      const char *channel_id,
                                      const char *parent_id) {
    WCUP_BEGIN("resolve_channel_prompt");
    const char *result = NULL;
    json_t *extra = NULL;
    if (!extra_json || !(extra = json_parse(extra_json, NULL))) {  /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        json_t *prompts = json_obj_get(extra, "channel_prompts");
        if (!prompts || prompts->type != JSON_OBJECT) {            /* B2 */
            WCUP_BRANCH(1);
            goto out;
        }
        WCUP_BRANCH(0);
        const char *keys[2] = { channel_id, parent_id };
        for (int i = 0; i < 2; i++) {
            int blank_key = (!keys[i] || !*keys[i]);               /* B3 / B6 */
            WCUP_BRANCH(blank_key);
            if (blank_key) continue;
            json_t *p = json_obj_get(prompts, keys[i]);
            int have = (p && p->type == JSON_STRING);
            WCUP_BRANCH(have);                                     /* B4 / B7 */
            if (have) {
                const char *s = p->str_val ? p->str_val : "";
                while (*s && isspace((unsigned char)*s)) s++;
                size_t sl = strlen(s);
                while (sl > 0 && isspace((unsigned char)s[sl-1])) sl--;
                if (sl) {
                    static char buf[512];
                    memcpy(buf, s, sl); buf[sl] = '\0';
                    result = buf;
                    break;
                }
                /* whitespace-only: treated as absent — no extra rung */
            }
        }
        WCUP_BRANCH(result != NULL);                               /* B5 */
    }
out:
    finish(&_wc, result);
    json_free(extra);
    WCUP_END("resolve_channel_prompt");
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: %s resolve_channel_prompt <enc>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *s1 = strchr(buf, '|');
        char *s2 = s1 ? strchr(s1 + 1, '|') : NULL;
        if (s1) *s1 = '\0';
        if (s2) *s2 = '\0';
        wc_resolve_channel_prompt(buf,
                                  s1 ? s1 + 1 : "",
                                  s2 ? s2 + 1 : "");
    }
    return 0;
}
