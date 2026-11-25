/* JSON-lines harness for the w27_adapter_74f618_p4 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p4.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "resolve_audio_ext")) out = w27_p4_resolve_audio_ext(in);
        else if (!strcmp(func, "is_voice_clip")) out = w27_p4_is_voice_clip(in);
        else if (!strcmp(func, "attachment_label")) out = w27_p4_attachment_label(in);
        else if (!strcmp(func, "file_kind")) out = w27_p4_file_kind(in);
        else if (!strcmp(func, "describe_api_error")) out = w27_p4_describe_api_error(in);
        else if (!strcmp(func, "describe_download_failure")) out = w27_p4_describe_download_failure(in);
        else if (!strcmp(func, "retryable_upload_error")) out = w27_p4_retryable_upload_error(in);
        else out = json_string("UNKNOWN_FUNC");
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
