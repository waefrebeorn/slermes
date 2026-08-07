/*
 * t_port_agent_import.c — Oracle harness for hermes_cli.agent_import pure helpers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libjson/json.h"
#include "port_hermes_cli_agent_import.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static void print_str(const char *s) {
    putchar('"');
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
            else if (c == '\n') { fputs("\\n", stdout); }
            else if (c == '\t') { fputs("\\t", stdout); }
            else if (c == '\r') { fputs("\\r", stdout); }
            else if (c < 32) { printf("\\u%04x", c); }
            else { putchar(c); }
        }
    }
    putchar('"');
}

static void print_str_array(char **arr) {
    putchar('[');
    if (arr) {
        bool first = true;
        for (size_t i = 0; arr[i]; i++) {
            if (!first) putchar(',');
            first = false;
            print_str(arr[i]);
        }
    }
    putchar(']');
    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: t_port_agent_import <cases.in>\n");
        return 1;
    }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err) { free(err); fprintf(stderr, "parse error\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");
        json_t *arg = json_obj_get(c, "arg");

        if (strcmp(op, "normalize_text") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = ai_normalize_text(s);
            print_str(r); printf("\n");
            free(r);
        } else if (strcmp(op, "is_secret_key") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            printf("%s\n", ai_is_secret_key(s) ? "true" : "false");
        } else if (strcmp(op, "claude_rule_to_command_pattern") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = ai_claude_rule_to_command_pattern(s);
            if (r) { printf("\"%s\"\n", r); free(r); }
            else { printf("null\n"); }
        } else if (strcmp(op, "extract_markdown_entries") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char **entries = ai_extract_markdown_entries(s);
            print_str_array(entries);
            if (entries) {
                for (size_t j = 0; entries[j]; j++) free(entries[j]);
                free(entries);
            }
        } else if (strcmp(op, "sanitize_mcp_env") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *kept = NULL;
            char **stripped = NULL;
            ai_sanitize_mcp_env(s, &kept, &stripped);
            /* Build sorted stripped list */
            json_t *st_arr = json_array();
            if (stripped) {
                /* sort by simple insertion sort for determinism */
                size_t n = 0; while (stripped[n]) n++;
                for (size_t a = 0; a < n; a++)
                    for (size_t b = a+1; b < n; b++)
                        if (strcmp(stripped[a], stripped[b]) > 0) {
                            char *t = stripped[a]; stripped[a] = stripped[b]; stripped[b] = t;
                        }
                for (size_t j = 0; j < n; j++) json_append(st_arr, json_string(stripped[j]));
            }
            json_t *out = json_object();
            json_t *kept_obj = json_parse(kept ? kept : "{}", NULL);
            if (kept_obj && kept_obj->type == JSON_OBJECT)
                json_set(out, "kept", json_copy(kept_obj));
            else json_set(out, "kept", json_object());
            json_set(out, "stripped", st_arr);
            char *s2 = json_serialize(out);
            printf("%s\n", s2);
            free(s2);
            json_free(out);
            if (kept_obj) json_free(kept_obj);
            free(kept);
            if (stripped) {
                for (size_t j = 0; stripped[j]; j++) free(stripped[j]);
                free(stripped);
            }
        } else if (strcmp(op, "parse_existing_memory_entries") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char **entries = ai_parse_existing_memory_entries(s);
            print_str_array(entries);
            if (entries) {
                for (size_t j = 0; entries[j]; j++) free(entries[j]);
                free(entries);
            }
        } else if (strcmp(op, "merge_entries") == 0) {
            /* arg is an object: {"existing": [...], "incoming": [...], "limit": N} */
            json_t *existing = json_obj_get(arg, "existing");
            json_t *incoming = json_obj_get(arg, "incoming");
            json_t *lj = json_obj_get(arg, "limit");
            long limit = lj ? (long)lj->num_val : 1000;
            char *ex_json = existing ? json_serialize(existing) : strdup("[]");
            char *in_json = incoming ? json_serialize(incoming) : strdup("[]");
            size_t added = 0, dups = 0, ovf = 0;
            size_t existing_count = 0;
            json_t *ex_cnt = json_parse(ex_json, NULL);
            if (ex_cnt && ex_cnt->type == JSON_ARRAY) existing_count = ex_cnt->c.count;
            char **merged = ai_merge_entries(ex_json, in_json, limit, &added, &dups, &ovf);

            json_t *marr = json_array();
            if (merged) {
                for (size_t j = 0; merged[j]; j++) json_append(marr, json_string(merged[j]));
                for (size_t j = 0; merged[j]; j++) free(merged[j]);
                free(merged);
            }
            if (ex_cnt) json_free(ex_cnt);
            json_t *stats = json_object();
            json_set(stats, "existing", json_number((double)existing_count));
            json_set(stats, "added", json_number((double)added));
            json_set(stats, "duplicates", json_number((double)dups));
            json_set(stats, "overflowed", json_number((double)ovf));
            json_t *out = json_object();
            json_set(out, "merged", marr);
            json_set(out, "stats", stats);
            char *s = json_serialize(out);
            printf("%s\n", s);
            free(s);
            json_free(out);
            free(ex_json);
            free(in_json);
        } else if (strcmp(op, "load_yaml_file") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = ai_load_yaml_from_string(s);
            printf("%s\n", r);
            free(r);
        } else if (strcmp(op, "dump_yaml_file") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = ai_dump_yaml_to_string(s);
            print_str(r); printf("\n");
            free(r);
        } else if (strcmp(op, "default_source_dir") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = ai_default_source_dir(s, "@SBX@");
            if (r) { printf("\"%s\"\n", r); free(r); }
            else { printf("null\n"); }
        } else if (strcmp(op, "detect_agents") == 0) {
            char **r = ai_detect_agents("@SBX@");
            print_str_array(r);
            if (r) {
                for (size_t j = 0; r[j]; j++) free(r[j]);
                free(r);
            }
        } else if (strcmp(op, "backup_memory_file") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = ai_backup_path(s, 1700000000);
            if (r) { printf("\"%s\"\n", r); free(r); }
            else { printf("null\n"); }
        } else {
            printf("UNKNOWN_OP\n");
        }
    }

    json_free(cases);
    return 0;
}
