/* Oracle harness for hermes_cli/sqlite_util.py port.
 * Reads fixture (argv[1]): {"db":"/tmp/x.db","ops":[...]}
 *   {"op":"add","table":..,"column":..,"ddl":..} -> result code (1 added,0 exists,-1 err)
 *   {"op":"txn_begin"} -> rc
 *   {"op":"txn_end","committed":bool} -> rc
 * Prints a compact JSON array of per-op results. Byte-diffed vs Python oracle.
 */
#include "hermes_cli/sqlite_util.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

static sqlite3 *open_db(const char *path) {
    sqlite3 *db = NULL;
    /* ensure parent dir exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0700); *slash = '/'; }
    sqlite3_open(path, &db);
    return db;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *src = read_file(argv[1]);
    if (!src) return 1;
    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    const char *dbpath = json_string_value(json_object_get(root, "db"));
    /* Mirror the Python oracle: start from a clean fixture DB so the run is
     * deterministic across invocations (otherwise columns accumulate and
     * "add" ops read as EXISTS on the second+ run). */
    remove(dbpath);
    sqlite3 *db = open_db(dbpath);
    if (!db) { fprintf(stderr, "cannot open %s\n", dbpath); return 1; }
    /* create a seed table for column-add tests */
    char *e = NULL;
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS t(id INTEGER)", NULL, NULL, &e);
    if (e) sqlite3_free(e);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS other(x INTEGER)", NULL, NULL, &e);
    if (e) sqlite3_free(e);

    json_t *results = json_new_array();
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < ops->c.count; i++) {
            json_t *o = ops->c.items[i];
            const char *op = json_string_value(json_object_get(o, "op"));
            if (strcmp(op, "add") == 0) {
                const char *table = json_string_value(json_object_get(o, "table"));
                const char *column = json_string_value(json_object_get(o, "column"));
                const char *ddl = json_string_value(json_object_get(o, "ddl"));
                sqlite_util_add_result_t r =
                    sqlite_util_add_column_if_missing(db, table, column, ddl);
                json_array_append(results, json_new_number((double)r));
            } else if (strcmp(op, "txn_begin") == 0) {
                json_array_append(results, json_new_number((double)sqlite_util_write_txn_begin(db)));
            } else if (strcmp(op, "txn_end") == 0) {
                int committed = json_is_true(json_object_get(o, "committed"));
                json_array_append(results, json_new_number((double)sqlite_util_write_txn_end(db, committed)));
            }
        }
    }

    char *out = json_serialize(results);
    printf("%s", out ? out : "[]");
    free(out);
    json_free(results);
    sqlite3_close(db);
    json_free(root);
    free(src);
    return 0;
}
