/* gate: gateway_platforms_api_server_normalize_multimodal_content
 * Calls the REAL port aps_normalize_multimodal_content(content_json) per input
 * and cups the canonical value derived from its JSON output. No branch ladder:
 * the real output IS the behavior under test, and a value-only cup is what the
 * twin (which renders the same canon) can reproduce exactly.
 *
 * canon:
 *   error object  -> "ERR|<message>"
 *   json string   -> "STR|<value>"
 *   json array    -> "PARTS|t:<text>;i:<url>|<detail-or->"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"

extern char *aps_normalize_multimodal_content(const char *content_json);

static void canon_from_output(const char *out_json, char *val, size_t valsz) {
    json_t *o = out_json ? json_parse(out_json, NULL) : NULL;
    val[0] = '\0';
    if (!o) { snprintf(val, valsz, "ERR|<null>"); return; }
    if (o->type == JSON_OBJECT) {
        json_t *e = json_obj_get(o, "error");
        snprintf(val, valsz, "ERR|%s", (e && e->type == JSON_STRING) ? e->str_val : "?");
    } else if (o->type == JSON_STRING) {
        snprintf(val, valsz, "STR|%s", o->str_val ? o->str_val : "");
    } else if (o->type == JSON_ARRAY) {
        size_t w = 0;
        w += (size_t)snprintf(val + w, valsz - w, "PARTS|");
        for (size_t i = 0; i < o->c.count && w < valsz; i++) {
            json_t *p = o->c.items[i];
            const char *ty = "-";
            if (p && p->type == JSON_OBJECT) {
                json_t *t = json_obj_get(p, "type");
                if (t && t->type == JSON_STRING) ty = t->str_val;
            }
            if (strcmp(ty, "text") == 0) {
                json_t *tx = json_obj_get(p, "text");
                w += (size_t)snprintf(val + w, valsz - w, "%st:%s", i ? ";" : "",
                                      (tx && tx->type == JSON_STRING) ? tx->str_val : "?");
            } else {
                json_t *iu = json_obj_get(p, "image_url");
                json_t *u = iu ? json_obj_get(iu, "url") : NULL;
                json_t *d = iu ? json_obj_get(iu, "detail") : NULL;
                w += (size_t)snprintf(val + w, valsz - w, "%si:%s|%s", i ? ";" : "",
                                      (u && u->type == JSON_STRING) ? u->str_val : "?",
                                      d ? ((d->type == JSON_STRING) ? d->str_val : "?") : "-");
            }
        }
    } else {
        snprintf(val, valsz, "OTHER|%s", out_json);
    }
    json_free(o);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <content-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("aps_normalize_multimodal_content");
        fflush(stdout);
        char *out = aps_normalize_multimodal_content(argv[i]);
        char val[8192];
        canon_from_output(out, val, sizeof val);
        free(out);
        WCUP_VALSTR(val);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("aps_normalize_multimodal_content");
    }
    return 0;
}
