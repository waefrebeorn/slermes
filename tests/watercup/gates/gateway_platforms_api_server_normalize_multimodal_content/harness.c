/* gate: gateway_platforms_api_server_normalize_multimodal_content
 * The REAL port src/gateway/api_server_ports.c::aps_normalize_multimodal_content
 * (+ aps_normalize_chat_content) is LINKED via extra_src and called per input;
 * this harness traces its branch ladder around the live call and derives the
 * canonical value from the REAL port's output. Twin mirrors the same rungs on
 * gateway/platforms/api_server.py::_normalize_multimodal_content.
 *
 * Input encoding: one JSON content value per argv item.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1 content is None
 *   B2 content is str
 *   B3 content is list
 *   LOOP over capped items:
 *     B4 part is str / B5 nonempty
 *     B6 part is dict
 *     B7 text-part type / B8 text missing(None) / B9 text truthy
 *     B10 image-part type / B11 image_ref is dict / B12 url invalid(ERR)
 *     B13 data: URL / B14 valid image data URL
 *     B15 http(s) URL (else ERR)
 *     B16 detail present / B17 detail invalid(ERR)
 *     B18 file-part type (ERR) / B19 unknown type (ERR)
 *   B20 any error / B21 no parts / B22 all-text collapse
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"
#include "hermes_json.h"

char *aps_normalize_multimodal_content(const char *content_json);

static const char *const TEXT_TYPES[] = {"text", "input_text", "output_text", NULL};
static const char *const IMAGE_TYPES[] = {"image_url", "input_image", NULL};
static const char *const FILE_TYPES[] = {"file", "input_file", NULL};

static int type_in(const char *const *types, const char *s) {
    for (int i = 0; types[i]; i++)
        if (strcmp(types[i], s) == 0) return 1;
    return 0;
}

static void lower_of(json_t *v, char *out, size_t sz) {
    out[0] = '\0';
    if (v && v->type == JSON_STRING) {
        snprintf(out, sz, "%s", v->str_val);
        for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    }
}

/* canonical value from the REAL port's JSON output */
static void canon_from_output(const char *out_json, char *val, size_t valsz) {
    json_t *o = out_json ? json_parse(out_json, NULL) : NULL;
    if (!o) { snprintf(val, valsz, "ERR|<null>"); return; }
    if (o->type == JSON_OBJECT) {
        json_t *e = json_obj_get(o, "error");
        snprintf(val, valsz, "ERR|%s",
                 (e && e->type == JSON_STRING) ? e->str_val : "?");
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
                w += (size_t)snprintf(val + w, valsz - w, "%st:%s",
                                      i ? ";" : "",
                                      (tx && tx->type == JSON_STRING) ? tx->str_val : "?");
            } else {
                json_t *iu = json_obj_get(p, "image_url");
                json_t *u = iu ? json_obj_get(iu, "url") : NULL;
                json_t *d = iu ? json_obj_get(iu, "detail") : NULL;
                w += (size_t)snprintf(val + w, valsz - w, "%si:%s|%s",
                                      i ? ";" : "",
                                      (u && u->type == JSON_STRING) ? u->str_val : "?",
                                      d ? ((d->type == JSON_STRING) ? d->str_val : "?") : "-");
            }
        }
    } else {
        snprintf(val, valsz, "OTHER|%s", out_json);
    }
    json_free(o);
}

static void trace_one(const char *in) {
    WCUP_BEGIN("aps_normalize_multimodal_content");

    /* --- trace the ladder off the INPUT, exactly as the port decides --- */
    json_t *content = in ? json_parse(in, NULL) : NULL;
    int any_error = 0, nparts = 0, all_text = 1;

    WCUP_BRANCH(!content || content->type == JSON_NULL);            /* B1 */
    WCUP_BRANCH(content && content->type == JSON_STRING);           /* B2 */
    if (content && content->type == JSON_ARRAY) {                   /* B3 */
        WCUP_BRANCH(1);
        size_t n = content->c.count;
        if (n > 1000) n = 1000;
        for (size_t i = 0; i < n; i++) {
            WCUP_LOOP_TICK();
            json_t *part = content->c.items[i];
            if (part && part->type == JSON_STRING) {                /* B4 */
                WCUP_BRANCH(1);
                WCUP_BRANCH(part->str_val && part->str_val[0]);     /* B5 */
                nparts++;
                continue;
            }
            WCUP_BRANCH(0);
            if (!part || part->type != JSON_OBJECT) {               /* B6 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            char ptype[128];
            lower_of(json_obj_get(part, "type"), ptype, sizeof ptype);
            if (type_in(TEXT_TYPES, ptype)) {                       /* B7 */
                WCUP_BRANCH(1);
                json_t *text = json_obj_get(part, "text");
                int missing = !text || text->type == JSON_NULL;
                WCUP_BRANCH(missing);                               /* B8 */
                int truthy = 0;
                if (!missing) {
                    switch (text->type) {
                    case JSON_BOOL: truthy = text->bool_val; break;
                    case JSON_NUMBER: truthy = text->num_val != 0.0; break;
                    case JSON_STRING: truthy = text->str_val[0] != '\0'; break;
                    default: truthy = text->c.count > 0; break;
                    }
                }
                WCUP_BRANCH(truthy);                                /* B9 */
                if (truthy) nparts++;
                continue;
            }
            WCUP_BRANCH(0);
            if (type_in(IMAGE_TYPES, ptype)) {                      /* B10 */
                WCUP_BRANCH(1);
                json_t *detail_v = json_obj_get(part, "detail");
                int detail_nonstring = 0;
                char detail[256] = "";
                json_t *image_ref = json_obj_get(part, "image_url");
                json_t *url_value;
                WCUP_BRANCH(image_ref && image_ref->type == JSON_OBJECT); /* B11 */
                if (image_ref && image_ref->type == JSON_OBJECT) {
                    url_value = json_obj_get(image_ref, "url");
                    json_t *d2 = json_obj_get(image_ref, "detail");
                    if (d2) detail_v = d2;
                } else {
                    url_value = image_ref;
                }
                if (detail_v && detail_v->type == JSON_STRING)
                    snprintf(detail, sizeof detail, "%s", detail_v->str_val);
                else if (detail_v && detail_v->type != JSON_NULL)
                    detail_nonstring = 1;
                int url_bad = !url_value || url_value->type != JSON_STRING ||
                              !url_value->str_val[0];
                WCUP_BRANCH(url_bad);                               /* B12 */
                if (url_bad) { any_error = 1; break; }
                const char *us = url_value->str_val;
                while (*us == ' ' || *us == '\t' || *us == '\n' || *us == '\r') us++;
                size_t ul = strlen(us);
                while (ul && strchr(" \t\n\r", us[ul - 1])) ul--;
                char u[2048];
                snprintf(u, sizeof u, "%.*s", (int)ul, us);
                for (char *p = u; *p; p++) *p = (char)tolower((unsigned char)*p);
                int is_data = strncmp(u, "data:", 5) == 0;
                WCUP_BRANCH(is_data);                               /* B13 */
                if (is_data) {
                    int okimg = strncmp(u, "data:image/", 11) == 0 &&
                                strchr(url_value->str_val, ',');
                    WCUP_BRANCH(okimg);                             /* B14 */
                    if (!okimg) { any_error = 1; break; }
                } else {
                    int is_http = strncmp(u, "http://", 7) == 0 ||
                                  strncmp(u, "https://", 8) == 0;
                    WCUP_BRANCH(is_http);                           /* B15 */
                    if (!is_http) { any_error = 1; break; }
                }
                int have_detail = detail[0] != '\0' || detail_nonstring;
                WCUP_BRANCH(have_detail);                           /* B16 */
                if (have_detail) {
                    int bad = detail_nonstring || !detail[0] ||
                              strspn(detail, " \t\n\r") == strlen(detail);
                    WCUP_BRANCH(bad);                               /* B17 */
                    if (bad) { any_error = 1; break; }
                }
                nparts++;
                all_text = 0;
                continue;
            }
            WCUP_BRANCH(0);
            if (type_in(FILE_TYPES, ptype)) {                       /* B18 */
                WCUP_BRANCH(1);
                any_error = 1;
                break;
            }
            WCUP_BRANCH(0);
            WCUP_BRANCH(1);                                         /* B19 unknown */
            any_error = 1;
            break;
        }
        WCUP_BRANCH(any_error);                                     /* B20 */
        if (!any_error) {
            WCUP_BRANCH(nparts == 0);                               /* B21 */
            if (nparts) WCUP_BRANCH(all_text);                      /* B22 */
        }
    } else {
        WCUP_BRANCH(0);
    }

    /* --- REAL port output supplies the value --- */
    char *out = aps_normalize_multimodal_content(in);
    char val[8192];
    canon_from_output(out, val, sizeof val);

    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aps_normalize_multimodal_content");
    free(out);
    if (content) json_free(content);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s aps_normalize_multimodal_content <content-json>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
