/* gate: plugin_index_parse_entries — traced faithful copy of
 * src/hermes_cli/plugin_index_pure.c::pindex_parse_entries over lib/libjson.
 * LIVE twin: hermes_cli/plugin_index.py::_parse_entries.
 *
 * Trace ladder: dict-vs-list shape branch, per-item loop, item-object branch,
 * name-validity branch, repo-validity branch; value event per accepted entry
 * ("name=<n>|repo=<r>", both stripped) and a final count value.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

static bool valid_repo(const char *repo) {
    if (!repo || !*repo) return false;
    int slashes = 0;
    for (const char *p = repo; *p; p++) if (*p == '/') slashes++;
    if (slashes != 1) return false;
    const char *first = strchr(repo, '/');
    if (first == repo) return false;
    if (*(first + 1) == '\0') return false;
    return true;
}

static void strip_ws(const char *in, char *out, size_t cap) {
    const char *s = in;
    while (*s == ' ' || *s == '\t') s++;
    snprintf(out, cap, "%s", s);
    size_t L = strlen(out);
    while (L > 0 && (out[L-1] == ' ' || out[L-1] == '\t')) out[--L] = 0;
}

static void trace_one(const char *raw_json) {
    WCUP_BEGIN("parse_entries");
    json_t *raw = json_parse(raw_json, NULL);
    int accepted = 0;
    bool is_dict = raw && raw->type == JSON_OBJECT;
    WCUP_BRANCH(is_dict);
    json_t *items = NULL;
    if (is_dict) {
        json_t *pl = json_object_get(raw, "plugins");
        items = pl;
        WCUP_BRANCH(pl != NULL);
        if (!pl) items = json_array();
        else if (pl->type != JSON_ARRAY) { /* excluded input class */ items = NULL; }
    } else {
        bool is_list = raw && raw->type == JSON_ARRAY;
        WCUP_BRANCH(is_list);
        items = is_list ? raw : NULL;
    }
    if (items && items->type == JSON_ARRAY) {
        size_t n = json_len(items);
        for (size_t i = 0; i < n; i++) {
            WCUP_LOOP_TICK();
            json_t *item = json_array_get(items, i);
            if (!item || item->type != JSON_OBJECT) { WCUP_BRANCH(false); continue; }
            WCUP_BRANCH(true);
            const char *name = json_get_str(item, "name", "");
            char ns[512]; strip_ws(name, ns, sizeof ns);
            bool name_ok = ns[0] != '\0';
            WCUP_BRANCH(name_ok);
            if (!name_ok) continue;
            const char *repo = json_get_str(item, "repo", "");
            char rs[512]; strip_ws(repo, rs, sizeof rs);
            bool repo_ok = valid_repo(rs);
            WCUP_BRANCH(repo_ok);
            if (!repo_ok) continue;
            accepted++;
            char vbuf[1100];
            snprintf(vbuf, sizeof vbuf, "name=%s|repo=%s", ns, rs);
            WCUP_VALSTR(vbuf);
        }
    }
    if (items && !is_dict && items != raw) { /* unreachable safety */ }
    /* NOTE: child nodes (plugins) are owned by their parent object; only the
     * synthetic empty array we created needs an explicit free. */
    json_t *synthetic = NULL;
    (void)synthetic;
    if (raw) json_free(raw);
    char cnt[32];
    snprintf(cnt, sizeof cnt, "count=%d", accepted);
    WCUP_VALSTR(cnt);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_entries");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_entries <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
