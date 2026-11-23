/*
 * run_background.c — C port of upstream Hermes gateway/run_background.py
 * (gate w8_runbackgrou_99f3d0).
 *
 * PoP: runbackgrou_bg_prompt_preview @ gateway/run_background.py:_bg_prompt_preview
 * PoP: runbackgrou_complete_preview @ gateway/run_background.py:_run_background_task_inner
 * PoP: runbackgrou_media_image_paths @ gateway/run_background.py:_run_background_task_inner
 */
#include "noc_w8_runbackgrou_99f3d0.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char ELLIPSIS[] = "\xe2\x80\xa6";

/* " ".join(str(prompt or "").split()) */
static char *collapse_ws(const char *prompt) {
    const char *s = prompt ? prompt : "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;
    size_t k = 0;
    size_t i = 0;
    int need_space = 0;
    while (i < n) {
        while (i < n && isspace((unsigned char)s[i])) i++;
        if (i >= n) break;
        size_t start = i;
        while (i < n && !isspace((unsigned char)s[i])) i++;
        if (need_space) out[k++] = ' ';
        memcpy(out + k, s + start, i - start);
        k += i - start;
        need_space = 1;
    }
    out[k] = '\0';
    return out;
}

json_t *runbackgrou_bg_prompt_preview(const char *prompt, int limit) {
    char *text = collapse_ws(prompt);
    if (!text) return json_string("");
    size_t n = strlen(text);
    if ((double)n <= (double)limit) {
        json_t *out = json_string(text);
        free(text);
        return out;
    }
    long cut = (long)limit - 1;
    if (cut < 0) cut = (long)n + cut;
    if (cut < 0) cut = 0;
    if (cut > (long)n) cut = (long)n;
    while (cut > 0 && isspace((unsigned char)text[cut - 1])) cut--;
    size_t total = (size_t)cut + sizeof(ELLIPSIS) - 1;
    char *buf = (char *)malloc(total + 1);
    if (!buf) { free(text); return json_null(); }
    memcpy(buf, text, (size_t)cut);
    memcpy(buf + cut, ELLIPSIS, sizeof(ELLIPSIS) - 1);
    buf[total] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    free(text);
    return out;
}

json_t *runbackgrou_complete_preview(const char *prompt) {
    const char *s = prompt ? prompt : "";
    size_t n = strlen(s);
    if (n <= 60) return json_string(s);
    char *buf = (char *)malloc(64);
    if (!buf) return json_null();
    memcpy(buf, s, 60);
    memcpy(buf + 60, "...", 4);
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *runbackgrou_media_image_paths(const json_t *urls, const json_t *media_types) {
    json_t *out = json_array();
    if (!json_is_array(urls)) return out;
    size_t nu = json_len(urls);
    size_t nt = json_is_array(media_types) ? json_len(media_types) : 0;
    for (size_t i = 0; i < nu; i++) {
        json_t *u = json_get(urls, i);
        const char *t = "";
        if (i < nt) {
            json_t *tv = json_get(media_types, i);
            if (tv && tv->type == JSON_STRING) t = tv->str_val;
        }
        if (!strncmp(t, "image/", 6) && u && u->type == JSON_STRING)
            json_append(out, json_copy(u));
    }
    return out;
}
