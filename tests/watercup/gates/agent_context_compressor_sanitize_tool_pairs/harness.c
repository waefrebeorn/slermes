/* gate: agent_context_compressor_sanitize_tool_pairs
 * The REAL port src/agent/context_compressor_ports.c::cc_sanitize_tool_pairs
 * (FIXED from a passthrough stub this wave) is LINKED via extra_src and called
 * per input; helper symbols from context_compressor_pure.c are stubbed here
 * (not on the traced path). Twin traces the same ladder over LIVE
 * agent/context_compressor.py::ContextCompressor._sanitize_tool_pairs.
 *
 * Input encoding: one JSON message array per argv item.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1 messages parse && array
 *   LOOP surviving-scan: B2 assistant / B3 tool_calls / LOOP+tick B4 cid
 *   LOOP results-scan:   B5 tool / B6 tool_call_id
 *   B7 orphaned results exist -> result-drop pass
 *   B8 missing results exist -> call-strip pass
 *   LOOP walk-back: B9 trailing-tool skip;  B10 trailing assistant found
 *   LOOP strip-pass: B11 assistant / B12 not-trailing / B13 has-calls /
 *     LOOP+tick B14 cid-missing / B15 changed / B16 kept-nonempty /
 *     B17 content-blank
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

char *cc_sanitize_tool_pairs(const char *messages_json);

/* stubs: helpers referenced by other fns in context_compressor_ports.c */
void *strip_think_blocks(void) { return 0; }

static const char *stp_role(json_t *m) {
    json_t *r = json_obj_get(m, "role");
    return (r && r->type == JSON_STRING) ? r->str_val : NULL;
}

static int stp_in(char **set, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(set[i], s) == 0) return 1;
    return 0;
}

static void tc_id_of(json_t *tc, char *out, size_t sz) {
    out[0] = '\0';
    if (!tc || tc->type != JSON_OBJECT) return;
    const char *cid = json_get_str(tc, "call_id", "");
    if (!cid || !cid[0]) cid = json_get_str(tc, "id", "");
    if (cid) snprintf(out, sz, "%s", cid);
}

/* canonical value derived from input + sanitized output (same on both sides) */
static void build_val(const char *in_json, const char *out_json,
                      char *val, size_t valsz) {
    json_t *in = in_json ? json_parse(in_json, NULL) : NULL;
    json_t *out = out_json ? json_parse(out_json, NULL) : NULL;
    size_t n_in = (in && in->type == JSON_ARRAY) ? in->c.count : 0;
    size_t n_out = (out && out->type == JSON_ARRAY) ? out->c.count : 0;
    int strips = 0;
    char roles[2048] = "";
    size_t w = 0;
    for (size_t i = 0; out && i < n_out && w < sizeof roles - 8; i++) {
        json_t *m = out->c.items[i];
        const char *role = "-";
        int blanked = 0;
        if (m && m->type == JSON_OBJECT) {
            const char *r = stp_role(m);
            if (r) role = r;
            const char *ct = json_get_str(m, "content", NULL);
            if (ct && strcmp(ct, "(tool call removed)") == 0) {
                blanked = 1;
                strips++;
            }
        }
        w += (size_t)snprintf(roles + w, sizeof roles - w, "%s%s%d",
                              i ? "," : "", role, blanked);
    }
    snprintf(val, valsz, "n=%zu|drop=%zu|strip=%d|%s", n_out, n_in - n_out,
             strips, roles);
    if (out) json_free(out);
    if (in) json_free(in);
}

static void trace_one(const char *in) {
    WCUP_BEGIN("cc_sanitize_tool_pairs");

    json_t *msgs = in ? json_parse(in, NULL) : NULL;
    int is_arr = msgs && msgs->type == JSON_ARRAY;
    WCUP_BRANCH(is_arr);                                            /* B1 */

    if (is_arr) {
        /* surviving scan */
        for (size_t i = 0; i < msgs->c.count; i++) {
            WCUP_LOOP_TICK();
            json_t *m = msgs->c.items[i];
            const char *role = (m && m->type == JSON_OBJECT) ? stp_role(m) : NULL;
            WCUP_BRANCH(role && strcmp(role, "assistant") == 0);     /* B2 */
            if (!role || strcmp(role, "assistant") != 0) continue;
            json_t *tcs = json_obj_get(m, "tool_calls");
            WCUP_BRANCH(tcs && tcs->type == JSON_ARRAY && tcs->c.count); /* B3 */
            if (!tcs || tcs->type != JSON_ARRAY || !tcs->c.count) continue;
            for (size_t j = 0; j < tcs->c.count; j++) {
                WCUP_LOOP_TICK();
                char cid[256];
                tc_id_of(json_get(tcs, j), cid, sizeof cid);
                WCUP_BRANCH(cid[0] != '\0');                        /* B4 */
            }
        }
        /* results scan */
        for (size_t i = 0; i < msgs->c.count; i++) {
            WCUP_LOOP_TICK();
            json_t *m = msgs->c.items[i];
            const char *role = (m && m->type == JSON_OBJECT) ? stp_role(m) : NULL;
            WCUP_BRANCH(role && strcmp(role, "tool") == 0);          /* B5 */
            if (!role || strcmp(role, "tool") != 0) continue;
            const char *cid = json_get_str(m, "tool_call_id", "");
            WCUP_BRANCH(cid && cid[0]);                             /* B6 */
        }

        /* orphaned / missing sets (computed off input, mirroring the port) */
        char **surv = calloc(msgs->c.count * 8 + 8, sizeof(char *));
        char **res = calloc(msgs->c.count + 1, sizeof(char *));
        size_t nsurv = 0, nres = 0;
        for (size_t i = 0; i < msgs->c.count; i++) {
            json_t *m = msgs->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            const char *role = stp_role(m);
            if (role && strcmp(role, "assistant") == 0) {
                json_t *tcs = json_obj_get(m, "tool_calls");
                if (tcs && tcs->type == JSON_ARRAY)
                    for (size_t j = 0; j < tcs->c.count; j++) {
                        char cid[256];
                        tc_id_of(json_get(tcs, j), cid, sizeof cid);
                        if (cid[0]) surv[nsurv++] = strdup(cid);
                    }
            } else if (role && strcmp(role, "tool") == 0) {
                const char *cid = json_get_str(m, "tool_call_id", "");
                if (cid && cid[0]) res[nres++] = strdup(cid);
            }
        }
        int orphaned = 0, missing = 0;
        for (size_t i = 0; i < nres; i++)
            if (!stp_in(surv, nsurv, res[i])) orphaned = 1;
        for (size_t i = 0; i < nsurv; i++)
            if (!stp_in(res, nres, surv[i])) missing = 1;

        WCUP_BRANCH(orphaned);                                      /* B7 */
        WCUP_BRANCH(missing);                                       /* B8 */

        if (missing) {
            long idx = (long)msgs->c.count - 1;
            while (idx >= 0) {
                WCUP_LOOP_TICK();
                json_t *m = msgs->c.items[idx];
                const char *role = (m && m->type == JSON_OBJECT) ? stp_role(m) : NULL;
                WCUP_BRANCH(role && strcmp(role, "tool") == 0);      /* B9 */
                if (!(role && strcmp(role, "tool") == 0)) break;
                idx--;
            }
            json_t *trailing = NULL;
            if (idx >= 0) {
                json_t *m = msgs->c.items[idx];
                const char *role = (m && m->type == JSON_OBJECT) ? stp_role(m) : NULL;
                if (role && strcmp(role, "assistant") == 0) trailing = m;
            }
            WCUP_BRANCH(trailing != NULL);                          /* B10 */

            for (size_t i = 0; i < msgs->c.count; i++) {
                WCUP_LOOP_TICK();
                json_t *m = msgs->c.items[i];
                const char *role = (m && m->type == JSON_OBJECT) ? stp_role(m) : NULL;
                WCUP_BRANCH(role && strcmp(role, "assistant") == 0); /* B11 */
                if (!role || strcmp(role, "assistant") != 0) continue;
                WCUP_BRANCH(m != trailing);                         /* B12 */
                if (m == trailing) continue;
                json_t *tcs = json_obj_get(m, "tool_calls");
                WCUP_BRANCH(tcs && tcs->type == JSON_ARRAY && tcs->c.count); /* B13 */
                if (!tcs || tcs->type != JSON_ARRAY || !tcs->c.count) continue;
                size_t nkept = 0;
                for (size_t j = 0; j < tcs->c.count; j++) {
                    WCUP_LOOP_TICK();
                    char cid[256];
                    tc_id_of(json_get(tcs, j), cid, sizeof cid);
                    int miss = cid[0] && stp_in(surv, nsurv, cid) && !stp_in(res, nres, cid);
                    WCUP_BRANCH(miss);                              /* B14 */
                    if (!miss) nkept++;
                }
                WCUP_BRANCH(nkept != tcs->c.count);                 /* B15 */
                if (nkept == tcs->c.count) continue;
                WCUP_BRANCH(nkept > 0);                             /* B16 */
                if (nkept > 0) continue;
                const char *content = json_get_str(m, "content", NULL);
                WCUP_BRANCH(!content || !content[0] ||
                            strspn(content, " \t\n\r") == strlen(content)); /* B17 */
            }
        }

        for (size_t i = 0; i < nsurv; i++) free(surv[i]);
        for (size_t i = 0; i < nres; i++) free(res[i]);
        free(surv);
        free(res);
    }

    char *out = cc_sanitize_tool_pairs(in);
    char val[4096];
    build_val(in, out, val, sizeof val);

    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cc_sanitize_tool_pairs");
    free(out);
    if (msgs) json_free(msgs);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s cc_sanitize_tool_pairs <messages-json>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
