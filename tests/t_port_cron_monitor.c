/* t_port_cron_monitor.c — oracle harness for cron/monitor.py pure functions.
 *
 * Reads a fixture (argv[1]) of newline-delimited JSON records, one per case:
 *   {"fn":"hash","in":"<text>"}
 *   {"fn":"diff","old":"<prev>","new":"<curr>"}
 *   {"fn":"has","script":"<s>","url":"<u>"}
 * Emits one JSON line per case: {"in":<canonical-input>,"out":<c-out>}.
 * The runner pipes C stdout into the Python oracle, which recomputes `out`
 * from `in` against the live Python cron.monitor.* and prints expected out.
 * The runner normalizes + diffs both sides (see run_oracle.sh).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "cron_monitor.h"

/* minimal json parser for the fixture + json writer for records */

/* Escape a string into a freshly malloc'd JSON string literal. */
static char *js(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    /* worst case: every byte escaped */
    char *out = malloc(n * 6 + 3);
    if (!out) return strdup("\"\"");
    char *q = out;
    *q++ = '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = (char)c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\r') { *q++ = '\\'; *q++ = 'r'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else if (c < 0x20) { q += snprintf(q, 7, "\\u%04x", c); }
        else { *q++ = (char)c; }
    }
    *q++ = '"'; *q = 0;
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_cron_monitor <fixture>\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char line[65536];
    size_t ln = 0;
    while (fgets(line, sizeof(line), f)) {
        /* strip trailing newline; tolerate embedded newlines via the fixture's
         * structure (we treat each \n-terminated top-level line as one case). */
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
        if (L == 0 || (line[0] != '{' && line[0] != '[')) continue;

        json_t *rec = json_parse(line, NULL);
        if (!rec || !json_is_object(rec)) { json_free(rec); continue; }

        const char *fn = json_object_get_string(rec, "fn", NULL);
        char *out = NULL;
        char *in_key = NULL, *in_val = NULL;

        if (fn && strcmp(fn, "hash") == 0) {
            const char *s = json_object_get_string(rec, "in", NULL);
            char *h = cron_monitor_hash_output(s);
            out = h;
            asprintf(&in_key,  "in");  asprintf(&in_val, "%s", s ? s : "");
        } else if (fn && strcmp(fn, "diff") == 0) {
            const char *old = json_object_get_string(rec, "old", NULL);
            const char *newn = json_object_get_string(rec, "new", NULL);
            out = cron_monitor_build_diff(old, newn);
            asprintf(&in_key, "old,new");
            char *jold = js(old), *jnew = js(newn);
            asprintf(&in_val, "%s|%s", jold, jnew);
            free(jold); free(jnew);
        } else if (fn && strcmp(fn, "has") == 0) {
            const char *sc = json_object_get_string(rec, "script", NULL);
            const char *u  = json_object_get_string(rec, "url", NULL);
            bool b = cron_monitor_has(sc, u);
            asprintf(&out, b ? "true" : "false");
            char *jsc = js(sc), *ju = js(u);
            asprintf(&in_key, "script,url"); asprintf(&in_val, "%s|%s", jsc, ju);
            free(jsc); free(ju);
        } else {
            asprintf(&in_key, "fn"); asprintf(&in_val, "%s", fn ? fn : "");
            out = strdup("");
        }
        char *jin = js(in_val);
        /* canonical: {"in":<in>,"out":<out>} */
        printf("{\"in\":%s,\"out\":%s}\n", jin, js(out));
        free(in_key); free(in_val); free(jin); free(out);
        json_free(rec);
        ln++;
    }
    fclose(f);
    fprintf(stderr, "cron_monitor harness: %zu cases\n", ln);
    return 0;
}
