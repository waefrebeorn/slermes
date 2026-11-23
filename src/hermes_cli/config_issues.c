/*
 * config_issues.c — C port of upstream Hermes hermes_cli/config_issues.py.
 * PoP: ciss_issue @ hermes_cli/config_issues.py:_issue
 * PoP: ciss_config_issue_t @ hermes_cli/config_issues.py:ConfigIssue
 *
 * _issue(issues, severity, message, hint) appends a ConfigIssue record.
 */
#include "noc_ciss.h"
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

void ciss_issue_list_init(ciss_issue_list_t *list) {
    if (!list) return;
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void ciss_issue(ciss_issue_list_t *issues, const char *severity,
                const char *message, const char *hint) {
    if (!issues) return;
    if (issues->count == issues->cap) {
        size_t nc = issues->cap ? issues->cap * 2 : 4;
        ciss_config_issue_t *ni = (ciss_config_issue_t *)realloc(
            issues->items, nc * sizeof(*ni));
        if (!ni) return; /* allocation failure: leave list unchanged */
        issues->items = ni;
        issues->cap = nc;
    }
    ciss_config_issue_t *it = &issues->items[issues->count++];
    it->severity = dup_str(severity);
    it->message = dup_str(message);
    it->hint = dup_str(hint);
}

void ciss_issue_list_free(ciss_issue_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].severity);
        free(list->items[i].message);
        free(list->items[i].hint);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}
