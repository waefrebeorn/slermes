/*
 * chat_launch.c — C port of upstream Hermes hermes_cli/chat_launch.py.
 *
 * PoP: w8_chatlaunch_31c924_short_resume_id @ hermes_cli/chat_launch.py:_short_resume_id
 * PoP: w8_chatlaunch_31c924_branch_lineage @ hermes_cli/chat_launch.py:_branch_lineage
 *
 * Faithful port of the two pure epilogue helpers: the 22-char canonical-local
 * resume-id shortening (only when the prefix resolves uniquely back to the
 * session, and the store is readable) and the /branch ancestor walk with a
 * seen set. Store lookups / failures are injected through the fixture.
 */
#include "noc_w8_chatlaunch_31c924.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

struct json_t *w8_chatlaunch_31c924_short_resume_id(const char *session_id,
                                                    const struct json_t *resolve_map,
                                                    int resolve_error) {
    if (!session_id) return json_string("");
    size_t len = strlen(session_id);
    if (strncmp(session_id, "local-", 6) != 0 || len <= 22)
        return json_string(session_id);
    char short_id[23];
    memcpy(short_id, session_id, 22);
    short_id[22] = '\0';
    if (resolve_error) return json_string(session_id);
    const char *resolved = json_string_value(json_obj_get(resolve_map, short_id));
    if (resolved && !strcmp(resolved, session_id)) return json_string(short_id);
    return json_string(session_id);
}

struct json_t *w8_chatlaunch_31c924_branch_lineage(const struct json_t *sessions,
                                                   const char *start_id) {
    struct json_t *rows = json_array();
    if (!start_id || !*start_id) return rows;
    json_append(rows, json_string(start_id));

    char seen[64][256];
    size_t nseen = 0;
    snprintf(seen[nseen++], 256, "%s", start_id);

    for (;;) {
        const char *cur = json_string_value(json_get(rows, json_len(rows) - 1));
        struct json_t *node = json_obj_get(sessions, cur);
        if (!node || node->type != JSON_OBJECT) break;
        const char *parent = json_get_str(node, "_branched_from", "");
        if (!parent || !*parent) break;
        int already = 0;
        for (size_t i = 0; i < nseen; i++)
            if (!strcmp(seen[i], parent)) { already = 1; break; }
        if (already) break;
        if (!json_obj_get(sessions, parent)) break; /* sessions.get(parent) -> None */
        if (nseen < 64) snprintf(seen[nseen++], 256, "%s", parent);
        json_append(rows, json_string(parent));
    }
    return rows;
}
