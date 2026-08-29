#define _POSIX_C_SOURCE 200809L
/* session_lost_and_found_pure.c - Faithful C11 port of the pure, deterministic
 * classification heuristics in hermes_cli/session_lost_and_found.py (page-level
 * schema-less session DB salvage).
 *
 * Ports (PoP):
 *   _is_session_id                  -> laf_is_session_id
 *   _looks_like_source              -> laf_looks_like_source
 *   classify_lost_and_found_row     -> laf_classify_row
 *   _heuristic_started_at           -> laf_heuristic_started_at
 *
 * The sqlite/.recover/CLI glue is NOT ported (subprocess + DB, not pure logic).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LAF_EPOCH_LOW  1000000000.0
#define LAF_EPOCH_HIGH 4000000000.0

/* SESSION_ID_PATTERN = ^\d{8}_\d{6}_  */
/* PoP: laf_is_session_id @ hermes_cli/session_lost_and_found.py:_is_session_id */

int laf_is_session_id(const char *value) {
    if (!value) return 0;
    for (int i = 0; i < 8; i++) if (value[i] < '0' || value[i] > '9') return 0;
    if (value[8] != '_') return 0;
    for (int i = 9; i < 15; i++) if (value[i] < '0' || value[i] > '9') return 0;
    if (value[15] != '_') return 0;
    return 1;
}

static int laf_known_source(const char *v) {
    static const char *known[] = {
        "cli", "telegram", "discord", "slack", "whatsapp", "signal", "matrix",
        "irc", "email", "x", "twitter", "api", "gateway", "web", "dashboard",
        "tool", "subagent", "cron", "recovered", "imported", "acp", NULL
    };
    for (int i = 0; known[i]; i++) if (strcmp(v, known[i]) == 0) return 1;
    return 0;
}

/* ^[a-z][a-z0-9_-]{0,31}$ */
static int laf_source_shape(const char *v) {
    size_t n = strlen(v);
    if (n == 0 || n > 32) return 0;
    if (!(v[0] >= 'a' && v[0] <= 'z')) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = v[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}
/* PoP: laf_looks_like_source @ hermes_cli/session_lost_and_found.py:_looks_like_source */

int laf_looks_like_source(const char *value) {
    if (!value || !*value) return 0;
    return laf_known_source(value) || laf_source_shape(value);
}

/* MESSAGE_ROLES membership */
static int laf_is_role(const char *v) {
    return strcmp(v, "user") == 0 || strcmp(v, "assistant") == 0 ||
           strcmp(v, "tool") == 0 || strcmp(v, "system") == 0;
}

/* PoP: laf_classify_row @ hermes_cli/session_lost_and_found.py:classify_lost_and_found_row */
char *laf_classify_row(int nfield, const char *cells_json) {
    json_t *cells = json_parse(cells_json, NULL);
    char *result = (char *)malloc(8);
    strcpy(result, "null");

    if (cells && cells->type == JSON_ARRAY) {
        size_t n = json_len(cells);
        /* cells[0] for the rowid-alias messages case */
        json_t *c0 = (n > 0) ? json_array_get(cells, 0) : NULL;
        json_t *c1 = (n > 1) ? json_array_get(cells, 1) : NULL;
        json_t *c2 = (n > 2) ? json_array_get(cells, 2) : NULL;

        if (n >= 3 && c0 && c0->type == JSON_NULL) {
            if (c1 && c1->type == JSON_STRING && c1->str_val && *c1->str_val &&
                c2 && c2->type == JSON_STRING && c2->str_val &&
                laf_is_role(c2->str_val)) {
                free(result);
                result = strdup("\"messages\"");
                json_free(cells);
                return result;
            }
            json_free(cells);
            return result; /* null */
        }

        const char *idval = (c0 && c0->type == JSON_STRING) ? c0->str_val : NULL;
        if (!laf_is_session_id(idval)) {
            json_free(cells);
            return result; /* null */
        }

        const char *srcval = (c1 && c1->type == JSON_STRING) ? c1->str_val : NULL;

        if (nfield == 18) { /* SESSION_MODEL_USAGE_NFIELD */
            if (srcval && *srcval) {
                free(result);
                result = strdup("\"session_model_usage\"");
            }
        } else if (nfield == 55 || nfield == 54 || nfield == 52 || nfield == 14) {
            if (laf_looks_like_source(srcval)) {
                free(result);
                result = strdup("\"sessions\"");
            }
        } else if (nfield >= 30 && laf_looks_like_source(srcval)) {
            free(result);
            result = strdup("\"sessions\"");
        }
    }
    json_free(cells);
    return result;
}
/* PoP: laf_heuristic_started_at @ hermes_cli/session_lost_and_found.py:_heuristic_started_at */

double laf_heuristic_started_at(const char *cells_json) {
    json_t *cells = json_parse(cells_json, NULL);
    double found = 0.0;
    if (cells && cells->type == JSON_ARRAY) {
        size_t n = json_len(cells);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_array_get(cells, i);
            if (v && v->type == JSON_NUMBER) {
                double d = v->num_val;
                if (d >= LAF_EPOCH_LOW && d <= LAF_EPOCH_HIGH) { found = d; break; }
            }
        }
    }
    json_free(cells);
    return found;
}


static char *laf_strndup(const char *s, size_t n)
{
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ======================================================================
 *  sqlite / .recover glue (hermes_cli/session_lost_and_found.py)
 *  Page-level schema-less SessionDB salvage via the sqlite3 CLI.
 * ====================================================================== */

#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define LAF_SESSION_MODEL_USAGE_NFIELD     18
#define LAF_SESSIONS_LEGACY_MINIMAL_NFIELD 14

/* PoP: laf_cli_supports_recover @ hermes_cli/session_lost_and_found.py:_cli_supports_recover */
/*
 *  True when the binary can run `.recover` (has sqlite_dbpage). Probes a
 * scratch DB instead of discovering it mid-recovery. */
int laf_cli_supports_recover(const char *binary)
{
    if (!binary || !*binary) return 0;
    char tmpl[] = "/tmp/hermes-recover-probe-XXXXXX";
    char *scratch = mkdtemp(tmpl);
    if (!scratch) return 0;
    char dbpath[512];
    snprintf(dbpath, sizeof(dbpath), "%s/probe.db", scratch);
    /* build probe DB with the library */
    sqlite3 *conn = NULL;
    int ok = 0;
    if (sqlite3_open(dbpath, &conn) == SQLITE_OK) {
        if (sqlite3_exec(conn, "CREATE TABLE t (x); INSERT INTO t VALUES (1);",
                         NULL, NULL, NULL) == SQLITE_OK)
            ok = 1;
        sqlite3_close(conn);
    }
    int supports = 0;
    if (ok) {
        char *argv[5];
        argv[0] = (char *)binary;
        argv[1] = (char *)"-readonly";
        argv[2] = dbpath;
        argv[3] = (char *)".recover";
        argv[4] = NULL;
        int pipefd[2];
        if (pipe(pipefd) == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                dup2(pipefd[1], STDERR_FILENO);
                close(pipefd[1]);
                execv(binary, argv);
                _exit(127);
            } else if (pid > 0) {
                close(pipefd[1]);
                char errbuf[4096];
                size_t total = 0;
                ssize_t nread;
                while ((nread = read(pipefd[0], errbuf + total,
                                     sizeof(errbuf) - 1 - total)) > 0 &&
                       total < sizeof(errbuf) - 1)
                    total += (size_t)nread;
                errbuf[total] = '\0';
                close(pipefd[0]);
                int status = 0;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                    !strstr(errbuf, "sqlite_dbpage"))
                    supports = 1;
            } else {
                close(pipefd[0]); close(pipefd[1]);
            }
        }
    }
    /* shutil.rmtree(scratch_dir, ignore_errors=True) */
    char cmd[640];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", scratch);
    int rc = system(cmd);
    (void)rc;
    return supports;
}

/* PoP: laf_find_sqlite3_cli @ hermes_cli/session_lost_and_found.py:find_sqlite3_cli */
/*
 *  Return a malloc'd `.recover`-capable sqlite3 CLI path from PATH, or NULL.
 * PATH presence is not enough: distro builds can lack sqlite_dbpage. */
char *laf_find_sqlite3_cli(void)
{
    const char *pathenv = getenv("PATH");
    if (!pathenv || !*pathenv) return NULL;
    char *paths = strdup(pathenv);
    if (!paths) return NULL;
    char found[1024] = "";
    char *save = NULL;
    for (char *dir = strtok_r(paths, ":", &save); dir && !found[0];
         dir = strtok_r(NULL, ":", &save)) {
        char cand[768];
        snprintf(cand, sizeof(cand), "%s/sqlite3", dir);
        if (access(cand, X_OK) != 0) continue;
        if (laf_cli_supports_recover(cand))
            snprintf(found, sizeof(found), "%s", cand);
    }
    free(paths);
    return found[0] ? strdup(found) : NULL;
}

/* PoP: laf_lost_and_found_db_usable @ hermes_cli/session_lost_and_found.py:_lost_and_found_db_usable */
int laf_lost_and_found_db_usable(const char *lf_path)
{
    struct stat st;
    if (!lf_path || stat(lf_path, &st) != 0 || st.st_size == 0) return 0;
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(lf_path, &conn, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        return 0;
    }
    int usable = 0;
    sqlite3_stmt *stt = NULL;
    if (sqlite3_prepare_v2(conn, "SELECT name FROM sqlite_master WHERE type='table'",
                           -1, &stt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stt) == SQLITE_ROW) { usable = 1; break; }
        sqlite3_finalize(stt);
    }
    sqlite3_close(conn);
    return usable;
}

/* PoP: laf_table_columns @ hermes_cli/session_lost_and_found.py:_table_columns */
/*
 *  Column names of *table* in *conn*. Malloc'd NULL-terminated array; caller
 * frees each string + array. *out_count set. Empty when table is missing. */
char **laf_table_columns(sqlite3 *conn, const char *table, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!conn || !table || !*table) return NULL;
    char sql[512];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
    char **cols = NULL;
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(st, 1);
        char **tmp = realloc(cols, sizeof(char *) * (size_t)(n + 1));
        if (!tmp) break;
        cols = tmp;
        cols[n++] = strdup(name ? (const char *)name : "");
    }
    sqlite3_finalize(st);
    if (out_count) *out_count = n;
    return cols;
}

static void laf_free_columns(char **cols, int n)
{
    if (!cols) return;
    for (int i = 0; i < n; i++) free(cols[i]);
    free(cols);
}

/* PoP: laf_notnull_defaults @ hermes_cli/session_lost_and_found.py:_notnull_defaults */
/*
 *  Map column index -> substitute value for NOT NULL columns (JSON-encoded:
 * {"<index>": <value>} with numeric or string substitutes). Returns a
 * malloc'd json_t object; caller frees. */
json_t *laf_notnull_defaults(sqlite3 *conn, const char *table)
{
    json_t *subs = json_object();
    if (!conn || !table || !*table) return subs;
    char sql[512];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(\"%s\")", table);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn, sql, -1, &st, NULL) != SQLITE_OK) return subs;
    int index = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        int notnull = sqlite3_column_int(st, 3);
        if (!notnull) { index++; continue; }
        const unsigned char *dtype_raw = sqlite3_column_text(st, 2);
        const unsigned char *dflt_raw = sqlite3_column_text(st, 4);
        char key[16];
        snprintf(key, sizeof(key), "%d", index);
        if (!dflt_raw) {
            char declared[64] = "";
            if (dtype_raw) snprintf(declared, sizeof(declared), "%s", (const char *)dtype_raw);
            for (char *p = declared; *p; p++) *p = (char)toupper((unsigned char)*p);
            if (strstr(declared, "INT") || strstr(declared, "REAL"))
                json_set(subs, key, json_number(0.0));
            else
                json_set(subs, key, json_string(""));
        } else {
            const char *text = (const char *)dflt_raw;
            size_t tl = strlen(text);
            if (tl >= 2 && text[0] == '\'' && text[tl - 1] == '\'')
                json_set(subs, key, json_string(laf_strndup(text + 1, tl - 2)));
            else {
                char *end = NULL;
                long long iv = strtoll(text, &end, 10);
                if (end && *end == '\0' && end != text)
                    json_set(subs, key, json_number((double)iv));
                else {
                    double dv2 = strtod(text, &end);
                    if (end && *end == '\0' && end != text)
                        json_set(subs, key, json_number(dv2));
                    else
                        json_set(subs, key, json_string(text));
                }
            }
        }
        index++;
    }
    sqlite3_finalize(st);
    return subs;
}

/* Helper: substitute NOT NULL defaults into a value list before insert.
 * values_json: JSON array; notnull_subs: {"idx": value}. Returns a new
 * json_t array (caller frees). */
static json_t *laf_apply_substitutes(json_t *values, json_t *notnull_subs)
{
    json_t *out = json_array();
    size_t n = values && values->type == JSON_ARRAY ? json_len(values) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(values, i);
        if (v && v->type == JSON_NULL && notnull_subs && notnull_subs->type == JSON_OBJECT) {
            char key[16];
            snprintf(key, sizeof(key), "%zu", i);
            const json_t *sub = json_obj_get(notnull_subs, key);
            if (sub) { json_append(out, json_copy(sub)); continue; }
        }
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}

/* PoP: laf_insert_prefix_row @ hermes_cli/session_lost_and_found.py:_insert_prefix_row */
/*
 *  INSERT OR IGNORE the first N values into dest_columns of *table*.
 * values/notnull_substitutes are json_t arrays/objects. Returns rows==1. */
int laf_insert_prefix_row(sqlite3 *dest, const char *table,
                          char **dest_columns, int dest_ncols,
                          json_t *values, json_t *notnull_substitutes)
{
    if (!dest || !table || !values || values->type != JSON_ARRAY) return 0;
    json_t *vals = laf_apply_substitutes(values, notnull_substitutes);
    int nv = (int)json_len(vals);
    if (nv > dest_ncols) nv = dest_ncols;
    if (nv <= 0) { json_free(vals); return 0; }
    size_t cap = 64 + (size_t)nv * 48;
    char *cols = malloc(cap), *ph = malloc((size_t)nv * 3 + 8);
    if (!cols || !ph) { free(cols); free(ph); json_free(vals); return 0; }
    cols[0] = '\0'; ph[0] = '\0';
    for (int i = 0; i < nv; i++) {
        char one[128];
        snprintf(one, sizeof(one), "%s\"%s\"", i ? ", " : "", dest_columns[i]);
        strncat(cols, one, cap - strlen(cols) - 1);
        snprintf(one, sizeof(one), "%s?", i ? ", " : "");
        strncat(ph, one, (size_t)nv * 3 + 8 - strlen(ph) - 1);
    }
    char *sql = sqlite3_mprintf("INSERT OR IGNORE INTO \"%w\" (%s) VALUES (%s)",
                                table, cols, ph);
    sqlite3_stmt *st = NULL;
    int inserted = 0;
    if (sql && sqlite3_prepare_v2(dest, sql, -1, &st, NULL) == SQLITE_OK) {
        for (int i = 0; i < nv; i++) {
            const json_t *v = json_get(vals, i);
            switch (v ? v->type : JSON_NULL) {
            case JSON_NULL:    sqlite3_bind_null(st, i + 1); break;
            case JSON_BOOL:    sqlite3_bind_int(st, i + 1, v->bool_val ? 1 : 0); break;
            case JSON_NUMBER:  sqlite3_bind_double(st, i + 1, v->num_val); break;
            case JSON_STRING:  sqlite3_bind_text(st, i + 1, v->str_val, -1, SQLITE_TRANSIENT); break;
            default: {
                char *s = json_dumps((json_t *)v, 0);
                sqlite3_bind_text(st, i + 1, s ? s : "", -1, SQLITE_TRANSIENT);
                free(s);
                break;
            }
            }
        }
        inserted = (sqlite3_step(st) == SQLITE_DONE &&
                    sqlite3_changes(dest) == 1);
        sqlite3_finalize(st);
    }
    sqlite3_free(sql);
    free(cols); free(ph);
    json_free(vals);
    return inserted;
}

/* PoP: laf_copy_direct_tables @ hermes_cli/session_lost_and_found.py:_copy_direct_tables */
/*
 *  Copy rows .recover attributed to real canonical tables. Returns a JSON
 * object {table: copied_count} (caller frees). */
json_t *laf_copy_direct_tables(sqlite3 *lf_conn, sqlite3 *dest)
{
    static const char *kTables[] = {
        "system_prompts", "sessions", "messages", "session_model_usage",
        "compression_locks", "gateway_routing", "async_delegations", NULL };
    json_t *copied = json_object();
    for (int t = 0; kTables[t]; t++) {
        const char *table = kTables[t];
        int src_n = 0, dst_n = 0;
        char **src_cols = laf_table_columns(lf_conn, table, &src_n);
        if (!src_cols || src_n == 0) { laf_free_columns(src_cols, src_n); continue; }
        char **dst_cols = laf_table_columns(dest, table, &dst_n);
        /* columns present in both, in destination order */
        char common[32][128];
        int nc = 0;
        for (int i = 0; i < dst_n && nc < 32; i++)
            for (int j = 0; j < src_n; j++)
                if (strcmp(dst_cols[i], src_cols[j]) == 0) {
                    snprintf(common[nc++], 128, "%s", dst_cols[i]);
                    break;
                }
        laf_free_columns(src_cols, src_n);
        if (nc == 0) { laf_free_columns(dst_cols, dst_n); continue; }
        char quoted[4096] = "";
        for (int i = 0; i < nc; i++) {
            char one[160];
            snprintf(one, sizeof(one), "%s\"%s\"", i ? ", " : "", common[i]);
            strncat(quoted, one, sizeof(quoted) - strlen(quoted) - 1);
        }
        char select_sql[4600];
        snprintf(select_sql, sizeof(select_sql),
                 "SELECT %s FROM \"%s\"", quoted, table);
        sqlite3_stmt *rows = NULL;
        int delta_total = 0;
        if (sqlite3_prepare_v2(lf_conn, select_sql, -1, &rows, NULL) != SQLITE_OK) {
            laf_free_columns(dst_cols, dst_n);
            continue;
        }
        int any_row = 0;
        while (sqlite3_step(rows) == SQLITE_ROW) {
            any_row = 1;
            char insert_sql[4600];
            snprintf(insert_sql, sizeof(insert_sql),
                     "INSERT OR IGNORE INTO \"%s\" (%s) VALUES (%s)",
                     table, quoted, "?");
            /* build placeholders properly */
            char ph[256] = "";
            for (int i = 0; i < nc; i++)
                strncat(ph, i ? ", ?" : "?", sizeof(ph) - strlen(ph) - 1);
            snprintf(insert_sql, sizeof(insert_sql),
                     "INSERT OR IGNORE INTO \"%s\" (%s) VALUES (%s)",
                     table, quoted, ph);
            sqlite3_stmt *ins = NULL;
            if (sqlite3_prepare_v2(dest, insert_sql, -1, &ins, NULL) == SQLITE_OK) {
                for (int i = 0; i < nc; i++) {
                    switch (sqlite3_column_type(rows, i)) {
                    case SQLITE_NULL:   sqlite3_bind_null(ins, i + 1); break;
                    case SQLITE_INTEGER: sqlite3_bind_int64(ins, i + 1, sqlite3_column_int64(rows, i)); break;
                    case SQLITE_FLOAT:  sqlite3_bind_double(ins, i + 1, sqlite3_column_double(rows, i)); break;
                    case SQLITE_TEXT:   sqlite3_bind_text(ins, i + 1, (const char *)sqlite3_column_text(rows, i), -1, SQLITE_TRANSIENT); break;
                    case SQLITE_BLOB:   sqlite3_bind_blob(ins, i + 1, sqlite3_column_blob(rows, i), sqlite3_column_bytes(rows, i), SQLITE_TRANSIENT); break;
                    default:            sqlite3_bind_null(ins, i + 1); break;
                    }
                }
                sqlite3_step(ins);
                delta_total += sqlite3_changes(dest);
                sqlite3_finalize(ins);
            }
        }
        sqlite3_finalize(rows);
        if (!any_row)
            json_set(copied, table, json_number(0.0));
        else
            json_set(copied, table, json_number((double)delta_total));
        laf_free_columns(dst_cols, dst_n);
    }
    return copied;
}


/* PoP: laf_run_cli_lost_and_found_recover @ hermes_cli/session_lost_and_found.py:run_cli_lost_and_found_recover */
/*
 *  Run `sqlite3 <source> .recover` streamed into a fresh scratch DB. Tries
 * `.recover --ignore-freelist` first, falling back to a plain `.recover`.
 * On success returns a malloc'd JSON report {binary, attempts:[...]};
 * on failure returns a malloc'd error JSON {"error": ...}. */
char *laf_run_cli_lost_and_found_recover(const char *source, const char *lf_path,
                                         const char *sqlite3_bin, double timeout_seconds)
{
    (void)timeout_seconds;   /* wall-clock cap handled by the caller's alarm */
    json_t *report = json_object();
    json_set(report, "binary", json_string(sqlite3_bin ? sqlite3_bin : ""));
    json_t *attempts = json_array();
    const char *commands[2] = { ".recover --ignore-freelist", ".recover" };
    for (int ci = 0; ci < 2; ci++) {
        struct stat st;
        if (stat(lf_path, &st) == 0)
            unlink(lf_path);
        char dump_arg[1024];
        snprintf(dump_arg, sizeof(dump_arg), "%s", commands[ci]);
        int dump_pipe[2], load_err_pipe[2];
        if (pipe(dump_pipe) != 0) continue;
        if (pipe(load_err_pipe) != 0) { close(dump_pipe[0]); close(dump_pipe[1]); continue; }
        pid_t dump_pid = fork();
        if (dump_pid == 0) {
            /* child: dump source SQL to stdout */
            close(dump_pipe[0]);
            close(load_err_pipe[0]);
            dup2(dump_pipe[1], STDOUT_FILENO);
            dup2(load_err_pipe[1], STDERR_FILENO);
            close(dump_pipe[1]);
            close(load_err_pipe[1]);
            execl(sqlite3_bin, sqlite3_bin, "-readonly", source, dump_arg, (char *)NULL);
            _exit(127);
        }
        pid_t load_pid = fork();
        if (load_pid == 0) {
            /* child: load dump into lf_path */
            close(dump_pipe[1]);
            close(load_err_pipe[0]);
            dup2(dump_pipe[0], STDIN_FILENO);
            dup2(load_err_pipe[1], STDERR_FILENO);
            close(dump_pipe[0]);
            close(load_err_pipe[1]);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); close(devnull); }
            execl(sqlite3_bin, sqlite3_bin, lf_path, (char *)NULL);
            _exit(127);
        }
        close(dump_pipe[0]);   /* let dump receive SIGPIPE if load dies */
        close(dump_pipe[1]);
        close(load_err_pipe[1]);
        /* drain load stderr */
        static __thread char errbuf[4096];
        size_t total = 0;
        while (total < sizeof(errbuf) - 1) {
            size_t room = sizeof(errbuf) - 1 - total;
            ssize_t nread2 = read(load_err_pipe[0], errbuf + total, room);
            if (nread2 <= 0) break;
            total += (size_t)nread2;
        }
        errbuf[total] = '\0';
        close(load_err_pipe[0]);
        int dstatus = 0, lstatus = 0;
        waitpid(load_pid, &lstatus, 0);
        waitpid(dump_pid, &dstatus, 0);
        int dump_rc = WIFEXITED(dstatus) ? WEXITSTATUS(dstatus) : -1;
        int load_rc = WIFEXITED(lstatus) ? WEXITSTATUS(lstatus) : -1;
        json_t *attempt = json_object();
        json_set(attempt, "command", json_string(commands[ci]));
        json_set(attempt, "dump_returncode", json_number((double)dump_rc));
        json_set(attempt, "load_returncode", json_number((double)load_rc));
        size_t etail = strlen(errbuf);
        size_t tail_off = etail > 2000 ? etail - 2000 : 0;
        json_set(attempt, "load_stderr_tail", json_string(errbuf + tail_off));
        int usable = laf_lost_and_found_db_usable(lf_path);
        json_set(attempt, "usable", json_bool(usable));
        json_append(attempts, attempt);
        if (usable) {
            json_set(report, "attempts", attempts);
            char *out = json_dumps(report, 0);
            json_free(report);
            return out;
        }
    }
    /* neither attempt produced a usable DB → LostAndFoundError shape */
    json_free(report);   /* keeps binary; rebuild error payload */
    json_t *err = json_object();
    json_set(err, "error", json_string(
        "sqlite3 .recover did not produce a usable lost_and_found database"));
    json_set(err, "attempts", attempts);
    char *out = json_dumps(err, 0);
    json_free(err);
    return out;
}

/* PoP: laf_map_lost_and_found_rows @ hermes_cli/session_lost_and_found.py:map_lost_and_found_rows */
/*
 *  Best-effort mapping of a .recover output DB into a fresh SessionDB.
 * Returns a JSON report object (caller frees). */
json_t *laf_map_lost_and_found_rows(sqlite3 *lf_conn, sqlite3 *dest)
{
    json_t *report = json_object();
    json_t *mapped = json_object();
    json_set(mapped, "sessions", json_number(0.0));
    json_set(mapped, "messages", json_number(0.0));
    json_set(mapped, "session_model_usage", json_number(0.0));
    json_set(report, "direct_table_rows", json_object());
    json_set(report, "mapped", mapped);
    json_set(report, "legacy_minimal_sessions", json_number(0.0));
    json_set(report, "unmapped_rows", json_number(0.0));
    json_set(report, "insert_conflicts", json_number(0.0));
    json_set(report, "lost_and_found_tables", json_array());

    if (!lf_conn || !dest) return report;

    sqlite3_exec(dest, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    json_t *direct = laf_copy_direct_tables(lf_conn, dest);
    if (direct) {
        json_t *old = (json_t *)json_obj_get(report, "direct_table_rows");
        (void)old;
        json_obj_del(report, "direct_table_rows");
        json_set(report, "direct_table_rows", direct);
    }

    int sessions_n = 0, messages_n = 0, usage_n = 0;
    char **sessions_columns = laf_table_columns(dest, "sessions", &sessions_n);
    char **messages_columns = laf_table_columns(dest, "messages", &messages_n);
    char **usage_columns = laf_table_columns(dest, "session_model_usage", &usage_n);
    json_t *sessions_defaults = laf_notnull_defaults(dest, "sessions");
    json_t *messages_defaults = laf_notnull_defaults(dest, "messages");
    json_t *usage_defaults = laf_notnull_defaults(dest, "session_model_usage");
    /* Never fabricate identity fields (Python pops protected indexes). */
    for (int i = 0; i < 2; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%d", i == 0 ? 0 : 1);
        json_obj_del(sessions_defaults, key);
        snprintf(key, sizeof(key), "%d", i == 0 ? 1 : 2);
        json_obj_del(messages_defaults, key);
        snprintf(key, sizeof(key), "%d", i == 0 ? 0 : 1);
        json_obj_del(usage_defaults, key);
    }

    /* list lost_and_found% tables */
    json_t *lf_tables = json_array();
    sqlite3_stmt *tbl = NULL;
    if (sqlite3_prepare_v2(lf_conn,
            "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'lost_and_found%'",
            -1, &tbl, NULL) == SQLITE_OK) {
        while (sqlite3_step(tbl) == SQLITE_ROW) {
            const unsigned char *name = sqlite3_column_text(tbl, 0);
            json_append(lf_tables, json_string(name ? (const char *)name : ""));
        }
        sqlite3_finalize(tbl);
    }
    json_obj_del(report, "lost_and_found_tables");
    json_set(report, "lost_and_found_tables", lf_tables);

    for (size_t ti = 0; ti < json_len(lf_tables); ti++) {
        const json_t *tname_v = json_get(lf_tables, ti);
        if (!tname_v || tname_v->type != JSON_STRING) continue;
        const char *lf_table = tname_v->str_val;
        int lf_ncols = 0;
        char **lf_cols = laf_table_columns(lf_conn, lf_table, &lf_ncols);
        if (!lf_cols || lf_ncols < 3 || strcmp(lf_cols[0], "rootpgno") != 0 ||
            strcmp(lf_cols[1], "pgno") != 0 || strcmp(lf_cols[2], "nfield") != 0) {
            laf_free_columns(lf_cols, lf_ncols);
            continue;
        }
        laf_free_columns(lf_cols, lf_ncols);

        char sel[512];
        snprintf(sel, sizeof(sel), "SELECT * FROM \"%s\"", lf_table);
        sqlite3_stmt *rows = NULL;
        if (sqlite3_prepare_v2(lf_conn, sel, -1, &rows, NULL) != SQLITE_OK) continue;
        while (sqlite3_step(rows) == SQLITE_ROW) {
            int ncols_total = sqlite3_column_count(rows);
            int nfield = 0;
            if (ncols_total > 2 && sqlite3_column_type(rows, 2) != SQLITE_NULL)
                nfield = sqlite3_column_int(rows, 2);
            if (nfield <= 0) {
                json_t *m = (json_t *)json_obj_get(report, "unmapped_rows");
                double v = m ? m->num_val : 0;
                json_obj_del(report, "unmapped_rows");
                json_set(report, "unmapped_rows", json_number(v + 1.0));
                continue;
            }
            long long lf_rowid = ncols_total > 3 ? sqlite3_column_int64(rows, 3) : 0;
            /* classify via cells[4 .. 4+nfield) using the ported classifier */
            int ncells = nfield < (ncols_total - 4) ? nfield : (ncols_total - 4);
            if (ncells < 0) ncells = 0;
            json_t *cells = json_array();
            for (int c = 0; c < ncells; c++) {
                switch (sqlite3_column_type(rows, 4 + c)) {
                case SQLITE_NULL:    json_append(cells, json_null()); break;
                case SQLITE_INTEGER: json_append(cells, json_number((double)sqlite3_column_int64(rows, 4 + c))); break;
                case SQLITE_FLOAT:   json_append(cells, json_number(sqlite3_column_double(rows, 4 + c))); break;
                case SQLITE_TEXT:    json_append(cells, json_string((const char *)sqlite3_column_text(rows, 4 + c))); break;
                default:             json_append(cells, json_null()); break;
                }
            }
            char *cells_ser = json_dumps(cells, 0);
            const char *kind = NULL;
            char *kind_str = cells_ser ? laf_classify_row(nfield, cells_ser) : NULL;
            if (kind_str && strcmp(kind_str, "null") != 0 && strlen(kind_str) >= 3) {
                /* strip the surrounding JSON quotes */
                static __thread char kindbuf[32];
                size_t kl = strlen(kind_str);
                snprintf(kindbuf, sizeof(kindbuf), "%.*s", (int)(kl - 2), kind_str + 1);
                free(kind_str);
                kind = kindbuf;
            } else {
                free(kind_str);
            }
            int inserted = 0;
            if (kind && strcmp(kind, "messages") == 0) {
                json_t *values = json_array();
                json_append(values, json_number((double)lf_rowid));
                int take = nfield < messages_n ? nfield : messages_n;
                for (int c = 1; c < take && (size_t)c < json_len(cells); c++)
                    json_append(values, json_copy(json_get(cells, c)));
                inserted = laf_insert_prefix_row(dest, "messages",
                                                 messages_columns, messages_n,
                                                 values, messages_defaults);
                json_free(values);
            } else if (kind && strcmp(kind, "session_model_usage") == 0) {
                int take = usage_n < (int)json_len(cells) ? usage_n : (int)json_len(cells);
                json_t *values = json_array();
                for (int c = 0; c < take; c++)
                    json_append(values, json_copy(json_get(cells, c)));
                inserted = laf_insert_prefix_row(dest, "session_model_usage",
                                                 usage_columns, usage_n,
                                                 values, usage_defaults);
                json_free(values);
            } else if (kind && nfield == LAF_SESSIONS_LEGACY_MINIMAL_NFIELD) {
                /* legacy minimal layout: identity + timing only */
                const json_t *c0 = json_len(cells) > 0 ? json_get(cells, 0) : NULL;
                const json_t *c1 = json_len(cells) > 1 ? json_get(cells, 1) : NULL;
                const char *sid = (c0 && c0->type == JSON_STRING) ? c0->str_val : "";
                const char *src = (c1 && c1->type == JSON_STRING &&
                                   laf_looks_like_source(c1->str_val)) ? c1->str_val
                                   : "recovered";
                double started = cells_ser ? laf_heuristic_started_at(cells_ser) : 0.0;
                sqlite3_stmt *ins = NULL;
                if (sqlite3_prepare_v2(dest,
                        "INSERT OR IGNORE INTO sessions (id, source, started_at, title) "
                        "VALUES (?, ?, ?, ?)", -1, &ins, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(ins, 1, sid, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ins, 2, src, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_double(ins, 3, started);
                    sqlite3_bind_text(ins, 4,
                        "[best-effort recovered] legacy session row (layout unknown)",
                        -1, SQLITE_TRANSIENT);
                    sqlite3_step(ins);
                    inserted = sqlite3_changes(dest) == 1;
                    sqlite3_finalize(ins);
                }
                if (inserted) {
                    json_t *lm = (json_t *)json_obj_get(report, "legacy_minimal_sessions");
                    double v = lm ? lm->num_val : 0;
                    json_obj_del(report, "legacy_minimal_sessions");
                    json_set(report, "legacy_minimal_sessions", json_number(v + 1.0));
                }
            } else if (kind) {
                int take = nfield < sessions_n ? nfield : sessions_n;
                json_t *values = json_array();
                for (int c = 0; c < take && (size_t)c < json_len(cells); c++)
                    json_append(values, json_copy(json_get(cells, c)));
                inserted = laf_insert_prefix_row(dest, "sessions",
                                                 sessions_columns, sessions_n,
                                                 values, sessions_defaults);
                json_free(values);
            }
            json_free(cells);
            free(cells_ser);
            if (!kind) {
                json_t *m = (json_t *)json_obj_get(report, "unmapped_rows");
                double v = m ? m->num_val : 0;
                json_obj_del(report, "unmapped_rows");
                json_set(report, "unmapped_rows", json_number(v + 1.0));
            } else if (inserted) {
                json_t *m = (json_t *)json_obj_get(report, "mapped");
                const json_t *slot = json_obj_get(m, kind);
                double v = slot ? slot->num_val : 0;
                json_obj_del(m, kind);
                json_set(m, kind, json_number(v + 1.0));
            } else {
                json_t *m = (json_t *)json_obj_get(report, "insert_conflicts");
                double v = m ? m->num_val : 0;
                json_obj_del(report, "insert_conflicts");
                json_set(report, "insert_conflicts", json_number(v + 1.0));
            }
        }
        sqlite3_finalize(rows);
    }

    sqlite3_exec(dest, "COMMIT", NULL, NULL, NULL);

    laf_free_columns(sessions_columns, sessions_n);
    laf_free_columns(messages_columns, messages_n);
    laf_free_columns(usage_columns, usage_n);
    json_free(sessions_defaults);
    json_free(messages_defaults);
    json_free(usage_defaults);
    return report;
}

/* PoP: laf_stub_missing_parent_sessions @ hermes_cli/session_lost_and_found.py:stub_missing_parent_sessions */
/*
 *  Fabricate clearly-marked placeholder parents for salvaged child rows.
 * Returns a JSON report (caller frees). */
static int laf_insert_session_stub(sqlite3 *dest, const char *session_id,
                                   double started_at, long long message_count,
                                   long long *sequence_inout);

json_t *laf_stub_missing_parent_sessions(sqlite3 *dest)
{
    json_t *result = json_object();
    json_set(result, "sessions_stubbed", json_number(0.0));
    json_set(result, "messages_retained", json_number(0.0));
    json_set(result, "usage_rows_retained", json_number(0.0));
    if (!dest) return result;

    sqlite3_exec(dest, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    long long stubbed = 0, retained_msgs = 0;
    sqlite3_stmt *orphans = NULL;
    if (sqlite3_prepare_v2(dest,
            "SELECT m.session_id, MIN(m.timestamp), COUNT(*) FROM messages AS m "
            "WHERE m.session_id IS NOT NULL AND NOT EXISTS "
            "(SELECT 1 FROM sessions WHERE sessions.id = m.session_id) "
            "GROUP BY m.session_id", -1, &orphans, NULL) == SQLITE_OK) {
        while (sqlite3_step(orphans) == SQLITE_ROW) {
            const unsigned char *sid = sqlite3_column_text(orphans, 0);
            double first_ts = sqlite3_column_type(orphans, 1) != SQLITE_NULL
                                  ? sqlite3_column_double(orphans, 1) : 0.0;
            long long count = sqlite3_column_int64(orphans, 2);
            if (laf_insert_session_stub(dest, (const char *)sid, first_ts,
                                        count, &stubbed))
                retained_msgs += count;
        }
        sqlite3_finalize(orphans);
    }
    sqlite3_stmt *uorphans = NULL;
    if (sqlite3_prepare_v2(dest,
            "SELECT DISTINCT u.session_id FROM session_model_usage AS u "
            "WHERE u.session_id IS NOT NULL AND NOT EXISTS "
            "(SELECT 1 FROM sessions WHERE sessions.id = u.session_id)",
            -1, &uorphans, NULL) == SQLITE_OK) {
        while (sqlite3_step(uorphans) == SQLITE_ROW) {
            const unsigned char *sid = sqlite3_column_text(uorphans, 0);
            laf_insert_session_stub(dest, (const char *)sid, 0.0, 0, &stubbed);
        }
        sqlite3_finalize(uorphans);
    }

    json_t *ss = (json_t *)json_obj_get(result, "sessions_stubbed");
    (void)ss;
    json_obj_del(result, "sessions_stubbed");
    json_set(result, "sessions_stubbed", json_number((double)stubbed));
    json_obj_del(result, "messages_retained");
    json_set(result, "messages_retained", json_number((double)retained_msgs));

    sqlite3_stmt *ucount = NULL;
    if (sqlite3_prepare_v2(dest, "SELECT COUNT(*) FROM session_model_usage",
                           -1, &ucount, NULL) == SQLITE_OK) {
        if (sqlite3_step(ucount) == SQLITE_ROW) {
            json_obj_del(result, "usage_rows_retained");
            json_set(result, "usage_rows_retained",
                     json_number((double)sqlite3_column_int64(ucount, 0)));
        }
        sqlite3_finalize(ucount);
    }

    /* repair dangling intra-sessions references without deleting rows */
    sqlite3_exec(dest,
        "UPDATE sessions SET parent_session_id = NULL "
        "WHERE parent_session_id IS NOT NULL AND NOT EXISTS "
        "(SELECT 1 FROM sessions AS p WHERE p.id = sessions.parent_session_id)",
        NULL, NULL, NULL);
    sqlite3_exec(dest,
        "UPDATE sessions SET system_prompt_hash = NULL "
        "WHERE system_prompt_hash IS NOT NULL AND NOT EXISTS "
        "(SELECT 1 FROM system_prompts WHERE system_prompts.hash = sessions.system_prompt_hash)",
        NULL, NULL, NULL);

    sqlite3_exec(dest, "COMMIT", NULL, NULL, NULL);
    return result;
}

/* Helper: insert one uniquely-titled placeholder parent session.
 * Mirrors the Python title-collision loop. Returns 1 when inserted. */
static int laf_insert_session_stub(sqlite3 *dest, const char *session_id,
                                   double started_at, long long message_count,
                                   long long *sequence_inout)
{
    if (!dest || !session_id || !*session_id) return 0;
    long long seq = *sequence_inout + 1;
    char title[160];
    sqlite3_stmt *dup = NULL;
    for (;; seq++) {
        snprintf(title, sizeof(title),
                 "[best-effort recovered %lld] session metadata was unreadable", seq);
        int exists = 0;
        if (sqlite3_prepare_v2(dest, "SELECT 1 FROM sessions WHERE title = ? LIMIT 1",
                               -1, &dup, NULL) == SQLITE_OK) {
            sqlite3_bind_text(dup, 1, title, -1, SQLITE_TRANSIENT);
            exists = (sqlite3_step(dup) == SQLITE_ROW);
            sqlite3_finalize(dup);
            dup = NULL;
        }
        if (!exists) break;
    }
    *sequence_inout = seq;
    sqlite3_stmt *ins = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(dest,
            "INSERT INTO sessions (id, source, started_at, title, message_count) "
            "VALUES (?, 'recovered', ?, ?, ?)", -1, &ins, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ins, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(ins, 2, started_at);
        sqlite3_bind_text(ins, 3, title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(ins, 4, message_count);
        ok = (sqlite3_step(ins) == SQLITE_DONE);
        sqlite3_finalize(ins);
    }
    return ok;
}

/* PoP: laf_rebuild_fts_indexes @ hermes_cli/session_lost_and_found.py:rebuild_fts_indexes */
/*
 *  Rebuild derived FTS indexes from the salvaged canonical rows. Returns a
 * JSON object {table: status} (caller frees). */
json_t *laf_rebuild_fts_indexes(sqlite3 *dest)
{
    static const char *kTables[] = {
        "messages_fts", "messages_fts_trigram", "messages_fts_cjk", NULL };
    json_t *results = json_object();
    if (!dest) return results;
    for (int i = 0; kTables[i]; i++) {
        const char *table = kTables[i];
        int n = 0;
        char **cols = laf_table_columns(dest, table, &n);
        if (!cols || n == 0) { laf_free_columns(cols, n); continue; }
        laf_free_columns(cols, n);
        char sql[256];
        snprintf(sql, sizeof(sql), "INSERT INTO \"%s\"(\"%s\") VALUES ('rebuild')",
                 table, table);
        char *errmsg = NULL;
        if (sqlite3_exec(dest, sql, NULL, NULL, &errmsg) == SQLITE_OK)
            json_set(results, table, json_string("rebuilt"));
        else {
            char status[256];
            snprintf(status, sizeof(status), "rebuild failed: %.180s",
                     errmsg ? errmsg : "unknown error");
            json_set(results, table, json_string(status));
            sqlite3_free(errmsg);
        }
    }
    return results;
}
