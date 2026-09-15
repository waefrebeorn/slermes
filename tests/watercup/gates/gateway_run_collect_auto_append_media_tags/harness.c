/* gate: gateway_run_collect_auto_append_media_tags — traced copy of
 * src/gateway/run_pure2.c::gw_collect_auto_append_media_tags.
 * LIVE twin: gateway/run.py::_collect_auto_append_media_tags.
 *
 * Shared (outcome-level) rung ladder — per-message/per-rung internals are NOT
 * evented because the C port scans every tool message while LIVE layers a
 * producer-tool allowlist (tool_call_id -> name map) and an
 * extension-anchored regex on top; see gate.json notes:
 *   B1 messages_json is not a JSON array -> no tags
 *   per collected tag: V-event with the tag text
 *   B2 any [[audio_as_voice]] directive present?
 *   V-event "True"/"False"
 *
 * Inputs restrict every tool message to a producer-allowlisted tool declared
 * through an assistant tool_calls entry, with extension-anchored absolute
 * MEDIA paths — the intersection where C's naive token scan and LIVE's
 * allowlist+regex pipeline agree on both the tag list and order.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "json.h"
#include "watercup.h"

static void wc_collect(const char *messages_json) {
    WCUP_BEGIN("collect_auto_append_media_tags");
    bool has_voice = false;
    char joined[8192];
    size_t jlen = 0;
    joined[0] = '\0';
    json_t *root = json_parse(messages_json ? messages_json : "", NULL);
    if (!root || root->type != JSON_ARRAY) {                    /* B1 */
        WCUP_BRANCH(1);
        if (root) json_free(root);
        goto out;
    }
    WCUP_BRANCH(0);
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *m = root->c.items[i];
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "tool") != 0 && strcmp(role, "function") != 0) continue;
        const char *content = json_get_str(m, "content", "");
        const char *p = content;
        while ((p = strstr(p, "MEDIA:")) != NULL) {
            p += 6;
            const char *start = p;
            while (*p && *p != ' ' && *p != '"' && *p != '}' &&
                   *p != '\n' && *p != '\r') p++;
            size_t len = (size_t)(p - start);
            if (len > 4) {
                char tag[4096];
                snprintf(tag, sizeof tag, "MEDIA:%.*s", (int)len, start);
                WCUP_VALSTR(tag);
                int w = snprintf(joined + jlen, sizeof joined - jlen,
                                 "%s%s", (jlen ? "\n" : ""), tag);
                if (w > 0) jlen += (size_t)w;
            }
            if (*p == '\0') break;
        }
        if (strstr(content, "[[audio_as_voice]]")) has_voice = true;
    }
    json_free(root);
out:
    WCUP_BRANCH(has_voice ? 1 : 0);                             /* B2 */
    WCUP_VALSTR(has_voice ? "True" : "False");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("collect_auto_append_media_tags");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s collect_auto_append_media_tags <messages_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_collect(argv[i]);
    return 0;
}
