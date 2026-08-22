/*
 * t_port_skillevaluator_scan.c — parity harness for
 * src/tools/skillevaluator_scan.c (tools/skillevaluator_scan.py).
 *
 * Fixture: JSON {"ops":[...]} — one JSON output line per op:
 *   {"kind":"is_secrets_class","check":"jwt_tokens"}
 *   {"kind":"location","file":"a.md","line":3}
 *   {"kind":"parse_format","report":{...},"limit":10}
 *   {"kind":"parse_meta","report":{...}}
 *   {"kind":"run_scan","avail":true,"launch_fail":false,"timeout_expire":false,
 *    "files":{"skillevaluator-output-1.json":<obj|str>},"timeout":5}
 */
#include "skillevaluator_scan.h"
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

/* --- injectable hooks driven by the current run_scan op --- */
static json_t *g_op = NULL;

static bool op_flag(const char *key) {
    json_t *v = json_object_get(g_op, key);
    if (!v) return false;
    if (v->type == JSON_BOOL) return v->bool_val != 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return false;
}

static bool hook_avail(void) { return op_flag("avail"); }

static int hook_run(const char *scanner_bin, const char *skill_dir,
                    const char *checks, const char *outdir, int timeout,
                    bool *timed_out, char **launch_err) {
    (void)scanner_bin; (void)skill_dir; (void)checks; (void)timeout;
    if (op_flag("timeout_expire")) { *timed_out = true; return 0; }
    if (op_flag("launch_fail")) {
        const char *msg = json_get_str(g_op, "launch_err", "launch failed");
        *launch_err = strdup(msg);
        return 1;
    }
    json_t *files = json_object_get(g_op, "files");
    if (files && files->type == JSON_OBJECT) {
        size_t n = json_object_size(files);
        for (size_t i = 0; i < n; i++) {
            const char *name = json_object_get_key_at(files, i);
            json_t *val = (json_t *)json_object_get_at(files, i);
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", outdir, name);
            FILE *fp = fopen(path, "wb");
            if (!fp) continue;
            if (val && val->type == JSON_STRING) {
                fputs(val->str_val, fp);
            } else if (val) {
                char *s = json_serialize(val);
                if (s) { fputs(s, fp); free(s); }
            }
            fclose(fp);
        }
    }
    return 0;
}

static void emit(json_t *o) {
    char *line = json_serialize(o);
    printf("%s\n", line);
    free(line);
    json_free(o);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "is_secrets_class") == 0) {
                bool v = skillevaluator_scan_is_secrets_class(json_get_str(c, "check", ""));
                json_object_set(o, "out", json_bool(v ? 1 : 0));
            } else if (strcmp(kind, "location") == 0) {
                skillevaluator_scan_finding_t f;
                memset(&f, 0, sizeof(f));
                f.file = (char *)json_get_str(c, "file", "");
                f.line = (long)json_get_num(c, "line", 0.0);
                char *s = skillevaluator_scan_finding_location(&f);
                json_object_set(o, "out", json_string(s ? s : ""));
                free(s);
            } else if (strcmp(kind, "parse_format") == 0) {
                json_t *rep = json_object_get(c, "report");
                skillevaluator_scan_report_t *r = skillevaluator_scan_parse_report(rep);
                int limit = (int)json_get_num(c, "limit", 10.0);
                char *txt = skillevaluator_scan_format_tier1_report(r, limit);
                json_object_set(o, "out", json_string(txt ? txt : ""));
                free(txt);
                skillevaluator_scan_report_free(r);
            } else if (strcmp(kind, "parse_meta") == 0) {
                json_t *rep = json_object_get(c, "report");
                skillevaluator_scan_report_t *r = skillevaluator_scan_parse_report(rep);
                json_t *meta = json_new_object();
                json_object_set(meta, "available", json_bool(r->available ? 1 : 0));
                json_object_set(meta, "passed", json_bool(r->passed ? 1 : 0));
                json_object_set(meta, "n_findings", json_int((long)r->n_findings));
                json_t *inc = json_new_array();
                for (size_t k = 0; k < r->n_incomplete_checks; k++)
                    json_array_append(inc, json_string(r->incomplete_checks[k]));
                json_object_set(meta, "incomplete", inc);
                json_t *fs = json_new_array();
                for (size_t k = 0; k < r->n_findings; k++) {
                    skillevaluator_scan_finding_t *f = &r->findings[k];
                    json_t *fo = json_new_object();
                    json_object_set(fo, "check", json_string(f->check ? f->check : ""));
                    json_object_set(fo, "validator", json_string(f->validator ? f->validator : ""));
                    json_object_set(fo, "severity", json_string(f->severity ? f->severity : ""));
                    json_object_set(fo, "message", json_string(f->message ? f->message : ""));
                    json_object_set(fo, "file", json_string(f->file ? f->file : ""));
                    json_object_set(fo, "line", json_int(f->line));
                    json_object_set(fo, "suggestion", json_string(f->suggestion ? f->suggestion : ""));
                    json_array_append(fs, fo);
                }
                json_object_set(meta, "findings", fs);
                json_object_set(o, "out", meta);
                skillevaluator_scan_report_free(r);
            } else if (strcmp(kind, "run_scan") == 0) {
                g_op = c;
                skillevaluator_scan_avail_hook = hook_avail;
                skillevaluator_scan_run_hook = hook_run;
                skillevaluator_scan_report_t *r =
                    skillevaluator_run_tier1_scan(json_get_str(c, "skill_dir", "."),
                                                  (int)json_get_num(c, "timeout", 5.0));
                json_t *meta = json_new_object();
                json_object_set(meta, "available", json_bool(r->available ? 1 : 0));
                json_object_set(meta, "passed", json_bool(r->passed ? 1 : 0));
                json_object_set(meta, "error", json_string(r->error ? r->error : ""));
                json_object_set(meta, "n_findings", json_int((long)r->n_findings));
                json_t *inc = json_new_array();
                for (size_t k = 0; k < r->n_incomplete_checks; k++)
                    json_array_append(inc, json_string(r->incomplete_checks[k]));
                json_object_set(meta, "incomplete", inc);
                json_object_set(o, "out", meta);
                skillevaluator_scan_report_free(r);
                skillevaluator_scan_avail_hook = NULL;
                skillevaluator_scan_run_hook = NULL;
                g_op = NULL;
            } else {
                json_object_set(o, "out", json_new_null());
            }
            emit(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
