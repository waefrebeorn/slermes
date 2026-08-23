/* gate: platforms_base_resolve_channel_skills — drives the REAL
 * src/hermes_cli/platforms_base_helpers.c::gw_base__resolve_channel_skills
 * (PoP: gateway/platforms/base.py::resolve_channel_skills).
 *
 * Input encoding: "<channel_id|-><parent_id|-><extra_json>"
 *   "-" means NULL/absent.
 *
 * Coarse rung ladder around the real C call (must match twin.py EXACTLY):
 *   B1 bindings is a nonempty list
 *   B2 ids_to_check nonempty
 *   per entry: LOOP tick, B(dict), B(id match),
 *     B(skill string), B(skill list nonempty),
 *     per name: LOOP tick, B(name is str), B(nm fresh)
 *   V(result: comma-joined skills, or "null")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "json.h"

/* link stubs: symbols required by platforms_base_helpers.c but off the
 * traced path (URL-media caching / config loading in other functions). */
void *http_new(void) { return 0; }
void *http_get(void *a, char *b) { (void)a; (void)b; return 0; }
void http_free(void *a) { (void)a; }
void http_resp_free(void *a) { (void)a; }
void *hermes_config_load(const char *a) { (void)a; return 0; }

char **gw_base__resolve_channel_skills(const char *extra_json,
                                       const char *channel_id,
                                       const char *parent_id);

static const char *tok_or_null(const char *t) { return strcmp(t, "-") == 0 ? NULL : t; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_resolve_channel_skills <ch|par|json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[2048]; snprintf(probe, sizeof probe, "%s", argv[i]);
        char *p1 = strchr(probe, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (!p1 || !p2) { fprintf(stderr, "bad probe\n"); return 2; }
        *p1 = '\0'; *p2 = '\0';
        const char *channel = tok_or_null(probe);
        const char *parent = tok_or_null(p1 + 1);
        const char *extra_json = p2 + 1;

        WCUP_BEGIN("resolve_channel_skills");
        json_t *extra = strcmp(extra_json, "null") == 0 ? NULL : json_parse(extra_json, NULL);
        json_t *bindings = NULL;
        WCUP_BRANCH(extra && (bindings = json_obj_get(extra, "channel_skill_bindings")) != NULL &&
                    bindings->type == JSON_ARRAY && json_len(bindings) > 0);   /* B1 */
        if (bindings && bindings->type == JSON_ARRAY && json_len(bindings) > 0) {
            WCUP_BRANCH((channel && *channel) || (parent && *parent));          /* B2 */
            size_t n = json_len(bindings);
            for (size_t e = 0; e < n; e++) {
                WCUP_LOOP_TICK();
                json_t *entry = json_get(bindings, e);
                WCUP_BRANCH(entry && entry->type == JSON_OBJECT);
                if (!entry || entry->type != JSON_OBJECT) continue;
                json_t *idj = json_obj_get(entry, "id");
                const char *eid = (idj && idj->type == JSON_STRING && idj->str_val) ? idj->str_val : "";
                int match = ((channel && *channel && strcmp(eid, channel) == 0) ||
                             (parent && *parent && strcmp(eid, parent) == 0));
                WCUP_BRANCH(match);
                if (!match) continue;
                json_t *sk = json_obj_get(entry, "skills");
                if (!sk) sk = json_obj_get(entry, "skill");
                WCUP_BRANCH(sk && sk->type == JSON_STRING);                     /* B str */
                if (sk && sk->type == JSON_STRING) {
                    /* V emitted below via real-call result */
                    break;
                }
                WCUP_BRANCH(sk && sk->type == JSON_ARRAY && json_len(sk) > 0);  /* B list */
                if (!(sk && sk->type == JSON_ARRAY && json_len(sk) > 0)) {
                    /* LIVE keeps scanning later bindings when this one has no
                     * usable skills — keep walking like the twin. */
                    continue;
                }
                size_t m = json_len(sk);
                for (size_t j = 0; j < m; j++) {
                    WCUP_LOOP_TICK();
                    json_t *s = json_get(sk, j);
                    int is_str = s && s->type == JSON_STRING;
                    WCUP_BRANCH(is_str);
                    if (is_str)
                        WCUP_BRANCH(s->str_val && s->str_val[0]);
                }
                break;
            }
        }
        char **out = gw_base__resolve_channel_skills(extra_json, channel, parent);
        char vs[1024]; size_t off = 0;
        if (!out) { strcpy(vs, "null"); }
        else {
            for (int k = 0; out[k] && off < sizeof vs - 2; k++) {
                if (k) vs[off++] = ',';
                for (const char *q = out[k]; *q && off < sizeof vs - 2; q++) vs[off++] = *q;
            }
            vs[off] = '\0';
        }
        WCUP_VALSTR(vs);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_channel_skills");
        if (extra) json_free(extra);
    }
    return 0;
}
