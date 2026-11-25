/* t_port_w12_setuptermi_48d5ea.c — JSON-lines harness for hermes_cli/setup_terminal.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_setuptermi_48d5ea.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "report_binary")) {
            out = w12_setuptermi_48d5ea_report_binary(
                json_get_str(in, "found", NULL),
                json_get_str(in, "missing", ""),
                json_get_str(in, "install_hint", ""),
                json_get_str(in, "found_prefix", "Found: "));
        } else if (!strcmp(func, "existing_secret_keeps")) {
            out = w12_setuptermi_48d5ea_existing_secret_keeps(
                json_get_str(in, "env_value", NULL),
                json_get_str(in, "label", ""),
                json_get_str(in, "question", ""),
                json_get_bool(in, "prompt_answer", false) ? 1 : 0);
        } else if (!strcmp(func, "prompt_secret_env")) {
            out = w12_setuptermi_48d5ea_prompt_secret_env(
                json_get_str(in, "value", NULL),
                json_get_str(in, "label", ""),
                json_get_str(in, "env_var", ""),
                json_get_str(in, "confirm_msg", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
