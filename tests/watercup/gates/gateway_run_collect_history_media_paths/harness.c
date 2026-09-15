/* gate: gateway_run_collect_history_media_paths — traced copy of
 * src/gateway/run_pure2.c::gw_collect_history_media_paths.
 * LIVE twin: gateway/run.py::_collect_history_media_paths.
 *
 * Outcome-level rung ladder (per-message internals not evented: LIVE layers
 * assistant-message scanning, tool_call_id maps and extract_media on top of
 * the C port's plain tool/function token scan — see gate.json notes):
 *   B1 agent_history_json is not a JSON array -> no paths
 *   V-event per unique media path (canonical SORTED order so the C port's
 *   first-seen order and the LIVE set compare equal)
 *   final V: comma-joined sorted path list
 *
 * Inputs are tool/function-only histories with extension-anchored absolute
 * MEDIA paths, where both implementations agree on the deduped path set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"
#include "watercup.h"

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void wc_collect(const char *agent_history_json) {
    WCUP_BEGIN("collect_history_media_paths");
    char *paths[256];
    int n = 0;
    json_t *root = json_parse(agent_history_json ? agent_history_json : "", NULL);
    if (!root || root->type != JSON_ARRAY) {                    /* B1 */
        WCUP_BRANCH(1);
        if (root) json_free(root);
        goto emit;
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
                char tmp[4096];
                snprintf(tmp, sizeof tmp, "%.*s", (int)len, start);
                bool dup = false;
                for (int j = 0; j < n; j++)
                    if (strcmp(paths[j], tmp) == 0) { dup = true; break; }
                if (!dup && n < 256)
                    paths[n++] = strdup(tmp);
            }
            if (*p == '\0') break;
        }
    }
    json_free(root);
emit:
    qsort(paths, (size_t)n, sizeof(char *), cmp_str);           /* canonical order */
    char joined[8192];
    size_t jlen = 0;
    joined[0] = '\0';
    for (int i = 0; i < n; i++) {
        WCUP_VALSTR(paths[i]);
        int w = snprintf(joined + jlen, sizeof joined - jlen, "%s%s",
                         (i ? "," : ""), paths[i]);
        if (w > 0) jlen += (size_t)w;
        free(paths[i]);
    }
    WCUP_VALSTR(joined);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("collect_history_media_paths");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s collect_history_media_paths <history_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_collect(argv[i]);
    return 0;
}
