/*
 * cli_process_dock.c - C home for upstream Hermes hermes_cli/cli_process_dock.py.
 * PoP: w15_cliprocess_4e06fe__last_output_line @ hermes_cli/cli_process_dock.py:_last_output_line
 * PoP: w15_cliprocess_4e06fe_process_status @ hermes_cli/cli_process_dock.py:process_status
 * PoP: w15_cliprocess_4e06fe_process_glyph @ hermes_cli/cli_process_dock.py:process_glyph
 *
 * Pure per-row helpers are ported. Registry polling, i18n text and process
 * control bind the live gateway registry and stay in the live Python owner.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_cliprocess_4e06fe.h"

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s, n + 1);
    return r;
}
static char *normalize_ws(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *r = (char *)malloc(n + 2);
    if (!r) return dupstr("");
    size_t k = 0;
    const char *p = s ? s : "";
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (k) r[k++] = ' ';
        while (*p && !isspace((unsigned char)*p)) r[k++] = *p++;
    }
    r[k] = '\0';
    return r;
}
static int py_truth(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

struct json_t *w15_cliprocess_4e06fe_last_output_line(const char *buffer) {
    const char *s = buffer ? buffer : "";
    /* reversed(s.splitlines()): scan lines from the end, skipping blanks. */
    size_t n = strlen(s);
    size_t end = n;
    while (end > 0) {
        size_t start = end;
        while (start > 0 && s[start - 1] != '\n' && s[start - 1] != '\r') start--;
        size_t len = end - start;
        char *line = (char *)malloc(len + 1);
        if (line) { memcpy(line, s + start, len); line[len] = '\0'; }
        char *text = normalize_ws(line ? line : "");
        free(line);
        if (text[0]) { json_t *out = json_string(text); free(text); return out; }
        free(text);
        if (start == 0) break;
        end = start;
        /* step over the line break(s) immediately before `start` */
        while (end > 0 && (s[end - 1] == '\n' || s[end - 1] == '\r')) end--;
    }
    return json_string("");
}

struct json_t *w15_cliprocess_4e06fe_process_status(const struct json_t *entry) {
    json_t *st = entry ? json_obj_get(entry, "status") : NULL;
    if (!st || st->type != JSON_STRING || strcmp(st->str_val, "exited"))
        return json_string("running");
    json_t *cr = json_obj_get(entry, "completion_reason");
    const char *reason = (cr && cr->type == JSON_STRING && cr->str_val[0]) ? cr->str_val : "";
    if (!strcmp(reason, "killed") || !strcmp(reason, "lost")) return json_string(reason);
    return json_string(py_truth(json_obj_get(entry, "exit_code")) ? "failed" : "done");
}

struct json_t *w15_cliprocess_4e06fe_process_glyph(const struct json_t *row) {
    json_t *st = row ? json_obj_get(row, "status") : NULL;
    const char *s = (st && st->type == JSON_STRING) ? st->str_val : "";
    if (!strcmp(s, "running")) return json_string("\xe2\x9a\x99"); /* U+2699 */
    if (!strcmp(s, "done"))    return json_string("\xe2\x9c\x94"); /* U+2714 */
    if (!strcmp(s, "failed") || !strcmp(s, "killed"))
        return json_string("\xe2\x9c\x98");                        /* U+2718 */
    return json_string("?");
}
