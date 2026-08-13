/* Oracle harness: tools/skill_linter.py lint_content vs LIVE Python.
 *
 * Reads a JSON fixture (argv[1]) whose top-level is either a bare array of
 * SkillLintCase records or an object {"cases":[...]}. For each case:
 *   - If skill_dir is null (and no fixture_files): lint with skill_dir=NULL
 *     (content-only checks).
 *   - Otherwise: create a per-case temp dir, populate from fixture_files +
 *     fixture_scripts, and lint with that dir.
 * Emits JSON: [{"case","n","has_errors","skill_dir":"tmpdir"|"" ,"findings":[...]}]
 *
 * The Python oracle (sta_oracle_skill_linter.py) recreates the same temp dir
 * from the same fixture fields and compares finding (severity,rule,message).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hermes_json.h"
#include "skill_linter.h"

static void mkdir_p(const char *path)
{
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    for (size_t i = 1; i < len; i++)
        if (tmp[i] == '/') {
            char save = tmp[i + 1];
            tmp[i + 1] = '\0';
            mkdir(tmp, 0755);
            tmp[i + 1] = save;
        }
    mkdir(tmp, 0755);
}

static char *make_temp_dir(const char *tag)
{
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("TMPDIR");
    if (!home) home = "/tmp";
    static unsigned long counter = 0;
    char *base = malloc(1024);
    if (!base) return NULL;
    snprintf(base, 1024, "%s/lint_%s_%lu_%ld", home, tag, counter++, (long)getpid());
    mkdir_p(base);
    return base;
}

static void write_file_at(const char *dir, const char *rel, const char *content)
{
    char full[2048];
    snprintf(full, sizeof(full), "%s/%s", dir, rel);
    char *dup = strdup(full);
    char *slash = strrchr(dup, '/');
    if (slash) { *slash = '\0'; mkdir_p(dup); }
    free(dup);
    FILE *f = fopen(full, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void write_obj_files(const char *dir, json_t *obj, const char *prefix)
{
    if (!obj || obj->type != JSON_OBJECT) return;
    for (size_t k = 0; k < obj->c.count; k++) {
        json_t *v = obj->c.items[k];
        if (!v || v->type != JSON_STRING) continue;
        char rel[512];
        if (prefix && *prefix)
            snprintf(rel, sizeof(rel), "%s/%s", prefix, obj->c.keys[k]);
        else
            snprintf(rel, sizeof(rel), "%s", obj->c.keys[k]);
        write_file_at(dir, rel, v->str_val);
    }
}

static void json_escape(const char *s)
{
    for (; *s; s++) {
        unsigned char uc = *(unsigned char *)s;
        if (*s == '"')  fputs("\\\"", stdout);
        else if (*s == '\\') fputs("\\\\", stdout);
        else if (*s == '\n') fputs("\\n", stdout);
        else if (*s == '\r') fputs("\\r", stdout);
        else if (*s == '\t') fputs("\\t", stdout);
        else if (uc < 0x20) printf("\\u%04x", uc);
        else putchar(*s);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture.json>\n", argv[0]); return 2; }
    char *err = NULL;
    json_t *root = json_parse_file(argv[1], &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); return 2; }
    if (!root) { fprintf(stderr, "parse error (null root)\n"); return 2; }

    json_t *cases = root;
    if (cases->type == JSON_OBJECT) {
        json_t *c = json_obj_get(cases, "cases");
        if (c && c->type == JSON_ARRAY) cases = c;
        else { fprintf(stderr, "no cases array\n"); json_free(root); return 2; }
    }
    if (cases->type != JSON_ARRAY) {
        fprintf(stderr, "root is not an array\n");
        json_free(root); return 2;
    }

    /* Emit the entire JSON array on a single line so the oracle's
     * line-by-line normalizer can parse it as one JSON document. */
    printf("[");
    size_t ncase = cases->c.count;
    for (size_t i = 0; i < ncase; i++) {
        json_t *c = cases->c.items[i];
        json_t *name_o = json_obj_get(c, "name");
        json_t *content_o = json_obj_get(c, "content");
        json_t *sd_o = json_obj_get(c, "skill_dir");
        json_t *ff_o = json_obj_get(c, "fixture_files");
        json_t *fs_o = json_obj_get(c, "fixture_scripts");

        const char *name = (name_o && name_o->type == JSON_STRING) ? name_o->str_val : "";
        const char *content = (content_o && content_o->type == JSON_STRING) ? content_o->str_val : "";
        const char *sd_s = (sd_o && sd_o->type == JSON_STRING) ? sd_o->str_val : NULL;

        int use_disk = (ff_o && ff_o->type == JSON_OBJECT) ||
                       (sd_s && sd_s[0]);
        char *dir = NULL;
        if (use_disk) {
            dir = make_temp_dir(name ? name : "case");
            if (ff_o) write_obj_files(dir, ff_o, NULL);
            if (fs_o) write_obj_files(dir, fs_o, "scripts");
        }

        lint_finding_t findings[64];
        size_t n = lint_content(content, dir, findings, 64);

        if (i) printf(",");
        printf("{\"case\":\"");
        json_escape(name);
        printf("\",\"n\":%zu,\"has_errors\":%s,\"skill_dir\":\"%s\",\"findings\":[",
               n, lint_has_errors(findings, n) ? "true" : "false", dir ? "tmpdir" : "");
        for (size_t j = 0; j < n; j++) {
            if (j) printf(",");
            printf("{\"severity\":\"%s\",\"rule\":\"", findings[j].severity);
            json_escape(findings[j].rule);
            printf("\",\"message\":\"");
            json_escape(findings[j].message);
            printf("\"}");
        }
        printf("]}");

        if (dir) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
            system(cmd);
            free(dir);
        }
    }
    printf("]");
    json_free(root);
    return 0;
}
