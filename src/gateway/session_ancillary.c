/*
 * session_ancillary.c — C port of upstream Hermes gateway/session_ancillary.py.
 *
 * PoP: w12_sessionanc_9a64bb_subagent_row @ gateway/session_ancillary.py:subagent_row
 * PoP: w12_sessionanc_9a64bb_reachable_upload_home @ gateway/session_ancillary.py:_reachable_upload_home
 *
 * Pure read-side projections: no event lock, no staging I/O, no process registry.
 */
#include "noc_w12_sessionanc_9a64bb.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SUBAGENT_FIELDS[] = {
    "subagent_id", "parent_id", "depth", "goal", "delegation_id", "model",
    "started_at", "status", "tool_count", "last_tool", "accepting_steer",
};

json_t *w12_sessionanc_9a64bb_subagent_row(const struct json_t *record) {
    json_t *out = json_object();
    for (size_t i = 0; i < sizeof(SUBAGENT_FIELDS) / sizeof(SUBAGENT_FIELDS[0]); i++) {
        const json_t *v = json_obj_get(record, SUBAGENT_FIELDS[i]);
        json_set(out, SUBAGENT_FIELDS[i], v ? json_copy(v) : json_null());
    }
    const json_t *agent = json_obj_get(record, "agent");
    const json_t *sid = json_obj_get(agent, "session_id");
    int ok = sid && sid->type == JSON_STRING && sid->str_val && sid->str_val[0];
    json_set(out, "child_session_id", ok ? json_copy(sid) : json_null());
    return out;
}

/* ntpath.normcase(Path(p).resolve()) for absolute paths. Returns a malloc'd string. */
static char *resolve_normcase(const char *src) {
    if (!src) src = "";
    size_t n = strlen(src);
    char *work = (char *)malloc(n + 1);
    if (!work) return strdup("");
    for (size_t i = 0; i < n; i++) work[i] = (src[i] == '/') ? '\\' : src[i];
    work[n] = '\0';
    char parts[64][512];
    size_t count = 0;
    char drive[8] = "";
    char *tok = strtok(work, "\\");
    while (tok && count < 64) {
        if (tok[1] == ':' && tok[2] == '\0') snprintf(drive, sizeof(drive), "%s", tok);
        else if (!strcmp(tok, ".") || !tok[0]) { /* skip */ }
        else if (!strcmp(tok, "..")) { if (count > 0) count--; }
        else { snprintf(parts[count], sizeof(parts[count]), "%s", tok); count++; }
        tok = strtok(NULL, "\\");
    }
    free(work);
    size_t cap = 8 + count * 513 + 2;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    size_t k = 0;
    if (drive[0]) {
        out[k++] = (char)tolower((unsigned char)drive[0]);
        out[k++] = ':'; out[k++] = '\\';
    }
    for (size_t i = 0; i < count; i++) {
        if (i) out[k++] = '\\';
        for (size_t j = 0; parts[i][j]; j++) out[k++] = (char)tolower((unsigned char)parts[i][j]);
    }
    if (k == 0) out[k++] = '.';
    out[k] = '\0';
    return out;
}

json_t *w12_sessionanc_9a64bb_reachable_upload_home(const struct json_t *authority_home,
                                                     const struct json_t *host_scoped,
                                                     const struct json_t *siblings,
                                                     const struct json_t *raw_home) {
    if (!raw_home || raw_home->type != JSON_STRING || !raw_home->str_val || !raw_home->str_val[0])
        return json_bool(0);
    char *target = resolve_normcase(raw_home->str_val);
    int found = 0;
    char *own = resolve_normcase(authority_home && authority_home->type == JSON_STRING ? authority_home->str_val : "");
    if (!strcmp(target, own)) found = 1;
    free(own);
    if (!found && host_scoped && host_scoped->type == JSON_BOOL && host_scoped->bool_val && json_is_array(siblings)) {
        for (size_t i = 0; i < json_len(siblings); i++) {
            const json_t *sib = json_get(siblings, i);
            const char *sp = (sib && sib->type == JSON_STRING) ? sib->str_val : "";
            char *cand = resolve_normcase(sp);
            if (!strcmp(target, cand)) { found = 1; free(cand); break; }
            free(cand);
        }
    }
    free(target);
    return json_bool(found ? 1 : 0);
}

