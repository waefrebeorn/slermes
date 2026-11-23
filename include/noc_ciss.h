#ifndef NOC_CISS_H
#define NOC_CISS_H
#include <stddef.h>

/* C mirrors of hermes_cli/config_issues.py.
 * PoP: ciss_issue @ hermes_cli/config_issues.py:_issue
 * PoP: ciss_config_issue_t @ hermes_cli/config_issues.py:ConfigIssue
 */
typedef struct {
    char *severity;
    char *message;
    char *hint;
} ciss_config_issue_t;

typedef struct {
    ciss_config_issue_t *items;
    size_t count;
    size_t cap;
} ciss_issue_list_t;

void ciss_issue_list_init(ciss_issue_list_t *list);
void ciss_issue(ciss_issue_list_t *issues, const char *severity,
                const char *message, const char *hint);
void ciss_issue_list_free(ciss_issue_list_t *list);

#endif /* NOC_CISS_H */
