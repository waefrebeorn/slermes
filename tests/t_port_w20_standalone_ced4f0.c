/* JSON-lines harness for the w20_standalone_ced4f0 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * Media entries are relative names; this harness materialises the ones that are
 * supposed to exist under SLERMES_HOME (the isolated dir the gate hands the
 * harness and the oracle), then passes the absolute path -- so the C side does a
 * real filesystem check and a missing SLERMES_HOME shows up as a wrong answer
 * instead of a NULL dereference. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "noc_w20_standalone_ced4f0.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *home_dir(void) {
    const char *home = getenv("SLERMES_HOME");
    return (home && home[0]) ? home : ".";
}

/* Absolute path for a fixture media name, creating parents + the file unless
 * the name is under "missing/" (the fixture's way of saying "not on disk"). */
static char *materialise(const char *rel, char *out, size_t outsz) {
    const char *home = home_dir();
    if (!rel || !rel[0]) return NULL;
    snprintf(out, outsz, "%s/%s", home, rel);
    if (strncmp(rel, "missing/", 8) != 0) {
        char dir[4096];
        char *slash;
        FILE *fh;
        snprintf(dir, sizeof(dir), "%s", out);
        slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            mkdir(dir, 0777);
        }
        fh = fopen(out, "ab");
        if (fh) fclose(fh);
    }
    return out;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "send")) {
            json_t *media = json_obj_get(in, "media");
            json_t *abs_media = json_array();
            json_t *httpx = json_obj_get(in, "httpx_available");
            size_t i;
            for (i = 0; media && media->type == JSON_ARRAY && i < media->c.count; i++) {
                json_t *entry = media->c.items[i];
                json_t *pair = json_array();
                char path[4096];
                const char *rel = NULL;
                if (entry && entry->type == JSON_ARRAY && entry->c.count > 0 &&
                    entry->c.items[0]->type == JSON_STRING)
                    rel = entry->c.items[0]->str_val;
                if (rel && materialise(rel, path, sizeof(path)))
                    json_append(pair, json_string(path));
                else
                    json_append(pair, json_null());
                json_append(pair, (entry && entry->type == JSON_ARRAY && entry->c.count > 1)
                                      ? json_copy(entry->c.items[1]) : json_bool(0));
                json_append(abs_media, pair);
            }
            out = w20_standalone_ced4f0_send(json_obj_get(in, "extra"),
                                             json_get_str(in, "chat_id", ""),
                                             abs_media,
                                             json_obj_get(in, "post"),
                                             json_obj_get(in, "estimates"),
                                             (httpx && httpx->type == JSON_BOOL) ? (httpx->bool_val ? 1 : 0) : 1);
            json_free(abs_media);
        } else if (!strcmp(func, "is_rate_limit")) {
            out = json_bool(w20_standalone_ced4f0_is_rate_limit(json_obj_get(in, "error")) ? 1 : 0);
        } else if (!strcmp(func, "retry_after")) {
            double secs = 0.0;
            if (w20_standalone_ced4f0_retry_after(json_obj_get(in, "error"), &secs))
                out = json_number(secs);
            else
                out = json_null();
        } else if (!strcmp(func, "send_timeout")) {
            out = json_number(w20_standalone_ced4f0_send_timeout((int)json_get_num(in, "attachments", 0)));
        } else if (!strcmp(func, "chat_id")) {
            char *c = w20_standalone_ced4f0_chat_id(json_get_str(in, "chat_id", ""));
            out = json_string(c ? c : "");
            free(c);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
