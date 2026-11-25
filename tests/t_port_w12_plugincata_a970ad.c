/* t_port_w12_plugincata_a970ad.c — JSON-lines oracle harness for hermes_cli/plugin_catalog.py */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_plugincata_a970ad.h"

static void emit_key(const char *key, json_t *v)
{
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void)
{
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *url = json_get_str(in, "url", "");
        json_t *out = NULL;

        if (!strcmp(func, "is_allowed_image_url"))
            out = json_bool(w12_plugincata_a970ad_is_allowed_image_url(url));
        else if (!strcmp(func, "is_https_url"))
            out = json_bool(w12_plugincata_a970ad_is_https_url(url));
        else if (!strcmp(func, "normalize_repo"))
            out = w12_plugincata_a970ad_normalize_repo(url);

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
