/* gate: agent_replay_cleanup_strip_interrupted_tool_tails — traced copy of
 * src/agent/replay_cleanup.c::agent_replay_cleanup_strip_interrupted_tool_tails
 * (the real file IS linked via extra_src for json support + sibling fns; the
 * traced copy mirrors its branch ladder rung-for-rung). LIVE twin:
 * agent/replay_cleanup.py::strip_interrupted_tool_tails with LIVE
 * is_interrupted_tool_result / tool_may_have_side_effect semantics.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty history -> returned as-is
 *   LOOP messages:
 *     B2: assistant with tool_calls
 *       LOOP tool-result scan (B3 role==tool)
 *       B4: has results && any interrupted
 *         LOOP calls (B5 may-side-effect per call)
 *         B6: any call may side-effect -> RECOVER vs strip
 *           LOOP results: B7 keep-non-interrupted; V disposition on enrich
 *     B8: orphan interrupted tool result -> dropped
 *   append otherwise
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

/* faithful copies of replay_cleanup.c's static helpers */
static bool rc_is_no_effect_tool(const char *name)
{
    if (!name || !name[0]) return false;
    char low[128];
    size_t n = 0;
    for (const char *p = name; *p && n + 1 < sizeof(low); p++)
        low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    static const char *no_effect[] = {
        "read_file", "search_files", "session_search", "skill_view", "skills_list",
        "web_extract", "web_search", "vision_analyze", "browser_snapshot",
        "browser_get_images", "browser_console", "read_terminal", NULL
    };
    for (int i = 0; no_effect[i]; i++)
        if (strcmp(low, no_effect[i]) == 0) return true;
    return false;
}
static bool rc_may_side_effect(const char *name) { return !rc_is_no_effect_tool(name); }

static const char *INTERRUPTED_MARK = "[command interrupted]";
static bool rc_interrupted(const char *content)
{
    if (!content) return false;
    char low[8192];
    size_t n = 0;
    for (const char *p = content; *p && n + 1 < sizeof(low); p++)
        low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    if (strstr(low, INTERRUPTED_MARK)) return true;
    if (strstr(low, "exit_code") && (strstr(low, "130") || strstr(low, "-1")))
        return strstr(low, "interrupt") != NULL;
    return false;
}

static void content_str(const json_t *msg, char *out, size_t cap)
{
    out[0] = '\0';
    if (!msg) return;
    json_t *c = json_obj_get((json_t *)msg, "content");
    if (c && c->type == JSON_STRING && c->str_val)
        snprintf(out, cap, "%s", c->str_val);
}

static void call_name(const json_t *msg, size_t idx, char *out, size_t cap)
{
    snprintf(out, cap, "");
    json_t *tcs = json_obj_get((json_t *)msg, "tool_calls");
    if (!tcs || tcs->type != JSON_ARRAY || idx >= tcs->c.count) return;
    json_t *call = json_get((json_t *)tcs, idx);
    json_t *fn = call ? json_obj_get(call, "function") : NULL;
    json_t *nm = fn ? json_obj_get(fn, "name") : NULL;
    if (nm && nm->type == JSON_STRING && nm->str_val)
        snprintf(out, cap, "%s", nm->str_val);
}

#define RECOVER_UNKNOWN "[Orphan recovery: interrupted side-effecting tool may have executed; its effect is UNKNOWN. Inspect state before retrying.]"
#define RECOVER_NONE    "[Orphan recovery: interrupted read-only tool did not complete.]"

static bool wc_strip(json_t *history)
{
    WCUP_BEGIN("strip_interrupted_tool_tails");
    if (!history || history->type != JSON_ARRAY || history->c.count == 0) {
        WCUP_BRANCH(1);                       /* B1 */
        goto out;
    }
    WCUP_BRANCH(0);
    {
        size_t n = history->c.count;
        size_t i = 0;
        while (i < n) {
            WCUP_LOOP_TICK();
            json_t *msg = json_get(history, i);
            char role[32] = "";
            if (msg) {
                json_t *rj = json_obj_get(msg, "role");
                if (rj && rj->type == JSON_STRING && rj->str_val)
                    snprintf(role, sizeof(role), "%s", rj->str_val);
            }
            bool asst_tc = strcmp(role, "assistant") == 0 && msg &&
                           json_obj_get(msg, "tool_calls") != NULL;
            WCUP_BRANCH(asst_tc);             /* B2 */
            if (asst_tc) {
                size_t j = i + 1;
                bool has_tool = false, any_int = false;
                for (; j < n; j++) {
                    WCUP_LOOP_TICK();         /* B3 loop */
                    json_t *t = json_get(history, j);
                    char tr[32] = "";
                    if (t) {
                        json_t *rj = json_obj_get(t, "role");
                        if (rj && rj->type == JSON_STRING && rj->str_val)
                            snprintf(tr, sizeof(tr), "%s", rj->str_val);
                    }
                    bool is_tool = strcmp(tr, "tool") == 0;
                    WCUP_BRANCH(is_tool);     /* B3 */
                    if (!is_tool) break;
                    has_tool = true;
                    char cbuf[4096];
                    content_str(t, cbuf, sizeof cbuf);
                    if (rc_interrupted(cbuf)) any_int = true;
                }
                bool block = has_tool && any_int;
                WCUP_BRANCH(block);           /* B4 */
                if (block) {
                    bool may = false;
                    json_t *tcs = json_obj_get(msg, "tool_calls");
                    size_t nt = tcs && tcs->type == JSON_ARRAY ? tcs->c.count : 0;
                    for (size_t k = 0; k < nt; k++) {
                        WCUP_LOOP_TICK();     /* B5 loop */
                        char nm[128];
                        call_name(msg, k, nm, sizeof nm);
                        bool se = rc_may_side_effect(nm);
                        WCUP_BRANCH(se);      /* B5 */
                        if (se) { may = true; break; }
                    }
                    WCUP_BRANCH(may);         /* B6 recover vs strip */
                    if (may) {
                        for (size_t k = i + 1; k < j; k++) {
                            WCUP_LOOP_TICK();
                            json_t *t = json_get(history, k);
                            char cbuf[4096];
                            content_str(t, cbuf, sizeof cbuf);
                            bool keep = !rc_interrupted(cbuf);
                            WCUP_BRANCH(keep);          /* B7 */
                            if (keep) continue;
                            /* find this result's call name */
                            char cid[128] = "", nm2[128] = "";
                            json_t *cidj = t ? json_obj_get(t, "tool_call_id") : NULL;
                            if (cidj && cidj->type == JSON_STRING && cidj->str_val)
                                snprintf(cid, sizeof cid, "%s", cidj->str_val);
                            for (size_t c = 0; c < nt; c++) {
                                char cn[128], ccid[128] = "";
                                call_name(msg, c, cn, sizeof cn);
                                json_t *cc = json_get(tcs, c);
                                json_t *cj = cc ? json_obj_get(cc, "id") : NULL;
                                if (cj && cj->type == JSON_STRING && cj->str_val)
                                    snprintf(ccid, sizeof ccid, "%s", cj->str_val);
                                if (strcmp(ccid, cid) == 0) { snprintf(nm2, sizeof nm2, "%s", cn); break; }
                            }
                            const char *disp = rc_may_side_effect(nm2) ? "unknown" : "none";
                            WCUP_VALSTR(disp);
                        }
                    }
                    i = j;
                    continue;
                }
            }
            char cbuf2[4096];
            content_str(msg, cbuf2, sizeof cbuf2);
            bool orphan = strcmp(role, "tool") == 0 && rc_interrupted(cbuf2);
            WCUP_BRANCH(orphan);              /* B8 */
            if (orphan) { i++; continue; }
            i++;
        }
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("strip_interrupted_tool_tails");
        return true;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s strip_interrupted_tool_tails <history_json>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        json_t *h = json_parse(argv[i], NULL);
        wc_strip(h);
        if (h) json_free(h);
    }
    return 0;
}
