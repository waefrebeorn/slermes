/*
 * Oracle harness: gateway/display_config.py  (_normalise / resolve_display_setting)
 * vs LIVE Python. Reads a self-describing fixture JSON from argv[1]:
 *   {"mode":"normalise","setting":S,"value":<json>}
 *   {"mode":"resolve","user_config":{...},"platform_key":P,"setting":S,"fallback":<json|absent>}
 * Prints the canonical JSON result (json.dumps(ensure_ascii=False) form).
 */
#include "hermes_json.h"
#include "gateway/port_display_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture.json>\n", argv[0]); return 2; }
    char *txt = read_file(argv[1]);
    if (!txt) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    json_t *root = json_parse(txt, NULL);
    free(txt);
    if (!root || root->type != JSON_OBJECT) {
        fprintf(stderr, "fixture parse failed\n");
        return 1;
    }

    const char *mode = json_get_str(root, "mode", NULL);
    char *out = NULL;

    if (mode && strcmp(mode, "normalise") == 0) {
        const char *setting = json_get_str(root, "setting", NULL);
        const json_t *value = json_obj_get(root, "value");
        out = display_config__normalise(setting, value);
    } else if (mode && strcmp(mode, "resolve") == 0) {
        const json_t *user_config = json_obj_get(root, "user_config");
        const char *platform_key = json_get_str(root, "platform_key", NULL);
        const char *setting = json_get_str(root, "setting", NULL);
        const json_t *fallback = json_obj_get(root, "fallback");
        out = display_config__resolve(user_config, platform_key, setting, fallback);
    } else {
        fprintf(stderr, "unknown mode '%s'\n", mode ? mode : "(null)");
        json_free(root);
        return 1;
    }

    json_free(root);
    if (out) { printf("%s\n", out); free(out); }
    return 0;
}
