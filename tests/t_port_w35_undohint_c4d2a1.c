/* JSON-lines harness for the w35_undohint_c4d2a1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w35_undohint_c4d2a1.h"

static void emit(json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "epoch")) {
            out = w35_undohint_c4d2a1_epoch(json_obj_get(in, "value"));
        } else if (!strcmp(func, "named_files")) {
            out = w35_undohint_c4d2a1_named_files(json_obj_get(in, "files"));
        } else {
            json_free(in);
            continue;
        }
        printf("{\"func\":");
        emit(json_string((char *)func));
        printf(",\"case\":");
        emit(json_obj_get(in, "case"));
        printf(",\"out\":");
        emit(out);
        printf(",\"value\":");
        emit(json_obj_get(in, "value"));
        printf(",\"files\":");
        emit(json_obj_get(in, "files"));
        printf("}\n");
        json_free(in);
    }
    return 0;
}
