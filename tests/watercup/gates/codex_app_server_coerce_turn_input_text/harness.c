/* gate: codex_app_server_coerce_turn_input_text — traced copy of
 * src/agent/codex_app_server_session_ports.c::cas2_coerce_turn_input_text
 * (REWRITTEN to match LIVE). LIVE twin:
 * agent/transports/codex_app_server_session.py::_coerce_turn_input_text.
 *
 * Ladder:
 *   B1 string passthrough / B2 list walk (B3 str+B4 blank, B5 obj+
 *      B6 text-type+B7 value, B8 image-type, B9 null-skip, B10 scalar)
 *      / B11 empty->default question / B12 null / B13 other scalar.
 *
 * Inputs are JSON-encoded values.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../../lib/libjson/json.h"
#include "watercup.h"

static int str_blank(const char *s) {
    for (; *s; s++)
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return 0;
    return 1;
}

#define EMIT(out) do { \
    WCUP_VALSTR(out); \
    char cup_[32]; \
    WCUP_FINALIZE(cup_, sizeof cup_); \
    printf("%s\n", cup_); \
    WCUP_END("coerce_turn_input_text"); \
} while (0)

static void coerce(const char *content_json) {
    WCUP_BEGIN("coerce_turn_input_text");
    if (!content_json) { EMIT(""); return; }
    json_t *root = json_parse(content_json, NULL);
    if (!root) { EMIT(""); return; }

    if (root->type == JSON_STRING) {                       /* B1 */
        WCUP_BRANCH(1);
        EMIT(root->str_val);
        json_free(root);
        return;
    }
    WCUP_BRANCH(0);
    if (root->type == JSON_ARRAY) {                        /* B2 */
        WCUP_BRANCH(1);
        char out[4096]; out[0] = '\0';
        size_t n = json_len(root);
        for (size_t i = 0; i < n; i++) {
            WCUP_LOOP_TICK();
            json_t *item = json_get(root, i);
            if (!item) continue;
            const char *piece = NULL;
            char numbuf[32];
            if (item->type == JSON_STRING) {               /* B3 */
                WCUP_BRANCH(1); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
                if (str_blank(item->str_val)) { WCUP_BRANCH(1); continue; }  /* B4 */
                WCUP_BRANCH(0);
                piece = item->str_val;
            } else if (item->type == JSON_OBJECT) {        /* B5 */
                WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
                json_t *t = json_obj_get(item, "type");
                const char *ty = (t && t->type == JSON_STRING) ? t->str_val : NULL;
                int is_text = ty && (strcmp(ty, "text") == 0 || strcmp(ty, "input_text") == 0);
                int is_img = ty && (strcmp(ty, "image") == 0 || strcmp(ty, "image_url") == 0 ||
                                    strcmp(ty, "input_image") == 0);
                if (is_text) {                             /* B6 */
                    WCUP_BRANCH(1); WCUP_BRANCH(0);
                    json_t *tv = json_obj_get(item, "text");
                    json_t *cv = json_obj_get(item, "content");
                    const char *s = NULL;
                    if (tv && tv->type == JSON_STRING && tv->str_val[0]) s = tv->str_val;
                    else if (cv && cv->type == JSON_STRING && cv->str_val[0]) s = cv->str_val;
                    if (s && *s) { WCUP_BRANCH(1); piece = s; }              /* B7 */
                    else WCUP_BRANCH(0);
                } else if (is_img) {                       /* B8 */
                    WCUP_BRANCH(0); WCUP_BRANCH(1);
                    piece = "[image attached]";
                } else {
                    WCUP_BRANCH(0); WCUP_BRANCH(0);
                }
            } else if (item->type == JSON_NULL) {          /* B9 */
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_BRANCH(0); WCUP_BRANCH(0);
                continue;
            } else {                                       /* B10 scalar */
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_BRANCH(0);
                if (item->type == JSON_NUMBER) {
                    snprintf(numbuf, sizeof numbuf, "%lld", (long long)item->num_val);
                    piece = numbuf;
                } else if (item->type == JSON_BOOL) {
                    piece = item->bool_val ? "True" : "False";
                }
            }
            if (piece && *piece) {
                if (*out) strcat(out, "\n\n");
                strcat(out, piece);
            }
        }
        /* strip */
        char *s = out;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        memmove(out, s, strlen(s) + 1);
        size_t L = strlen(out);
        while (L > 0 && (out[L-1]==' '||out[L-1]=='\t'||out[L-1]=='\n'||out[L-1]=='\r')) out[--L] = '\0';
        if (!*out) { WCUP_BRANCH(1); strcpy(out, "What do you see in this image?"); }   /* B11 */
        else WCUP_BRANCH(0);
        EMIT(out);
        json_free(root);
        return;
    }
    WCUP_BRANCH(0);
    if (root->type == JSON_NULL) {                         /* B12 */
        WCUP_BRANCH(1); WCUP_BRANCH(0);
        EMIT("");
    } else {                                               /* B13 scalar */
        WCUP_BRANCH(0); WCUP_BRANCH(1);
        if (root->type == JSON_NUMBER) {
            char buf[32]; snprintf(buf, sizeof buf, "%lld", (long long)root->num_val);
            EMIT(buf);
        } else if (root->type == JSON_BOOL) {
            EMIT(root->bool_val ? "True" : "False");
        } else {
            EMIT("");
        }
    }
    json_free(root);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coerce_turn_input_text <json>...\n", argv[0]); return 2; }
    for (int k = 2; k < argc; k++) coerce(argv[k]);
    return 0;
}
