/*
 * t_port_plugin_guard.c — parity harness for src/tools/plugin_guard.c
 * (tools/plugin_guard.py).
 *
 * Ops (JSON "ops", argv[1]):
 *   {"kind":"scan","dir":"caseN","name":"my-plugin","source":"owner/repo"}
 * Plugin dir resolves against $ORACLE_FIXDIR (set by the oracle runner).
 * Mirrors tests/sta_oracle_plugin_guard.py; scanned_at is excluded from the
 * envelope (wall-clock, nondeterministic).
 */
#include "hermes_json.h"
#include "plugin_guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    const char *fixdir = getenv("ORACLE_FIXDIR");
    if (!fixdir || !*fixdir) fixdir = ".";
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "scan") == 0) {
                char dir[2048];
                snprintf(dir, sizeof dir, "%s/%s", fixdir,
                         json_get_str(c, "dir", ""));
                pg_plugin_scan_result_t *r = plugin_guard_scan_plugin(
                    dir, json_get_str(c, "source", ""));
                json_t *fa = json_new_array();
                for (int k = 0; k < r->finding_count; k++) {
                    json_t *fo = json_new_object();
                    pg_finding_t *fd = &r->findings[k];
                    json_object_set(fo, "pattern_id",
                                    json_string(fd->pattern_id));
                    json_object_set(fo, "severity", json_string(fd->severity));
                    json_object_set(fo, "category", json_string(fd->category));
                    json_object_set(fo, "file", json_string(fd->file));
                    json_object_set(fo, "line", json_int(fd->line));
                    json_object_set(fo, "match", json_string(fd->match));
                    json_object_set(fo, "description",
                                    json_string(fd->description));
                    json_array_append(fa, fo);
                }
                json_object_set(o, "verdict", json_string(r->verdict));
                json_object_set(o, "trust_level", json_string(r->trust_level));
                json_object_set(o, "summary", json_string(r->summary));
                json_object_set(o, "findings", fa);

                int allow0 = 0, allow1 = 0;
                char reason0[512], reason1[512];
                should_allow_plugin_install(r, 0, &allow0, reason0,
                                            sizeof reason0);
                should_allow_plugin_install(r, 1, &allow1, reason1,
                                            sizeof reason1);
                json_object_set(o, "allowed_null_forced",
                                allow0 == -1 ? json_string("None")
                                             : (allow0 ? json_string("True")
                                                       : json_string("False")));
                json_object_set(o, "reason", json_string(reason0));
                json_object_set(o, "force_allowed",
                                json_bool(allow1 == 1));
                plugin_guard_scan_result_free(r);
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
