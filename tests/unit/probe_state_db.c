/* probe_state_db.c -- does the state db open on this box, and with which path? */
#include "hermes_json.h"
#include "hermes_state_db.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

const char *get_hermes_home(void) { return NULL; }
const char *hc_get_hermes_home_override(void) { return NULL; }
char *hs_resolve_journal_mode(void) { return strdup("wal"); }
void hermes_log(int level, const char *module, const char *fmt, ...) { (void)level; (void)module; (void)fmt; }
char *tool_error(const char *message, ...) { (void)message; return NULL; }
char *tool_error_with_class(const char *kind, const char *message) { (void)kind; (void)message; return NULL; }
const char *tool_classify_error(const char *message) { (void)message; return "error"; }
void *tool_idem_new(const char *a, const char *b) { (void)a; (void)b; return NULL; }
void tool_idem_free(void *p) { (void)p; }
const char *tool_idem_key(const char *a) { (void)a; return NULL; }
const char *tool_idem_get(const char *a) { (void)a; return NULL; }
void tool_idem_attach(const char *a, const char *b) { (void)a; (void)b; }
void tool_idem_put(const char *a, const char *b, const char *c) { (void)a; (void)b; (void)c; }
bool tool_result_is_error(const char *a) { (void)a; return false; }

extern void hermes_get_home(char *buf, size_t sz);

int main(void) {
    char home[1024], path[1200];
    hermes_get_home(home, sizeof home);
    printf("home=%s\n", home);
    snprintf(path, sizeof path, "%s/state.db", home);
    printf("path=%s\n", path);
    hermes_state_db_t *db = hermes_state_db_open(path);
    printf("open=%s\n", db ? "ok" : "NULL");
    if (db) {
        char *raw = hermes_state_get_session_model_config_value(db, "probe", "onboarding_run", "null");
        printf("read=%s\n", raw ? raw : "(null)");
        free(raw);
        hermes_state_db_close(db);
    }
    return 0;
}
