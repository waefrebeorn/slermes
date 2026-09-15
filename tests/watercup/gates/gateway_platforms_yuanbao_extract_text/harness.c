/* gate: gateway_platforms_yuanbao_extract_text
 * The REAL port src/gateway/yuanbao_ports.c::yb2_extract_text (+ helpers,
 * FIXED from an empty-string stub this wave) is LINKED via extra_src and
 * called per input; the WS-lifecycle symbols it references are stubbed here
 * (not on the traced path). Twin traces the same ladder over LIVE
 * gateway/platforms/yuanbao.py::ExtractContentMiddleware._extract_text.
 *
 * Input encoding: one JSON MsgBody array per argv item.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1 body parses && is array
 *   LOOP per elem:
 *     B2  elem is dict
 *     B3  TIMTextElem (text nonempty appended when truthy)
 *     B4  TIMImageElem: B5 array-is-list B6 idx1-dict B7 idx0-dict B8 rid
 *     B9  TIMFileElem: B10 rid B11 filename
 *     B12 TIMSoundElem: B13 rid
 *     B14 TIMVideoFileElem: B15 rid
 *     B16 TIMCustomElem: B17 data-nonempty B18 parses B19 is-object
 *         B20 ctype=1002 B21 ctype=1010 B22 ctype=1007 B23 lu-text-found
 *         B24 ctype=1009
 *     B25 TIMFaceElem: B26 data-nonempty B27 face-name
 *     B28 other nonempty etype
 *   Value event: extracted text.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

char *yb2_extract_text(const char *msg_body_json);

/* stubs: referenced by yuanbao_ports.c WS lifecycle fns, never traced */
bool yuanbao_is_running(void) { return false; }
int yuanbao_init(void) { return 0; }
int yuanbao_start(void) { return 0; }
void yuanbao_stop(void) {}

static void trace_one(const char *in) {
    WCUP_BEGIN("yb2_extract_text");

    json_t *body = in ? json_parse(in, NULL) : NULL;
    int is_arr = body && body->type == JSON_ARRAY;
    WCUP_BRANCH(is_arr);                                            /* B1 */

    if (is_arr) {
        for (size_t i = 0; i < body->c.count; i++) {
            WCUP_LOOP_TICK();
            json_t *elem = body->c.items[i];
            WCUP_BRANCH(elem && elem->type == JSON_OBJECT);         /* B2 */
            if (!elem || elem->type != JSON_OBJECT) continue;
            json_t *et_v = json_obj_get(elem, "msg_type");
            const char *etype = (et_v && et_v->type == JSON_STRING) ? et_v->str_val : "";
            json_t *content = json_obj_get(elem, "msg_content");
            int content_obj = content && content->type == JSON_OBJECT;

#define GETS(k, def) (content_obj ? json_get_str(content, k, def) : (def))

            if (strcmp(etype, "TIMTextElem") == 0) {                /* B3 */
                WCUP_BRANCH(1);
                const char *text = GETS("text", "");
                WCUP_BRANCH(text && text[0]);
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(etype, "TIMImageElem") == 0) {               /* B4 */
                WCUP_BRANCH(1);
                json_t *arr = content_obj ? json_obj_get(content, "image_info_array") : NULL;
                int arr_list = arr && arr->type == JSON_ARRAY;
                WCUP_BRANCH(arr_list);                              /* B5 */
                int pick1 = 0, pick0 = 0;
                if (arr_list) {
                    pick1 = arr->c.count > 1 && json_get(arr, 1) &&
                            json_get(arr, 1)->type == JSON_OBJECT;
                    WCUP_BRANCH(pick1);                             /* B6 */
                    if (!pick1)
                        pick0 = arr->c.count > 0 && json_get(arr, 0) &&
                                json_get(arr, 0)->type == JSON_OBJECT;
                    WCUP_BRANCH(pick0);                             /* B7 */
                } else {
                    WCUP_BRANCH(0); WCUP_BRANCH(0);
                }
                const char *raw_url = "";
                if ((pick1 || pick0) && arr) {
                    json_t *info = json_get(arr, pick1 ? 1 : 0);
                    const char *u = json_get_str(info, "url", "");
                    raw_url = u ? u : "";
                }
                int has_rid = strstr(raw_url, "resourceId=") != NULL ||
                              strstr(raw_url, "resourceid=") != NULL;
                WCUP_BRANCH(has_rid);                               /* B8 */
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(etype, "TIMFileElem") == 0) {                /* B9 */
                WCUP_BRANCH(1);
                const char *furl = GETS("url", "");
                int has_rid = furl && (strstr(furl, "resourceId=") ||
                                       strstr(furl, "resourceid="));
                WCUP_BRANCH(has_rid);                               /* B10 */
                const char *fn = GETS("file_name", NULL);
                if (!fn) fn = GETS("fileName", NULL);
                if (!fn) fn = GETS("filename", NULL);
                WCUP_BRANCH(fn && fn[0]);                           /* B11 */
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(etype, "TIMSoundElem") == 0) {               /* B12 */
                WCUP_BRANCH(1);
                const char *surl = GETS("url", "");
                WCUP_BRANCH(surl && (strstr(surl, "resourceId=") ||
                                     strstr(surl, "resourceid="))); /* B13 */
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(etype, "TIMVideoFileElem") == 0) {           /* B14 */
                WCUP_BRANCH(1);
                const char *vurl = GETS("url", "");
                WCUP_BRANCH(vurl && (strstr(vurl, "resourceId=") ||
                                     strstr(vurl, "resourceid="))); /* B15 */
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(etype, "TIMCustomElem") == 0) {              /* B16 */
                WCUP_BRANCH(1);
                const char *data_val = GETS("data", "");
                WCUP_BRANCH(data_val && data_val[0]);               /* B17 */
                if (data_val && data_val[0]) {
                    json_t *custom = json_parse(data_val, NULL);
                    WCUP_BRANCH(custom != NULL);                    /* B18 */
                    if (custom) {
                        WCUP_BRANCH(custom->type == JSON_OBJECT);   /* B19 */
                        if (custom->type == JSON_OBJECT) {
                            json_t *ct_v = json_obj_get(custom, "elem_type");
                            long ct = (ct_v && ct_v->type == JSON_NUMBER)
                                          ? (long)ct_v->num_val : -1;
                            WCUP_BRANCH(ct == 1002);                /* B20 */
                            if (ct != 1002) {
                                WCUP_BRANCH(ct == 1010);            /* B21 */
                                if (ct != 1010) {
                                    WCUP_BRANCH(ct == 1007);        /* B22 */
                                    if (ct == 1007) {
                                        json_t *cv = json_obj_get(custom, "content");
                                        int has_c = cv && cv->type == JSON_STRING &&
                                                    cv->str_val[0];
                                        json_t *parsed = has_c ? json_parse(cv->str_val, NULL) : NULL;
                                        const char *lk = parsed ?
                                            json_get_str(parsed, "link", NULL) : NULL;
                                        WCUP_BRANCH(lk && lk[0]);   /* B23 */
                                        if (parsed) json_free(parsed);
                                    } else {
                                        WCUP_BRANCH(ct == 1009);    /* B24 */
                                    }
                                }
                            }
                        }
                        json_free(custom);
                    } else {
                        WCUP_BRANCH(0);                             /* B19 */
                    }
                }
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(etype, "TIMFaceElem") == 0) {                /* B25 */
                WCUP_BRANCH(1);
                const char *raw_data = GETS("data", "");
                WCUP_BRANCH(raw_data && raw_data[0]);               /* B26 */
                int named = 0;
                if (raw_data && raw_data[0]) {
                    json_t *fd = json_parse(raw_data, NULL);
                    if (fd && fd->type == JSON_OBJECT) {
                        const char *n = json_get_str(fd, "name", "");
                        if (n) {
                            while (*n == ' '||*n=='\t'||*n=='\n'||*n=='\r') n++;
                            size_t l = strlen(n);
                            while (l && strchr(" \t\n\r", n[l-1])) l--;
                            named = l > 0;
                        }
                    }
                    if (fd) json_free(fd);
                }
                WCUP_BRANCH(named);                                 /* B27 */
                continue;
            }
            WCUP_BRANCH(0);
            WCUP_BRANCH(etype[0] != '\0');                          /* B28 */
        }
    }

    char *out = yb2_extract_text(in);
    WCUP_VALSTR(out ? out : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb2_extract_text");
    free(out);
    if (body) json_free(body);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s yb2_extract_text <msg-body-json>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
